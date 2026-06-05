/*=============================================================================
	ImageReflectionRendering.cpp: Implementation for rendering image based reflections.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "EngineMeshClasses.h"
#include "ImageUtils.h"
#include "SceneFilterRendering.h"
#include "ScreenRendering.h"

IMPLEMENT_CLASS(AImageReflection);
IMPLEMENT_CLASS(AImageReflectionSceneCapture);
IMPLEMENT_CLASS(UDEPRECATED_ImageReflectionComponent);
IMPLEMENT_CLASS(UImageBasedReflectionComponent);
IMPLEMENT_CLASS(AImageReflectionShadowPlane);
IMPLEMENT_CLASS(UImageReflectionShadowPlaneComponent);


void AImageReflection::PostLoad()
{
	Super::PostLoad();
	if (ReflectionComponent_DEPRECATED)
	{
		ImageReflectionComponent->ReflectionTexture = ReflectionComponent_DEPRECATED->ReflectionTexture;
	}
}

void AImageReflectionSceneCapture::PostDuplicate()
{
	Super::PostDuplicate();
	// Don't copy the generated texture reference
	ImageReflectionComponent->ReflectionTexture = NULL;
}

// World size of the EditorMeshes.TexPropPlane that is used for previewing
const FLOAT PlaneSize = 321.0f;

/** Renders the scene to a texture for a AImageReflectionSceneCapture. */
void GenerateImageReflectionTexture(AImageReflectionSceneCapture* Reflection, UTextureRenderTarget2D* TempRenderTarget)
{
	FSceneViewFamilyContext ViewFamily(
		TempRenderTarget->GameThread_GetRenderTargetResource(),
		GWorld->Scene,
		// Dynamic shadows currently don't work in ortho projections
		((SHOW_DefaultGame & ~SHOW_ViewMode_Mask) | SHOW_ViewMode_Lit) & ~(SHOW_PostProcess | SHOW_DynamicShadows | SHOW_LOD | SHOW_Fog | SHOW_Selection),
		GCurrentTime - GStartTime,
		GDeltaTime,
		GCurrentTime - GStartTime);

	ViewFamily.bClearScene = TRUE;

	FVector4 OverrideLODViewOrigin(0, 0, 0, 1.0f);

	// Transform positions into the local space of the mesh
	FMatrix ViewMatrix = Reflection->WorldToLocal();

	// Orient the view to look straight at the preview plane's front face
	ViewMatrix = ViewMatrix * FMatrix(
		FPlane(0,	0,	-1,	0),
		FPlane(-1,	0,	0,	0),
		FPlane(0,	1,	0,	0),
		FPlane(0,	0,	0,	1));

	// Remove the scaling along the depth of the image reflection that is included in LocalToWorld so that we can specify a world space depth range
	const FLOAT EffectiveZRange = Reflection->DepthRange / (Reflection->DrawScale * Reflection->DrawScale3D.X);

	const FMatrix ProjectionMatrix = FOrthoMatrix(
		PlaneSize / 2.0f,
		PlaneSize / 2.0f,
		0.5f / EffectiveZRange,
		EffectiveZRange
		);

	TSet<UPrimitiveComponent*> HiddenPrimitives;
	FSceneView* NewView = new FSceneView(
		&ViewFamily,
		NULL,
		-1,
		NULL,
		NULL,
		NULL,
		GEngine->GetWorldPostProcessChain(),
		NULL,
		NULL,
		0,
		0,
		(FLOAT)TempRenderTarget->SizeX,
		(FLOAT)TempRenderTarget->SizeY,
		ViewMatrix,
		ProjectionMatrix,
		FLinearColor::Black,
		FLinearColor(0,0,0,0),
		FLinearColor::White,
		HiddenPrimitives,
		FRenderingPerformanceOverrides(E_ForceInit),
		1.0f,
		TRUE,					// Treat this as a fresh frame, ignore any potential historical data
		FTemporalAAParameters()
#if !CONSOLE
		,1
		,OverrideLODViewOrigin // we need to override this since we are using an ortho view, the normal checks in SceneRendering will fail
#endif
		);

	// Disable specular, otherwise we would be capturing image based reflections from the previous lighting rebuild
	NewView->SpecularOverrideParameter = FVector4(0,0,0,0);

	ViewFamily.Views.Empty();
	ViewFamily.Views.AddItem(NewView);

	// Render the scene
	FCanvas Canvas(TempRenderTarget->GameThread_GetRenderTargetResource(), NULL);
	BeginRenderingViewFamily(&Canvas, &ViewFamily);

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FResolveCommand,
		FTextureRenderTargetResource*, RTResource, TempRenderTarget->GameThread_GetRenderTargetResource(),
	{
		// copy the results of the scene rendering from the target surface to its texture
		RHICopyToResolveTarget(RTResource->GetRenderTargetSurface(), FALSE, FResolveParams());
	});

	// Wait until rendering is complete before accessing the render target
	FlushRenderingCommands();

	FRenderTarget* RenderTarget = TempRenderTarget->GameThread_GetRenderTargetResource();

	TArray<FFloat16Color> OutputBuffer;
	// Read the data back to the CPU
	verify(RenderTarget->ReadFloat16Pixels(OutputBuffer));

	// Create a new texture
	UTexture2D* FinalTexture = CastChecked<UTexture2D>(UObject::StaticConstructObject(UTexture2D::StaticClass(), Reflection->GetOutermost(), Reflection->GetFName(), 0));
	FinalTexture->Init(TempRenderTarget->SizeX, TempRenderTarget->SizeY, PF_A8R8G8B8);

	// Create base mip for the texture we created.
	FColor* MipData = (FColor*)FinalTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
	for (INT y = 0; y < TempRenderTarget->SizeY; y++)
	{
		FColor* DestPtr = &MipData[(TempRenderTarget->SizeY - 1 - y) * TempRenderTarget->SizeX];
		FFloat16Color* SrcPtr = &OutputBuffer((TempRenderTarget->SizeY - 1 - y) * TempRenderTarget->SizeX);
		for (INT x = 0; x < TempRenderTarget->SizeX; x++)
		{
			FLinearColor CurrentColor = FLinearColor(*SrcPtr);
			// Scale down by ColorRange before gamma correction and quantizing so that we can store colors outside the [0, 1] range (at the cost of decreased color precision)
			CurrentColor.R /= Reflection->ColorRange;
			CurrentColor.G /= Reflection->ColorRange;
			CurrentColor.B /= Reflection->ColorRange;
			//@todo - mask based on depth
			CurrentColor.A = CurrentColor.GetLuminance() > .001f ? 1.0f : 0.0f;
			*DestPtr = CurrentColor.ToFColor(TRUE);
			DestPtr++;
			SrcPtr++;
		}
	}
	FinalTexture->Mips(0).Data.Unlock();

	// Set compression options.
	FinalTexture->SRGB = TRUE;
	FinalTexture->CompressionSettings = TC_Default;
	FinalTexture->DeferCompression= FALSE;
	FinalTexture->LODGroup = TEXTUREGROUP_ImageBasedReflection;

	FinalTexture->PostEditChange();

	// Update the image reflection with the newly generated texture
	Reflection->ImageReflectionComponent->ReflectionTexture = FinalTexture;

	Reflection->MarkPackageDirty();
	Reflection->ForceUpdateComponents(FALSE, FALSE);
}

