/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionVectorParameter extends MaterialExpressionParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

var() LinearColor	DefaultValue;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler);
	virtual void GetOutputs(TArray<FExpressionOutput>& Outputs) const;
	virtual FString GetCaption() const;
}

defaultproperties
{
	DefaultValue=(R=0.0,G=0.0,B=0.0,A=1.0)
	MenuCategories(0)="Parameters"
}
