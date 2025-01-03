/*=============================================================================
	TemporalAARendering.cpp: Temporal anti-aliasing implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"

/** A vertex shader for subsurface scattering. */
class FTemporalAAVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	FTemporalAAVertexShader()	{}
	FTemporalAAVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{}

	void SetParameters(const FViewInfo& View)
	{}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAVertexShader,TEXT("TemporalAA"),TEXT("MainVertexShader"),SF_Vertex,0,0);

/** A pixel shader for subsurface scattering. */
class FTemporalAAPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTemporalAAPixelShader,Global)

public:

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FTemporalAAPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	:	FGlobalShader(Initializer)
	{
		PreviousFrameBackBufferParameter.Bind(Initializer.ParameterMap,TEXT("PreviousFrameBackBuffer"),TRUE);
		CurrentFrameToPreviousFrameTransformParameter.Bind(Initializer.ParameterMap,TEXT("CurrentFrameToPreviousFrameTransform"),TRUE);
		ScreenPositionToBackBufferUVScaleBias.Bind(Initializer.ParameterMap,TEXT("ScreenPositionToBackBufferUVScaleBias"),TRUE);
		SceneTextures.Bind(Initializer.ParameterMap);
	}

	FTemporalAAPixelShader()
	{
	}

	void SetParameters(const FViewInfo& View)
	{
		SetTextureParameter(
			GetPixelShader(),
			PreviousFrameBackBufferParameter,
			TStaticSamplerState<>::GetRHI(),
			GSceneRenderTargets.GetRenderTargetTexture(PreviousFrameBackBuffer)
			);

		FMatrix ScreenToTranslatedWorld = FMatrix(
				FPlane(1,0,0,0),
				FPlane(0,1,0,0),
				FPlane(0,0,(1.0f - Z_PRECISION),1),
				FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0) ) * View.InvTranslatedViewProjectionMatrix;

		ScreenToTranslatedWorld.M[0][3] = 0.f; // Note that we reset the column here because in the shader we only used
		ScreenToTranslatedWorld.M[1][3] = 0.f; // the x, y, and z components of the matrix multiplication result and we
		ScreenToTranslatedWorld.M[2][3] = 0.f; // set the w component to 1 before multiplying by the CurrentFrameToPreviousFrameTransform.
		ScreenToTranslatedWorld.M[3][3] = 1.f;

		const FMatrix CurrentFrameToPreviousFrameTransform =
			ScreenToTranslatedWorld
			* FTranslationMatrix(View.PrevPreViewTranslation - View.PreViewTranslation)
			* View.PrevTranslatedViewProjectionMatrix;
		SetPixelShaderValue(GetPixelShader(),CurrentFrameToPreviousFrameTransformParameter,CurrentFrameToPreviousFrameTransform);

		SetPixelShaderValue(GetPixelShader(),ScreenPositionToBackBufferUVScaleBias,FVector4(
			View.SizeX / View.Family->RenderTarget->GetSizeX() / +2.0f,
			View.SizeY / View.Family->RenderTarget->GetSizeY() / -2.0f,
			(View.SizeX / 2.0f + GPixelCenterOffset + View.X) / View.Family->RenderTarget->GetSizeX(),
			(View.SizeY / 2.0f + GPixelCenterOffset + View.Y) / View.Family->RenderTarget->GetSizeY()
			));

		SceneTextures.Set(&View,this,SF_Point);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{		
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << PreviousFrameBackBufferParameter;
		Ar << CurrentFrameToPreviousFrameTransformParameter;
		Ar << ScreenPositionToBackBufferUVScaleBias;
		Ar << SceneTextures;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderResourceParameter PreviousFrameBackBufferParameter;
	FShaderParameter CurrentFrameToPreviousFrameTransformParameter;
	FShaderParameter ScreenPositionToBackBufferUVScaleBias;

	FSceneTextureShaderParameters SceneTextures;
};

IMPLEMENT_SHADER_TYPE(,FTemporalAAPixelShader,TEXT("TemporalAA"),TEXT("MainPixelShader"),SF_Pixel,0,0);

/** The bound shader state for the subsurface scattering shaders without MSAA. */
FGlobalBoundShaderState GTemporalAABoundShaderState;