/** A scene proxy used to preview the image reflection actor.  The actual reflection is not rendered through this proxy. */
class FImageReflectionPreviewSceneProxy : public FStaticMeshSceneProxy
{
public:

	/** Constructor - called from the game thread, initializes the proxy's copies of the component's data */
	FImageReflectionPreviewSceneProxy(const UImageBasedReflectionComponent* Component) :
		FStaticMeshSceneProxy(Component),
		ReflectionTexture(Component->ReflectionTexture),
		ReflectionColor(Component->ReflectionColor * Component->ReflectionColor.A)
	{
		check(ReflectionTexture);
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(Component->GetOwner());
		bDrawDepthRange = CaptureOwner != NULL;
		if (CaptureOwner)
		{
			// Remove the scaling inherent in the LocalToWorld transform so artists can provide DepthRange in world space
			DepthRange = CaptureOwner->DepthRange / (CaptureOwner->DrawScale * CaptureOwner->DrawScale3D.X);
			ReflectionColor *= CaptureOwner->ColorRange;
		}
		else
		{
			DepthRange = 0;
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Relevance = FStaticMeshSceneProxy::GetViewRelevance(View);
		if (bDrawDepthRange)
		{
			// Force DrawDynamicElements to always be called so we can draw the depth range
			Relevance.bDynamicRelevance = TRUE;
			Relevance.bStaticRelevance = FALSE;
		}
		return Relevance;
	}


	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		FStaticMeshSceneProxy::DrawDynamicElements(PDI, View, DPGIndex, Flags);

		// If selected, draw a wireframe box visualizing the depth range
		if (bDrawDepthRange 
			&& (View->Family->ShowFlags & SHOW_StaticMeshes)
			&& GetDepthPriorityGroup(View) == DPGIndex
			&& AllowDebugViewmodes()
			&& IsSelected())
		{
			const FVector Extent = FVector(DepthRange, PlaneSize / 2.0f, PlaneSize / 2.0f);
			const FVector Corner0 = LocalToWorld.TransformFVector(FVector(-Extent.X, -Extent.Y, -Extent.Z));
			const FVector Corner1 = LocalToWorld.TransformFVector(FVector(Extent.X, -Extent.Y, -Extent.Z));
			const FVector Corner2 = LocalToWorld.TransformFVector(FVector(-Extent.X, Extent.Y, -Extent.Z));
			const FVector Corner3 = LocalToWorld.TransformFVector(FVector(Extent.X, Extent.Y, -Extent.Z));
			const FVector Corner4 = LocalToWorld.TransformFVector(FVector(-Extent.X, -Extent.Y, Extent.Z));
			const FVector Corner5 = LocalToWorld.TransformFVector(FVector(Extent.X, -Extent.Y, Extent.Z));
			const FVector Corner6 = LocalToWorld.TransformFVector(FVector(-Extent.X, Extent.Y, Extent.Z));
			const FVector Corner7 = LocalToWorld.TransformFVector(FVector(Extent.X, Extent.Y, Extent.Z));

			PDI->DrawLine(Corner0, Corner1, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner1, Corner3, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner3, Corner2, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner2, Corner0, WireframeColor, DPGIndex);

			PDI->DrawLine(Corner4, Corner5, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner5, Corner7, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner7, Corner6, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner6, Corner4, WireframeColor, DPGIndex);

			PDI->DrawLine(Corner0, Corner4, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner1, Corner5, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner2, Corner6, WireframeColor, DPGIndex);
			PDI->DrawLine(Corner3, Corner7, WireframeColor, DPGIndex);
		}
	}


