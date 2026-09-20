/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpression extends Object within Material
	native
	abstract
	hidecategories(Object);

struct ExpressionInput
{
	var MaterialExpression	Expression;
	// BM
	var int					OutputIndex;
	var string				InputName;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
	var int					GCC64_Padding; // @todo 64: if the C++ didn't mismirror this structure (with MaterialInput), we might not need this
};

// BM
struct ExpressionOutput
{
	var string				OutputName;
	var int					Mask,
							MaskR,
							MaskG,
							MaskB,
							MaskA;
};

/** This variable is conlficting with Materia var, making new ones (MaterialExpressionEditor), and then deprecating this **/
var deprecated editoronly int	EditorX,
								EditorY;

var editoronly int		MaterialExpressionEditorX,
						MaterialExpressionEditorY;

/** Set to TRUE by RecursiveUpdateRealtimePreview() if the expression's preview needs to be updated in realtime in the material editor. */
var bool					bRealtimePreview;

/** If TRUE, we should update the preview next render. This is set when changing bRealtimePreview. */
var transient bool			bNeedToUpdatePreview;

/** Indicates that this is a 'parameter' type of expression and should always be loaded (ie not cooked away) because we might want the default parameter. */
var bool					bIsParameterExpression;

/** If TRUE, the expression is drawn collapsed in the material editor. */
var bool bCollapsedInEditor;

/** If TRUE, use the output name as the label for the pin */
var bool bShowOutputNameOnPin;
/** If TRUE, do not render the preview window for the expression */
var bool bHidePreviewWindow;

/** If TRUE, the expression's input pins are drawn. */
var bool bShowInputs;
/** If TRUE, the expression's output pins are drawn. */
var bool bShowOutputs;

/**
 *	If TRUE, this expression is used when generating the StaticParameterSet.
 *	It is important to set this correctly if the cooker is using the CleanupMaterials functionality.
 *	If it is not set correctly, the cleanup code will remove the expression and the StaticParameterSet
 *	will mismatch when verifying the shader map.
 *	The ClearInputExpression function should also be implement on expressions that set this as it
 *	will be called by the CleanupMaterials function to remove unrequired expressions.
 */
var bool bUsedByStaticParameterSet;

/** The material this expression belongs to. */
var const Material			Material;

/** The material function this expression belongs to. */
var const MaterialFunction	Function;

/** A description that level designers can add (shows in the material editor UI). */
var() editoronly string		Desc;

/** Colour of the expression's border in the material editor. */
var color					BorderColor;

/** Categories to sort this expression into... */
var editoronly array<name>	MenuCategories;

/** The expression's outputs. */
var array<ExpressionOutput>	Outputs;

cpptext
{
	// UObject interface.

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	// UMaterialExpression interface.

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL) {}

	virtual INT Compile(FMaterialCompiler* Compiler) { return INDEX_NONE; }
	virtual INT CompilePreview(FMaterialCompiler* Compiler) { return Compile(Compiler); }
	virtual void GetOutputs(TArray<FExpressionOutput>& Outputs) const;
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(INT InputIndex);
	virtual FString GetInputName(INT InputIndex) const;
	virtual INT GetWidth() const;
	virtual INT GetHeight() const;
	virtual UBOOL UsesLeftGutter() const;
	virtual UBOOL UsesRightGutter() const;
	virtual FString GetCaption() const;
	virtual int GetLabelPadding() { return 0; }

	virtual INT CompilerError(FMaterialCompiler* Compiler, const TCHAR* pcMessage);

	virtual void Serialize(FArchive& Ar);

	/**
	 * @return TRUE if the expression preview needs realtime update
     */
	virtual UBOOL NeedsRealtimePreview() { return FALSE; }

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );

#if WITH_EDITOR
	/**
	 *	Called by the CleanupMaterials function, this will clear the inputs of the expression.
	 *	This only needs to be implemented by expressions that have bUsedByStaticParameterSet set to TRUE.
	 */
	virtual void ClearInputExpressions() {}
#endif

	/**
	 * Copies the SrcExpressions into the specified material.  Preserves internal references.
	 * New material expressions are created within the specified material.
	 */
	static void CopyMaterialExpressions(const TArray<class UMaterialExpression*>& SrcExpressions, const TArray<class UMaterialExpressionComment*>& SrcExpressionComments, 
										class UMaterial* Material, TArray<class UMaterialExpression*>& OutNewExpressions, TArray<class UMaterialExpression*>& OutNewComments);
}

defaultproperties
{
	bShowInputs=TRUE
	bShowOutputs=TRUE
	Outputs(0)=(OutputName="")
}
