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
#if BATMAN
		// Batman3 SP (807.138) uses different property tag format:
		// INT16 Type, FName Name, INT Size, INT ArrayIndex, [BYTE BoolVal]
		if (Ar.IsBmCooked())
		{
			if (Ar.IsLoading())
			{
				// Read INT16 Type first (0 = end marker)
				SWORD TypeIndex = 0;
				Ar << TypeIndex;

				if (TypeIndex == 0)
				{
					Tag.Name = NAME_None;
					Tag.Type = NAME_None;
					return Ar;
				}

				// Convert int16 type index to FName (indices match NAME_ enum)
				Tag.Type = FName((EName)TypeIndex);

				// Read property name, size, array index
				Ar << Tag.Name;
				Ar << Tag.Size << Tag.ArrayIndex;

				// Bool properties store value in tag
				if (TypeIndex == NAME_BoolProperty)
				{
					Ar << Tag.BoolVal;
				}

				// Initialize to NAME_None - recovered from property defs later
				Tag.StructName = NAME_None;
				Tag.EnumName = NAME_None;
			}
			else // Saving
			{
				if (Tag.Name == NAME_None)
				{
					// Write end marker
					SWORD EndMarker = 0;
					Ar << EndMarker;
					return Ar;
				}

				// Write INT16 Type
				SWORD TypeIndex = (SWORD)Tag.Type.GetIndex();
				Ar << TypeIndex;

				// Write property name
				Ar << Tag.Name;

				// Remember size offset for later update
				Tag.SizeOffset = Ar.Tell();

				// Write size and array index
				Ar << Tag.Size << Tag.ArrayIndex;

				// Bool properties store value in tag
				if (TypeIndex == NAME_BoolProperty)
				{
					Ar << Tag.BoolVal;
				}
				// Note: StructName and EnumName are NOT serialized in Batman3 format
			}
			return Ar;
		}
#endif

		// Standard UE3 format
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