	virtual UBOOL GetMeshElement(INT LODIndex,INT ElementIndex,INT FragmentIndex,BYTE InDepthPriorityGroup,const FMatrix& WorldToLocal,FMeshElement& OutMeshElement, const UBOOL bUseSelectedMaterial, const UBOOL bUseHoveredMaterial) const
	{
		const UBOOL bShouldRender = FStaticMeshSceneProxy::GetMeshElement(LODIndex, ElementIndex, FragmentIndex, InDepthPriorityGroup, WorldToLocal, OutMeshElement, bUseSelectedMaterial, bUseHoveredMaterial);
		if (ReflectionTexture->Resource)
		{
			// Wrap the material with a FTexturedMaterialRenderProxy which has hooks to override certain parameters with our per-instance data
			OverrideMaterialProxy = FTexturedMaterialRenderProxy(OutMeshElement.MaterialRenderProxy, ReflectionTexture->Resource, ReflectionColor);
		}
		OutMeshElement.MaterialRenderProxy = &OverrideMaterialProxy;
		return bShouldRender && ReflectionTexture->Resource;
	}

protected:

	UBOOL bDrawDepthRange;
	FLOAT DepthRange;
	UTexture2D* ReflectionTexture;
	FLinearColor ReflectionColor;
	mutable FTexturedMaterialRenderProxy OverrideMaterialProxy;
};

void UImageBasedReflectionComponent::Attach()
{
	Super::Attach();

	// Add the image reflection to the scene if it is valid
	if (ReflectionTexture && ReflectionTexture->LODGroup == TEXTUREGROUP_ImageBasedReflection)
	{
		FLOAT ColorRange = 1.0f;
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(GetOwner());
		if (CaptureOwner)
		{
			ColorRange = CaptureOwner->ColorRange;
		}
		Scene->AddImageReflection(this, ReflectionTexture, 1.0f, ReflectionColor * ReflectionColor.A * ColorRange * Scale, bTwoSided, bEnabled);
	}
}

void UImageBasedReflectionComponent::UpdateTransform()
{
	Super::UpdateTransform();

	if (ReflectionTexture && ReflectionTexture->LODGroup == TEXTUREGROUP_ImageBasedReflection)
	{
		FLOAT ColorRange = 1.0f;
		AImageReflectionSceneCapture* CaptureOwner = Cast<AImageReflectionSceneCapture>(GetOwner());
		if (CaptureOwner)
		{
			ColorRange = CaptureOwner->ColorRange;
		}
		Scene->UpdateImageReflection(this, ReflectionTexture, 1.0f, ReflectionColor * ReflectionColor.A * ColorRange * Scale, bTwoSided, bEnabled);
	}
}

void UImageBasedReflectionComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach(bWillReattach);

	Scene->RemoveImageReflection(this);
}

void UImageBasedReflectionComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredUpdateTransform();
	}
}

void UImageBasedReflectionComponent::UpdateImageReflectionParameters()
{
	BeginDeferredUpdateTransform();
}

FPrimitiveSceneProxy* UImageBasedReflectionComponent::CreateSceneProxy()
{
	return new FImageReflectionPreviewSceneProxy(this);
}

void UImageBasedReflectionComponent::PostLoad()
{
	Super::PostLoad();
}

void UImageBasedReflectionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetName() == TEXT("ReflectionTexture")
			&& ReflectionTexture)
		{
			for (TObjectIterator<UImageBasedReflectionComponent> ReflectionIt; ReflectionIt; ++ReflectionIt)
			{
				UImageBasedReflectionComponent* Reflection = *ReflectionIt;
				const UBOOL bReflectionIsInWorld = Reflection->GetOwner() && GWorld->ContainsActor(Reflection->GetOwner());
				if (bReflectionIsInWorld
					&& Reflection->ReflectionTexture
					&& Reflection->bEnabled
					&& (Reflection->ReflectionTexture->SizeX != ReflectionTexture->SizeX
					|| Reflection->ReflectionTexture->SizeY != ReflectionTexture->SizeY
					|| Reflection->ReflectionTexture->Mips.Num() != ReflectionTexture->Mips.Num()
					|| Reflection->ReflectionTexture->LODGroup != ReflectionTexture->LODGroup
					|| Reflection->ReflectionTexture->Format != ReflectionTexture->Format
					|| Reflection->ReflectionTexture->SRGB != ReflectionTexture->SRGB))
				{
					//@todo - render an indicator that the texture is invalid, currently it just disappears
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ReflectionTextureDoesntMatch"), *Reflection->GetOwner()->GetName()));
					break;
				}
			}

			if (ReflectionTexture->LODGroup != TEXTUREGROUP_ImageBasedReflection)
			{
				appMsgf(AMT_OK, *LocalizeUnrealEd("Error_ReflectionTextureInvalid"));
			}
		}
	}
}

void UImageReflectionShadowPlaneComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	Super::SetParentToWorld(ParentToWorld);
	//get the plane height
	FLOAT W = ParentToWorld.GetOrigin().Z;
	//get the plane normal
	FVector4 T = ParentToWorld.TransformNormal(FVector(0.0f,0.0f,1.0f));
	FVector PlaneNormal = FVector(T);
	PlaneNormal.Normalize();
	ReflectionPlane = FPlane(PlaneNormal, W);
}

void UImageReflectionShadowPlaneComponent::Attach()
{
	Super::Attach();

	if (bEnabled)
	{
		Scene->AddImageReflectionShadowPlane(this, ReflectionPlane);
	}
}

void UImageReflectionShadowPlaneComponent::UpdateTransform()
{
	Super::UpdateTransform();

	Scene->RemoveImageReflectionShadowPlane(this);

	if (bEnabled)
	{
		Scene->AddImageReflectionShadowPlane(this, ReflectionPlane);
	}
}

void UImageReflectionShadowPlaneComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach(bWillReattach);

	Scene->RemoveImageReflectionShadowPlane(this);
}

void UImageReflectionShadowPlaneComponent::SetEnabled(UBOOL bSetEnabled)
{
	if(bEnabled != bSetEnabled)
	{
		// Update bEnabled, and begin a deferred component reattach.
		bEnabled = bSetEnabled;
		BeginDeferredReattach();
	}
}

/*-----------------------------------------------------------------------------
	FImageReflectionSceneInfo
-----------------------------------------------------------------------------*/
FImageReflectionSceneInfo::FImageReflectionSceneInfo(const UActorComponent* InComponent, UTexture2D* InReflectionTexture, FLOAT ReflectionScale, const FLinearColor& InReflectionColor, UBOOL bInTwoSided, UBOOL bInEnabled) : 
	ReflectionTexture(InReflectionTexture),
	ReflectionColor(InReflectionColor),
	bTwoSided(bInTwoSided),
	bEnabled(bInEnabled)
{
	const ULightComponent* LightComponent = ConstCast<ULightComponent>(InComponent);
	bLightReflection = LightComponent != NULL;

	check(InComponent && (InReflectionTexture || bLightReflection));
	check(InComponent->GetOwner() || LightComponent);

	if (LightComponent)
	{
		ReflectionPlane = FPlane(0, 0, 0, 0);
		ReflectionOrigin = LightComponent->GetPosition();
		ReflectionXAxisAndYScale = FVector4(0, 1, 0, 1);
	}
	else
	{
		const FVector DrawScale3D = InComponent->GetOwner()->DrawScale * InComponent->GetOwner()->DrawScale3D * ReflectionScale;
		const FMatrix LocalToWorld = InComponent->GetOwner()->LocalToWorld();
		FVector ForwardVector(1.0f,0.0f,0.0f);
		FVector RightVector(0.0f,-1.0f,0.0f);

		const FVector4 PlaneNormal = LocalToWorld.TransformNormal(ForwardVector);
		const FVector Origin = LocalToWorld.GetOrigin();

		// Normalize the plane
		ReflectionPlane = FPlane(Origin, FVector(PlaneNormal).SafeNormal());
		ReflectionOrigin = Origin;
		const FVector ReflectionXAxis = LocalToWorld.TransformNormal(RightVector);
		// Include the owner's draw scale in the axes
		ReflectionXAxisAndYScale = ReflectionXAxis.SafeNormal() / (PlaneSize * DrawScale3D.Y);
		ReflectionXAxisAndYScale.W = DrawScale3D.Y / DrawScale3D.Z;
	}
}

/** Shader parameters needed to render image based reflections. */
class FImageReflectionShaderParameters
{
public:

	const static UINT MaxNumImageReflections;

	/** Binds the parameters. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FImageReflectionShaderParameters& P);

	/** Sets the shader parameters needed for image based reflections. */
	void Set(const FPixelShaderRHIRef& PixelShaderRHI, const FSceneView& View, FScene& Scene) const;

private:

