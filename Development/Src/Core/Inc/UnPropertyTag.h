/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
/*-----------------------------------------------------------------------------
	FPropertyTag.
-----------------------------------------------------------------------------*/

/**
 *  A tag describing a class property, to aid in serialization.
 */
struct FPropertyTag
{
	// Variables.
	FName	Type;		// Type of property
	BYTE	BoolVal;	// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;		// Name of property.
	FName	StructName;	// Struct name if UStructProperty.
	FName	EnumName;	// Enum name if UByteProperty
	INT		Size;       // Property size.
	INT		ArrayIndex;	// Index if an array; else 0.
	INT		SizeOffset;	// location in stream of tag size member

	// Constructors.
	FPropertyTag()
	{}
	FPropertyTag( FArchive& InSaveAr, UProperty* Property, INT InIndex, BYTE* Value, BYTE* Defaults )
	:	Type		(Property->GetID())
	,	Name		(Property->GetFName())
	,	StructName	(NAME_None)
	,	EnumName	(NAME_None)
	,	Size		(0)
	,	ArrayIndex	(InIndex)
	,	SizeOffset	(INDEX_NONE)
	{
		// Handle structs.
		UStructProperty* StructProperty = Cast<UStructProperty>(Property, CLASS_IsAUStructProperty);
		if (StructProperty != NULL)
		{
			StructName = StructProperty->Struct->GetFName();
		}
		else
		{
			UByteProperty* ByteProp = ExactCast<UByteProperty>(Property);
			if (ByteProp != NULL && ByteProp->Enum != NULL)
			{
				EnumName = ByteProp->Enum->GetFName();
			}
		}

		UBoolProperty* Bool = Cast<UBoolProperty>(Property, CLASS_IsAUBoolProperty);
		BoolVal = (Bool && (*(BITFIELD*)Value & Bool->BitMask)) ? TRUE : FALSE;
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
	{
		// Name.
		Ar << Tag.Name;
		if( Tag.Name == NAME_None )
		{
			return Ar;
		}

		Ar << Tag.Type;
		if ( Ar.IsSaving() )
		{
			// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
			// property has been serialized.
			Tag.SizeOffset = Ar.Tell();
		}
		Ar << Tag.Size << Tag.ArrayIndex;

		// only need to serialize this for structs
		if (Tag.Type == NAME_StructProperty)
		{
			Ar << Tag.StructName;
		}
		// only need to serialize this for bools
		else if (Tag.Type == NAME_BoolProperty)
		{
			if (Ar.Ver() < VER_PROPERTYTAG_BOOL_OPTIMIZATION)
			{
				UBOOL Value = 0;
				Ar << Value;
				Tag.BoolVal = BYTE(Value);
			}
			else
			{
				Ar << Tag.BoolVal;
			}
		}
		// only need to serialize this for bytes
		else if (Tag.Type == NAME_ByteProperty && Ar.Ver() >= VER_BYTEPROP_SERIALIZE_ENUM)
		{
			Ar << Tag.EnumName;
		}

		return Ar;
	}

	// Property serializer.
	void SerializeTaggedProperty( FArchive& Ar, UProperty* Property, BYTE* Value, INT MaxReadBytes, BYTE* Defaults )
	{
		if (Property->GetClass() == UBoolProperty::StaticClass())
		{
			UBoolProperty* Bool = (UBoolProperty*)Property;
			check(Bool->BitMask!=0);
			if (Ar.IsLoading())
			{
				if (BoolVal)
				{
					*(BITFIELD*)Value |=  Bool->BitMask;
				}
				else
				{
					*(BITFIELD*)Value &= ~Bool->BitMask;
				}
			}
		}
		else
		{
			UProperty* OldSerializedProperty = GSerializedProperty;
			GSerializedProperty = Property;

			Property->SerializeItem( Ar, Value, MaxReadBytes, Defaults );

			GSerializedProperty = OldSerializedProperty;
		}
	}
};

#if BATMAN
struct FPropertyTagBat2
{
	// Variables.
	SHORT	Type;		// Type of property
	SHORT	Offset;
	BYTE	BoolVal;	// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;		// Name of property.
	INT		Size;       // Property size.
	INT		ArrayIndex;	// Index if an array; else 0.

	// Constructors.
	FPropertyTagBat2()
	{
	}

