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
#if BATMAN
	// BM2 cooked tag carries a 16-bit offset of the property within its parent struct.
	// Only valid when read/written from a BM2 cooked archive.
	WORD	PropertyOffset;
#endif

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
#if BATMAN
	,	PropertyOffset(Property ? (WORD)Property->Offset : 0)
#endif
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

#if BATMAN
	// BM2 cooked tag: when the Type is one of these "simple" intrinsic property
	// types, the tag has no Name/Size/ArrayIndex and the value is read directly
	// at obj+PropertyOffset. Mirrors BmGame.exe.c sub_5FC160.
	static FORCEINLINE UBOOL IsBmSimpleType(INT TypeIndex)
	{
		return TypeIndex == NAME_IntProperty
			|| TypeIndex == NAME_FloatProperty
			|| TypeIndex == NAME_NameProperty
			|| TypeIndex == NAME_VectorProperty
			|| TypeIndex == NAME_RotatorProperty
			|| TypeIndex == NAME_StrProperty
			|| TypeIndex == NAME_ObjectNCRProperty;
	}
#endif

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPropertyTag& Tag )
	{
#if BATMAN
		// BM2 cooked property tag (FCookedPropertyTag in the original game):
		//   INT16 Type                            (0 = end-of-properties marker)
		//   INT16 PropertyOffset
		//   For non-simple types:
		//     FName Name
		//     INT32 Size
		//     INT32 ArrayIndex
		//   If Type == BoolProperty: BYTE BoolVal
		if (Ar.IsBmCooked(TRUE, FALSE))
		{
			if (Ar.IsLoading())
			{
				SWORD TypeIndex = 0;
				Ar << TypeIndex;

				if (TypeIndex == 0)
				{
					Tag.Type = NAME_None;
					Tag.Name = NAME_None;
					Tag.PropertyOffset = 0;
					return Ar;
				}

				if (TypeIndex < 0 || TypeIndex > NAME_GUIDProperty)
				{
					warnf(NAME_Warning, TEXT("FPropertyTag: TypeIndex %d out of range, treating as end of properties"), (INT)TypeIndex);
					Tag.Type = NAME_None;
					Tag.Name = NAME_None;
					Tag.PropertyOffset = 0;
					return Ar;
				}

				Tag.Type = FName((EName)TypeIndex);

				WORD OffsetWord = 0;
				Ar << OffsetWord;
				Tag.PropertyOffset = OffsetWord;

				if (IsBmSimpleType(TypeIndex))
				{
					// Simple intrinsic: data follows directly, no name/size/array index.
					Tag.Name = NAME_None;
					Tag.Size = 0;
					Tag.ArrayIndex = 0;
				}
				else
				{
					Ar << Tag.Name;
					Ar << Tag.Size << Tag.ArrayIndex;
				}

				if (TypeIndex == NAME_BoolProperty)
				{
					Ar << Tag.BoolVal;
				}

				// Recovered from property defs later when needed.
				Tag.StructName = NAME_None;
				Tag.EnumName = NAME_None;
			}
			else // Saving
			{
				if (Tag.Type == NAME_None)
				{
					// End-of-properties marker.
					SWORD EndMarker = 0;
					Ar << EndMarker;
					return Ar;
				}

				SWORD TypeIndex = (SWORD)Tag.Type.GetIndex();
				Ar << TypeIndex;

				WORD OffsetWord = Tag.PropertyOffset;
				Ar << OffsetWord;

				if (IsBmSimpleType(TypeIndex))
				{
					// No tag fields beyond Type+Offset; no size fixup needed.
					Tag.SizeOffset = INDEX_NONE;
				}
				else
				{
					Ar << Tag.Name;
					Tag.SizeOffset = Ar.Tell();
					Ar << Tag.Size << Tag.ArrayIndex;
				}

				if (TypeIndex == NAME_BoolProperty)
				{
					Ar << Tag.BoolVal;
				}
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