	/** Parameters used for rendering image based reflections. */
	FShaderParameter NumActiveReflectionsParameter;
	FShaderParameter ImageReflectionPlaneParameter;
	FShaderParameter ImageReflectionOriginParameter;
	FShaderParameter ImageReflectionXAxisParameter;
	FShaderParameter ImageReflectionColorParameter;
	FShaderParameter CameraWorldPositionParameter;
	FShaderResourceParameter ImageReflectionTextureParameter;
	FShaderResourceParameter ImageReflectionSamplerParameter;
	FShaderParameter VolumeMinParameter;
	FShaderParameter VolumeSizeParameter;
	FShaderResourceParameter DistanceFieldTextureParameter;
	FShaderResourceParameter DistanceFieldSamplerParameter;
	FShaderResourceParameter EnvironmentTextureParameter;
	FShaderResourceParameter EnvironmentSamplerParameter;
	FShaderParameter EnvironmentColorParameter;
};

/** Vertex shader used to render deferred image reflections. */
class FImageReflectionVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FImageReflectionVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("NUM_IMAGE_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumImageReflections));
	}

	FImageReflectionVertexShader()	{}
	FImageReflectionVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		DeferredParameters.Set(View, this);

		if (ScreenToWorldParameter.IsBound())
		{
			const FMatrix ScreenToWorld = FMatrix(
				FPlane(1,0,0,0),
				FPlane(0,1,0,0),
				FPlane(0,0,(1.0f - Z_PRECISION),1),
				FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
				) *
				View.InvViewProjectionMatrix;

			SetVertexShaderValue(GetVertexShader(), ScreenToWorldParameter, ScreenToWorld);
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ScreenToWorldParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredVertexShaderParameters DeferredParameters;
	FShaderParameter ScreenToWorldParameter;
};

IMPLEMENT_SHADER_TYPE(,FImageReflectionVertexShader,TEXT("ImageReflectionShader"),TEXT("VertexMain"),SF_Vertex,0,0);

/** 
 * A pixel shader used to render deferred image reflections. 
 * Two versions - one that supports reading and shading the MSAA samples, and one that does not.
 */
template<UBOOL bSupportMSAA>
class TImageReflectionPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TImageReflectionPixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return Platform == SP_PCD3D_SM5;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("NUM_IMAGE_REFLECTIONS"),*appItoa(FImageReflectionShaderParameters::MaxNumImageReflections));
		OutEnvironment.Definitions.Set(TEXT("IMAGE_REFLECTION_MSAA"),bSupportMSAA ? TEXT("1") : TEXT("0"));
	}

	TImageReflectionPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ReflectionParameters.Bind(Initializer.ParameterMap);
		if (bSupportMSAA)
		{
			WorldNormalGBufferTextureMSAA.Bind(Initializer.ParameterMap,TEXT("WorldNormalGBufferTexture"),TRUE);
			SpecularGBufferTextureMSAA.Bind(Initializer.ParameterMap,TEXT("SpecularGBufferTexture"),TRUE);
			SceneDepthTextureMS.Bind(Initializer.ParameterMap,TEXT("SceneDepthTextureMS"),TRUE);
		}
	}

	TImageReflectionPixelShader()
	{
	}

	void SetParameters(const FSceneView& View, FScene& Scene)
	{
		ReflectionParameters.Set(GetPixelShader(), View, Scene);
		DeferredParameters.Set(View, this);
		if (bSupportMSAA)
		{
			SetSurfaceParameter(
				GetPixelShader(),
				WorldNormalGBufferTextureMSAA,
				GSceneRenderTargets.GetWorldNormalGBufferSurface());

			SetSurfaceParameter(
				GetPixelShader(),
				SpecularGBufferTextureMSAA,
				GSceneRenderTargets.GetSpecularGBufferSurface());

			SetSurfaceParameter(
				GetPixelShader(),
				SceneDepthTextureMS,
				GSceneRenderTargets.GetSceneDepthSurface());
		}
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ReflectionParameters;
		Ar << WorldNormalGBufferTextureMSAA;
		Ar << SpecularGBufferTextureMSAA;
		Ar << SceneDepthTextureMS;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FImageReflectionShaderParameters ReflectionParameters;
	FShaderResourceParameter WorldNormalGBufferTextureMSAA;
	FShaderResourceParameter SpecularGBufferTextureMSAA;
	FShaderResourceParameter SceneDepthTextureMS;
};