void FSceneRenderer::RenderTemporalAA()
{
	// Check if any view has temporal AA enabled.
	UBOOL bAnyViewHasTemporalAA = FALSE;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views(ViewIndex);
		if(View.RenderingOverrides.bAllowTemporalAA)
		{
			bAnyViewHasTemporalAA = TRUE;
		}
	}

	// Update the temporal AA render target allocation based on whether it's currently needed.
	// TEMP: Assume that we always need the buffers if Temporal AA is allowed to avoid dynamic reallocation possibly failing due to fragmentation.
	const UBOOL bNeedsAllocation = GSystemSettings.RenderThreadSettings.bAllowTemporalAA;
	GSceneRenderTargets.UpdateTemporalAAAllocation(bNeedsAllocation);

	// Check whether temporal AA is enabled in the configuration.
	// Don't allow it in the editor, since it only has a single previous frame backbuffer and there may be multiple viewports in the editor.
	// Also don't allow it unless post-process is enabled, since the temporal AA masking is computed in the uber post process blend shader.
	if(GSystemSettings.RenderThreadSettings.bAllowTemporalAA
	&& (ViewFamily.ShowFlags & SHOW_TemporalAA)
	&& (ViewFamily.ShowFlags & SHOW_PostProcess)
	&& !GIsEditor
	&& bAnyViewHasTemporalAA)
	{
		SCOPED_DRAW_EVENT(EventRenderTAA)(DEC_SCENE_ITEMS,TEXT("Temporal AA"));

		// Make a copy of the current frame's back buffer before the temporal AA is applied.
		RHICopyToResolveTarget(ViewFamily.RenderTarget->GetRenderTargetSurface(),FALSE,FResolveParams(FResolveRect(),CubeFace_PosX,GSceneRenderTargets.GetRenderTargetTexture(CurrentFrameBackBuffer)));

		// Draw the temporal AA to the view family render target.
		RHISetRenderTarget(ViewFamily.RenderTarget->GetRenderTargetSurface(),FSurfaceRHIRef());

		// Don't apply temporal AA if PreviousFrameBackBuffer doesn't contain the back buffer from the previous frame.
		// This happens on the first frame, when toggling temporal AA, etc.
		if(GSceneRenderTargets.IsPreviousFrameBackBufferValid(FrameNumber))
		{
			SCOPED_DRAW_EVENT(EventRenderApplyTAA)(DEC_SCENE_ITEMS,TEXT("Apply TemporalAA"));

			// Allocate more GPRs to the pixel shader.
			RHISetShaderRegisterAllocation(16, 112);

			for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				const FViewInfo& View = Views(ViewIndex);

				// Don't draw temporal AA if this frame is unrelated to the previous frame, or if temporal AA is disabled for this view by the rendering overrides.
				if(!View.bPrevTransformsReset && View.RenderingOverrides.bAllowTemporalAA)
				{
					TGuardValue<UBOOL> SavedLDRColor(*const_cast<UBOOL*>(&View.bUseLDRSceneColor),FALSE);

					// Set the device viewport for the view.
					RHISetViewport(appTrunc(View.X),appTrunc(View.Y),0,appTrunc(View.X) + View.RenderTargetSizeX,appTrunc(View.Y) + View.RenderTargetSizeY,1);
					RHISetViewParameters(View);

					// No depth or stencil tests, no backface culling.
					RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
					RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
					RHISetStencilState(TStaticStencilState<>::GetRHI());

					// Use dest alpha (the current frame's temporal aa mask) to blend between source and dest color.
					// Use alpha test to clip based on source alpha (the previous frame's temporal aa mask).
					RHISetBlendState(TStaticBlendState<BO_Add,BF_DestAlpha,BF_InverseDestAlpha,BO_Add,BF_One,BF_Zero,CF_GreaterEqual,255>::GetRHI());

					// Set the temporal AA shaders.
					TShaderMapRef<FTemporalAAVertexShader> VertexShader(GetGlobalShaderMap());
					TShaderMapRef<FTemporalAAPixelShader> PixelShader(GetGlobalShaderMap());
					SetGlobalBoundShaderState(
						GTemporalAABoundShaderState,
						GFilterVertexDeclaration.VertexDeclarationRHI,
						*VertexShader,
						*PixelShader,
						sizeof(FFilterVertex)
						);
					VertexShader->SetParameters(View);
					PixelShader->SetParameters(View);

					// Draw a quad over the viewport.
					DrawDenormalizedQuad(
						0,0,
						View.RenderTargetSizeX,View.RenderTargetSizeY,
						0,0,
						0,0,
						View.RenderTargetSizeX,View.RenderTargetSizeY,
						1,1
						);
				}
			}
			
			// Restore the GPR allocation.
			RHISetShaderRegisterAllocation(64, 64);
		}

		// Reinterpret the saved copy of the current frame's back buffer as the next frame's previous frame's back buffer.
		GSceneRenderTargets.SwapCurrentFrameAndPreviousFrameSavedBackBuffers(FrameNumber);
	}
}
