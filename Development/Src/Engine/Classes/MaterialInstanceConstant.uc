/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class MaterialInstanceConstant extends MaterialInstance
	native(Material)
	collapsecategories;


struct native FontParameterValue
{
	var() name		ParameterName;
	var() Font		FontValue;
	var() int		FontPage;
	var	  guid		ExpressionGUID;
};

struct native ScalarParameterValue
{
	var() name	ParameterName;
	var() float	ParameterValue;
	var	  guid	ExpressionGUID;
};

struct native TextureParameterValue
{
	var() name		ParameterName;
	var() Texture	ParameterValue;
	var	  guid		ExpressionGUID;
};

struct native VectorParameterValue
{
	var() name			ParameterName;
	var() LinearColor	ParameterValue;
	var	  guid			ExpressionGUID;
};



var() const array<FontParameterValue>		FontParameterValues;
var() const array<ScalarParameterValue>		ScalarParameterValues;
var() const array<TextureParameterValue>	TextureParameterValues;
var() const array<VectorParameterValue>		VectorParameterValues;


cpptext
{
	// Constructor.
	UMaterialInstanceConstant();

	// UMaterialInstance interface.
	virtual void InitResources();

	virtual void CheckStaticParameterValues(FStaticParameterSet* EditorParameters);

	// UMaterialInterface interface.
	virtual UBOOL GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, INT& OutFontPage);
	virtual UBOOL GetScalarParameterValue(FName ParameterName,FLOAT& OutValue);
	virtual UBOOL GetTextureParameterValue(FName ParameterName,class UTexture*& OutValue);
	virtual UBOOL GetVectorParameterValue(FName ParameterName,FLinearColor& OutValue);

	// UObject interface.
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	virtual void UpdateParameterNames();

	virtual void CleanupTextureParameterReferences(const TMap<FName,UTexture*>& InRefdTextureParamsMap);
};

// SetParent - Updates the parent.

native function SetParent(MaterialInterface NewParent);

// Set*ParameterValue - Updates the entry in ParameterValues for the named parameter, or adds a new entry.

native function SetScalarParameterValue(name ParameterName, float Value);
native function SetTextureParameterValue(name ParameterName, Texture Value);
native function SetVectorParameterValue(name ParameterName, const out LinearColor Value);

native function SetFontParameterValue(name ParameterName, Font FontValue, int FontPage);


/** Removes all parameter values */
native function ClearParameterValues();


defaultproperties
{

}