/** Implement a version that supports MSAA, and one that does not. */
IMPLEMENT_SHADER_TYPE(template<>,TImageReflectionPixelShader<TRUE>,TEXT("ImageReflectionShader"),TEXT("PixelMain"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(template<>,TImageReflectionPixelShader<FALSE>,TEXT("ImageReflectionShader"),TEXT("PixelMain"),SF_Pixel,0,0);

class FImageReflectionPerSamplePixelShader : public TImageReflectionPixelShader<TRUE>
{
	DECLARE_SHADER_TYPE(FImageReflectionPerSamplePixelShader,Global)
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TImageReflectionPixelShader<TRUE>::ShouldCache(Platform);
	}

	FImageReflectionPerSamplePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	TImageReflectionPixelShader<TRUE>(Initializer)
	{}

	FImageReflectionPerSamplePixelShader()
	{}
};

IMPLEMENT_SHADER_TYPE(,FImageReflectionPerSamplePixelShader,TEXT("ImageReflectionShader"),TEXT("SampleMain"),SF_Pixel,0,0);

FGlobalBoundShaderState ImageReflectionBoundStateMSAAFirstPass;
FGlobalBoundShaderState ImageReflectionBoundStateMSAASecondPass;
FGlobalBoundShaderState ImageReflectionBoundStateNoMSAA;

void FImageReflectionShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	NumActiveReflectionsParameter.Bind(ParameterMap,TEXT("NumReflectionsAndTextureResolution"),TRUE);
	ImageReflectionPlaneParameter.Bind(ParameterMap,TEXT("ImageReflectionPlane"),TRUE);
	ImageReflectionOriginParameter.Bind(ParameterMap,TEXT("ImageReflectionOrigin"),TRUE);
	ImageReflectionXAxisParameter.Bind(ParameterMap,TEXT("ReflectionXAxis"),TRUE);
	ImageReflectionColorParameter.Bind(ParameterMap,TEXT("ReflectionColor"),TRUE);
	CameraWorldPositionParameter.Bind(ParameterMap,TEXT("CameraWorldPos"),TRUE);
	ImageReflectionTextureParameter.Bind(ParameterMap,TEXT("ImageReflectionTexture"),TRUE);
	ImageReflectionSamplerParameter.Bind(ParameterMap,TEXT("ImageReflectionSampler"),TRUE);

	VolumeMinParameter.Bind(ParameterMap,TEXT("VolumeMinAndMaxDistance"),TRUE);
	VolumeSizeParameter.Bind(ParameterMap,TEXT("VolumeSizeAndbCalculateShadowing"),TRUE);
	DistanceFieldTextureParameter.Bind(ParameterMap,TEXT("DistanceFieldTexture"),TRUE);
	DistanceFieldSamplerParameter.Bind(ParameterMap,TEXT("DistanceFieldSampler"),TRUE);

	EnvironmentTextureParameter.Bind(ParameterMap,TEXT("EnvironmentTexture"),TRUE);
	EnvironmentSamplerParameter.Bind(ParameterMap,TEXT("EnvironmentSampler"),TRUE);
	EnvironmentColorParameter.Bind(ParameterMap,TEXT("EnvironmentColor"),TRUE);
}

FArchive& operator<<(FArchive& Ar,FImageReflectionShaderParameters& Parameters)
{
	Ar << Parameters.NumActiveReflectionsParameter;
	Ar << Parameters.ImageReflectionPlaneParameter;
	Ar << Parameters.ImageReflectionOriginParameter;
	Ar << Parameters.ImageReflectionXAxisParameter;
	Ar << Parameters.ImageReflectionColorParameter;
	Ar << Parameters.CameraWorldPositionParameter;
	Ar << Parameters.ImageReflectionTextureParameter;
	Ar << Parameters.ImageReflectionSamplerParameter;
	Ar << Parameters.VolumeMinParameter;
	Ar << Parameters.VolumeSizeParameter;
	Ar << Parameters.DistanceFieldTextureParameter;
	Ar << Parameters.DistanceFieldSamplerParameter;
	Ar << Parameters.EnvironmentTextureParameter;
	Ar << Parameters.EnvironmentSamplerParameter;
	Ar << Parameters.EnvironmentColorParameter;
	return Ar;
}

/** 
 * Number of simultaneous image reflections supported. 
 * This is limited by constant buffer size since the image reflection instance data takes a lot of constants to upload.
 */
const UINT FImageReflectionShaderParameters::MaxNumImageReflections = 85;