	// Serializer.
	friend FArchive& operator<<(FArchive& Ar, FPropertyTagBat2& Tag)
	{
		Ar << Tag.Type;
		if (!Tag.Type)
		{
			Tag.Name = NAME_None;
			return Ar;
		}

		check(Tag.Type <= 16);

		Ar << Tag.Offset;

		if (Tag.Type == NAME_IntProperty ||
			Tag.Type == NAME_FloatProperty ||
			Tag.Type == NAME_NameProperty ||
			Tag.Type == NAME_VectorProperty ||
			Tag.Type == NAME_RotatorProperty ||
			Tag.Type == NAME_StrProperty ||
			Tag.Type == 16)
		{
			if (Tag.Type == NAME_VectorProperty || Tag.Type == NAME_RotatorProperty)
			{
				Tag.Size = sizeof(FVector);
			}
			else if (Tag.Type == NAME_IntProperty || Tag.Type == NAME_FloatProperty || Tag.Type == NAME_StructProperty)
			{
				Tag.Size = sizeof(INT);
			}
			else if (Tag.Type == NAME_NameProperty)
			{
				Tag.Size = sizeof(INT);
			}
			else if (Tag.Type == 16)
			{
				Tag.Size = sizeof(INT);
			}
			/*else if (Tag.Type == NAME_StrProperty)
			{
				// TODO
			}*/
			else
			{
				warnf(TEXT("Unknown size for property type %d"), Tag.Type);
				Tag.Size = 0;
			}

			Tag.Name = NAME_None;
			Tag.ArrayIndex = 0;
			return Ar;
		}

		Ar << Tag.Name << Tag.Size << Tag.ArrayIndex;

		if (Tag.Type == NAME_BoolProperty)
		{
			Ar << Tag.BoolVal;
		}

		return Ar;
	}

#define HARDCODE_BEGIN(structname) if (Struct->GetName() == FString(TEXT(structname))) {
#define HARDCODE_NAME(name, offset) if (Offset == offset) { return FName(TEXT(name)); }
#define HARDCODE_END() }

	FName GetHardcodedName(UStruct* Struct) const
	{
		HARDCODE_BEGIN("Texture2D")
			HARDCODE_NAME("SizeX", 0xE0);
			HARDCODE_NAME("SizeY", 0xE4);
			HARDCODE_NAME("OriginalSizeX", 0xE8);
			HARDCODE_NAME("OriginalSizeY", 0xEC);
			HARDCODE_NAME("TextureFileCacheName", 0x104);
		HARDCODE_END()

		HARDCODE_BEGIN("Material")
			HARDCODE_NAME("OpacityMaskClipValue", 0x1AB);
		HARDCODE_END()

		HARDCODE_BEGIN("MaterialInstance")
			HARDCODE_NAME("Parent", 0x60);
		HARDCODE_END()

		HARDCODE_BEGIN("ScalarParameterValue")
			HARDCODE_NAME("ParameterName", 0x00);
			HARDCODE_NAME("ParameterValue", 0x08);
		HARDCODE_END()

		HARDCODE_BEGIN("TextureParameterValue")
			HARDCODE_NAME("ParameterName", 0x00);
			HARDCODE_NAME("ParameterValue", 0x08);
		HARDCODE_END()

		HARDCODE_BEGIN("VectorParameterValue")
			HARDCODE_NAME("ParameterName", 0x00);
			HARDCODE_NAME("ParameterValue", 0x08);
		HARDCODE_END()

		HARDCODE_BEGIN("AnimSequence")
			HARDCODE_NAME("SequenceName", 0x2C);
			HARDCODE_NAME("SequenceLength", 0x44);
			HARDCODE_NAME("NumFrames", 0x48);
			HARDCODE_NAME("RateScale", 0x4C);
		HARDCODE_END()

		HARDCODE_BEGIN("SkeletalMeshLODInfo")
			HARDCODE_NAME("DisplayFactor", 0x00);
			HARDCODE_NAME("LODHysteresis", 0x04);
		HARDCODE_END()

		HARDCODE_BEGIN("SkeletalMeshSocket")
			HARDCODE_NAME("DisplayFactor", 0x2C);
			HARDCODE_NAME("BoneName", 0x34);
			HARDCODE_NAME("RelativeLocation", 0x3C);
			HARDCODE_NAME("RelativeRotation", 0x48);
			HARDCODE_NAME("RelativeScale", 0x54);
		HARDCODE_END()

		return FName(*FString::Printf(TEXT("Prop%d"), Offset));
	}
};
#endif

