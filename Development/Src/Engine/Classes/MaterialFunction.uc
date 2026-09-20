/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * A reusable material expression graph, referenced by Material.MaterialFunctionInfos.
 */
// BM
class MaterialFunction extends Object;

var duplicatetransient guid StateId;

var editoronly transient MaterialFunction ParentFunction;

var() string Description;

var() bool bExposeToLibrary;

var const transient bool bReentrantFlag;

var() array<string> LibraryCategories;

var array<MaterialExpression> FunctionExpressions;

var editoronly array<MaterialExpressionComment> FunctionEditorComments;

defaultproperties
{
	LibraryCategories(0)="Misc"
}