/** Sets the shader parameters needed for image based reflections. */
void FImageReflectionShaderParameters::Set(const FPixelShaderRHIRef& PixelShaderRHI, const FSceneView& View, FScene& Scene) const
{
#if PLATFORM_SUPPORTS_D3D10_PLUS
	if ((NumActiveReflectionsParameter.IsBound() || ImageReflectionTextureParameter.IsBound() || ImageReflectionPlaneParameter.IsBound())
		&& GRHIShaderPlatform == SP_PCD3D_SM5)
	{
		FVector4 ImageReflectionPlanes[MaxNumImageReflections];
		FVector4 ImageReflectionOrigins[MaxNumImageReflections];
		FVector4 ImageReflectionXAxes[MaxNumImageReflections];
		FVector4 ImageReflectionColors[MaxNumImageReflections];

		if (ImageReflectionSamplerParameter.IsBound())
		{
			// Set the sampler state used for sampling image reflection textures
			// Forces anisotropic filtering with lots of samples and border color clamping with a border alpha of 0 (transparent)
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				ImageReflectionSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_AnisotropicLinear,AM_Border,AM_Border,AM_Clamp,MIPBIAS_None,16,0>::GetRHI());
		}

		TArray<FImageReflectionSceneInfo*> ImageReflections;
		Scene.ImageReflections.GenerateValueArray(ImageReflections);

		INT ValidImageReflectionIndex = 0;
		for (INT ReflectionIndex = 0; ReflectionIndex < ImageReflections.Num(); ReflectionIndex++)
		{
			const FImageReflectionSceneInfo* ImageReflection = ImageReflections(ReflectionIndex);
			if (!ImageReflection->bLightReflection)
			{
				const INT TextureIndex = Scene.ImageReflectionTextureArray.GetTextureIndex(ImageReflection->ReflectionTexture);
				// Only use reflections with a valid texture resource
				if (TextureIndex != INDEX_NONE 
					&& ValidImageReflectionIndex < MaxNumImageReflections
					&& ImageReflection->bEnabled)
				{
					ImageReflectionPlanes[ValidImageReflectionIndex] = FVector4(ImageReflection->ReflectionPlane, ImageReflection->ReflectionPlane.W);
					ImageReflectionOrigins[ValidImageReflectionIndex] = FVector4(ImageReflection->ReflectionOrigin, TextureIndex);
					ImageReflectionXAxes[ValidImageReflectionIndex] = ImageReflection->ReflectionXAxisAndYScale;
					ImageReflectionColors[ValidImageReflectionIndex] = FVector4(FVector4(ImageReflection->ReflectionColor), ImageReflection->bTwoSided ? 1.0f : 0.0f);

					ValidImageReflectionIndex++;
				}
			}
		}

		const FLOAT NumActiveImageReflectionsFloat = ValidImageReflectionIndex;
		// Set the arrays of reflection quad data
		// Anisotropy value in z
		SetPixelShaderValue(PixelShaderRHI, NumActiveReflectionsParameter, FVector(NumActiveImageReflectionsFloat, Scene.ImageReflectionTextureArray.GetSizeX(), 16.0f));
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionPlaneParameter, &ImageReflectionPlanes, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionOriginParameter, &ImageReflectionOrigins, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionXAxisParameter, &ImageReflectionXAxes, ValidImageReflectionIndex);
		SetPixelShaderValues(PixelShaderRHI, ImageReflectionColorParameter, &ImageReflectionColors, ValidImageReflectionIndex);
		SetPixelShaderValue(PixelShaderRHI, CameraWorldPositionParameter, (FVector)View.ViewOrigin);

		if (ImageReflectionTextureParameter.IsBound())
		{
			if (ValidImageReflectionIndex > 0 && Scene.ImageReflectionTextureArray.IsInitialized())
			{
				check(Scene.ImageReflectionTextureArray.TextureRHI);
				RHISetTextureParameter(
					PixelShaderRHI, 
					ImageReflectionTextureParameter.GetBaseIndex(), 
					Scene.ImageReflectionTextureArray.TextureRHI);
			}
			else
			{
				RHISetTextureParameter(
					PixelShaderRHI, 
					ImageReflectionTextureParameter.GetBaseIndex(), 
					GBlackArrayTexture->TextureRHI);
			}
		}

		SetPixelShaderValue(PixelShaderRHI, VolumeMinParameter, FVector4(Scene.VolumeDistanceFieldBox.Min, Scene.VolumeDistanceFieldMaxDistance));
		const FLOAT CalculateVolumeShadowing = Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI && GSystemSettings.bAllowImageReflectionShadowing ? 1.0f : 0.0f;
		SetPixelShaderValue(PixelShaderRHI, VolumeSizeParameter, FVector4(Scene.VolumeDistanceFieldBox.Max - Scene.VolumeDistanceFieldBox.Min, CalculateVolumeShadowing));

		if (EnvironmentSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				EnvironmentSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_Trilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI());
		}

		if (EnvironmentTextureParameter.IsBound())
		{
			if (Scene.ImageReflectionEnvironmentTexture
				&& Scene.ImageReflectionEnvironmentTexture->Resource->TextureRHI)
			{
				// Update last render time so it will be streamed correctly
				Scene.ImageReflectionEnvironmentTexture->Resource->LastRenderTime = GCurrentTime;
				RHISetTextureParameter(
					PixelShaderRHI, 
					EnvironmentTextureParameter.GetBaseIndex(), 
					Scene.ImageReflectionEnvironmentTexture->Resource->TextureRHI);
			}
			else
			{
				RHISetTextureParameter(
					PixelShaderRHI, 
					EnvironmentTextureParameter.GetBaseIndex(), 
					GBlackTexture->TextureRHI);
			}
		}

		SetPixelShaderValue(PixelShaderRHI, EnvironmentColorParameter, FVector4(Scene.EnvironmentColor, Scene.EnvironmentRotation * PI / 180.0f));

		if (DistanceFieldTextureParameter.IsBound() 
			&& Scene.PrecomputedDistanceFieldVolumeTexture.IsInitialized()
			&& Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI)
		{
			RHISetTextureParameter(
				PixelShaderRHI, 
				DistanceFieldTextureParameter.GetBaseIndex(), 
				Scene.PrecomputedDistanceFieldVolumeTexture.TextureRHI);
		}

		if (DistanceFieldSamplerParameter.IsBound())
		{
			RHISetSamplerStateOnly(
				PixelShaderRHI, 
				DistanceFieldSamplerParameter.GetSamplerIndex(), 
				TStaticSamplerState<SF_Trilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		}

	}
#endif
}

/** Creates planar reflection shadows for this frame. */
void FSceneRenderer::CreatePlanarReflectionShadows()
{
}

/** Renders deferred image reflections. */
UBOOL FSceneRenderer::RenderImageReflections(UINT DPGIndex)
{
#if PLATFORM_SUPPORTS_D3D10_PLUS
	if (DPGIndex == SDPG_World 
		&& GRHIShaderPlatform == SP_PCD3D_SM5 
		&& GSystemSettings.bAllowImageReflections
		&& (ViewFamily.ShowFlags & SHOW_ImageReflections))
	{
		Scene->ImageReflectionTextureArray.UpdateResource();

		UBOOL bAnyReflectionsToRender = Scene->ImageReflectionTextureArray.IsInitialized() || Scene->ImageReflectionEnvironmentTexture;
		for (TMap<const UActorComponent*, FImageReflectionSceneInfo*>::TConstIterator ReflectionIt(Scene->ImageReflections); ReflectionIt; ++ReflectionIt)
		{
			const FImageReflectionSceneInfo* ImageReflection = ReflectionIt.Value();
			bAnyReflectionsToRender = bAnyReflectionsToRender 
				|| (!ImageReflection->bLightReflection && Scene->ImageReflectionTextureArray.GetTextureIndex(ImageReflection->ReflectionTexture) != INDEX_NONE);
		}

		if (bAnyReflectionsToRender)
		{
			SCOPED_DRAW_EVENT(EventRenderDeferredReflections)(DEC_SCENE_ITEMS,TEXT("Deferred Reflections"));

			{
				// No depth tests
				RHISetDepthState(TStaticDepthState<FALSE, CF_Always>::GetRHI());

				// No backface culling
				RHISetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

				// Bind scene color
				GSceneRenderTargets.BeginRenderingSceneColor();

				// Use additive blending for color, and keep the destination alpha.
				RHISetBlendState(TStaticBlendState<BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());

				if (GSystemSettings.UsesMSAA())
				{
					// Clear stencil to 0
					RHIClear(FALSE, FLinearColor(0,0,0,0), FALSE, 0, TRUE, 0);
				}

				for (INT ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					FViewInfo& View = Views(ViewIndex);

					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
					RHISetViewParameters(View);

					TShaderMapRef<FImageReflectionVertexShader> VertexShader(GetGlobalShaderMap());
					VertexShader->SetParameters(View);

					if (GSystemSettings.UsesMSAA())
					{
						SCOPED_DRAW_EVENT(EventRenderImageReflections)(DEC_SCENE_ITEMS,TEXT("Image Reflections"));

						// Set stencil to one.
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0xff,1
							>::GetRHI());

						TShaderMapRef<TImageReflectionPixelShader<TRUE> > PixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateMSAAFirstPass,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex)
							);

						PixelShader->SetParameters(View, *Scene);

						DrawDenormalizedQuad( 
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetX, View.RenderTargetY, 
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							View.RenderTargetSizeX, View.RenderTargetSizeY,
							GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());


						// Pass if 0
						RHISetStencilState(TStaticStencilState<
							TRUE,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							0xff,0,0
							>::GetRHI());

						TShaderMapRef<FImageReflectionPerSamplePixelShader> SamplePixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateMSAASecondPass,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*SamplePixelShader,
							sizeof(FFilterVertex)
							);

						SamplePixelShader->SetParameters(View, *Scene);
					}
					else
					{
						TShaderMapRef<TImageReflectionPixelShader<FALSE> > PixelShader(GetGlobalShaderMap());
						SetGlobalBoundShaderState(
							ImageReflectionBoundStateNoMSAA,
							GFilterVertexDeclaration.VertexDeclarationRHI,
							*VertexShader,
							*PixelShader,
							sizeof(FFilterVertex)
							);

						PixelShader->SetParameters(View, *Scene);
					}

					SCOPED_DRAW_EVENT(EventRenderPerSample)(DEC_SCENE_ITEMS,(GSystemSettings.UsesMSAA() ? TEXT("PerSample IR") : TEXT("Image Reflections")));

					DrawDenormalizedQuad( 
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetX, View.RenderTargetY, 
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						View.RenderTargetSizeX, View.RenderTargetSizeY,
						GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY());
				}

				// Restore default stencil state
				RHISetStencilState(TStaticStencilState<>::GetRHI());

				GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
			}
			return TRUE;
		}
	}
#endif

	return FALSE;
}
