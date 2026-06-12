/*=============================================================================
	SubsurfaceScatteringRendering.cpp: Subsurface scattering rendering implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"

#if !CONSOLE
	/** A vertex shader for subsurface scattering. */
	class FSubsurfaceScatteringVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE(FSubsurfaceScatteringVertexShader,Global);
	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return IsPCPlatform(Platform);
		}

		FSubsurfaceScatteringVertexShader()	{}
		FSubsurfaceScatteringVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
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

	IMPLEMENT_SHADER_TYPE(,FSubsurfaceScatteringVertexShader,TEXT("SubsurfaceScatteringVertexShader"),TEXT("Main"),SF_Vertex,0,0);

	/** A pixel shader for subsurface scattering. */
	class FSubsurfaceScatteringPixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE(FSubsurfaceScatteringPixelShader,Global)

		enum { NumSamplePairs = 20 };
		enum { NumSamples = NumSamplePairs * 2 };
		enum { NumRadialStrata = 4 };
		enum { NumAngularStrata = 10 };

	public:

		static UBOOL ShouldCache(EShaderPlatform Platform)
		{
			return IsPCPlatform(Platform);
		}

		static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
		{
			FShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
			OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),(UINT)NumSamples));
		}

		FSubsurfaceScatteringPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
		{
			SceneTextureParameters.Bind(Initializer.ParameterMap);
			SampleDeltaUVsParameter.Bind(Initializer.ParameterMap,TEXT("SampleDeltaUVs"),TRUE);
			ClipToViewTransformParameter.Bind(Initializer.ParameterMap,TEXT("ClipToViewTransform"),TRUE);
			WorldFilterRadiusParameter.Bind(Initializer.ParameterMap,TEXT("WorldFilterRadius"),TRUE);
			SubsurfaceInscatteringTextureParameter.Bind(Initializer.ParameterMap,TEXT("SubsurfaceInscatteringTexture"),TRUE);
			SubsurfaceScatteringAttenuationTextureParameter.Bind(Initializer.ParameterMap,TEXT("SubsurfaceScatteringAttenuationTexture"),TRUE);
			RandomAngleTextureParameter.Bind(Initializer.ParameterMap,TEXT("RandomAngleTexture"),TRUE);
			NoiseScaleAndOffsetParameter.Bind(Initializer.ParameterMap,TEXT("NoiseScaleAndOffset"),TRUE);
		}

		FSubsurfaceScatteringPixelShader()
		{
		}

		void SetParameters(const FSceneView& View)
		{
			SceneTextureParameters.Set(&View,this,SF_Point);

			FRandomStream Random;

			// Create sample points in a stratified disc.
			// Note that it divides the disc's radius equally between strata, so the samples will not be uniformly distributed.
			// This corresponds to a lower sampling density far from the sample origin.
			static const FLOAT StrataAngle = 2.0f * PI / NumAngularStrata;
			static const FLOAT StrataRadius = 1.0f / NumRadialStrata;
			FVector2D SampleDeltaUVs[NumSamples];
			for(UINT RadialStrataIndex = 0;RadialStrataIndex < NumRadialStrata;++RadialStrataIndex)
			{
				for(UINT AngularStrataIndex = 0;AngularStrataIndex < NumAngularStrata;++AngularStrataIndex)
				{
					const UINT SampleIndex = RadialStrataIndex * NumAngularStrata + AngularStrataIndex;

					const FLOAT Angle = (AngularStrataIndex + Random.GetFraction()) * StrataAngle;
					const FLOAT Radius = (RadialStrataIndex + Random.GetFraction()) * StrataRadius;
					SampleDeltaUVs[SampleIndex] = FVector2D(
						appCos(Angle) * Radius,
						appSin(Angle) * Radius
						);
				}
			}
			
			// Set the random normal texture.
			UTexture2D* const RandomAngleTexture = GEngine->RandomAngleTexture;
			SetTextureParameter(
				GetPixelShader(),
				RandomAngleTextureParameter,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				RandomAngleTexture->Resource->TextureRHI
				);

			Random.Initialize(appCycles());
			const FVector4 NoiseScaleAndOffset = FVector4(
				GSceneRenderTargets.GetBufferSizeX() / (FLOAT)RandomAngleTexture->SizeX, 
				GSceneRenderTargets.GetBufferSizeY() / (FLOAT)RandomAngleTexture->SizeY,
				Random.GetFraction(),
				Random.GetFraction());
			SetPixelShaderValue(GetPixelShader(), NoiseScaleAndOffsetParameter, NoiseScaleAndOffset);

			// Pack the samples and put them in constant registers.
			for(UINT SamplePairIndex = 0;SamplePairIndex < NumSamplePairs;++SamplePairIndex)
			{
				const FVector4 PackedSamplePair(
					SampleDeltaUVs[SamplePairIndex * 2 + 0].X * View.ProjectionMatrix.M[0][0],
					SampleDeltaUVs[SamplePairIndex * 2 + 0].Y * View.ProjectionMatrix.M[1][1],
					SampleDeltaUVs[SamplePairIndex * 2 + 1].X * View.ProjectionMatrix.M[0][0],
					SampleDeltaUVs[SamplePairIndex * 2 + 1].Y * View.ProjectionMatrix.M[1][1]
					);
				SetPixelShaderValue(GetPixelShader(),SampleDeltaUVsParameter,PackedSamplePair,SamplePairIndex);
			}

			SetPixelShaderValue(GetPixelShader(),ClipToViewTransformParameter,View.InvProjectionMatrix);

			SetTextureParameter(GetPixelShader(),SubsurfaceInscatteringTextureParameter,TStaticSamplerState<>::GetRHI(),GSceneRenderTargets.GetSubsurfaceInscatteringTexture());
			SetTextureParameter(GetPixelShader(),SubsurfaceScatteringAttenuationTextureParameter,TStaticSamplerState<>::GetRHI(),GSceneRenderTargets.GetSubsurfaceScatteringAttenuationTexture());
		}

		virtual UBOOL Serialize(FArchive& Ar)
		{		
			UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << SceneTextureParameters;
			Ar << SampleDeltaUVsParameter;
			Ar << ClipToViewTransformParameter;
			Ar << WorldFilterRadiusParameter;
			Ar << SubsurfaceInscatteringTextureParameter;
			Ar << SubsurfaceScatteringAttenuationTextureParameter;
			Ar << RandomAngleTextureParameter;
			Ar << NoiseScaleAndOffsetParameter;
			return bShaderHasOutdatedParameters;
		}

	private:

		FSceneTextureShaderParameters SceneTextureParameters;

		FShaderParameter SampleDeltaUVsParameter;
		FShaderParameter ClipToViewTransformParameter;
		FShaderParameter WorldFilterRadiusParameter;

		FShaderResourceParameter SubsurfaceInscatteringTextureParameter;
		FShaderResourceParameter SubsurfaceScatteringAttenuationTextureParameter;
		
		FShaderResourceParameter RandomAngleTextureParameter;
		FShaderParameter NoiseScaleAndOffsetParameter;
	};

	IMPLEMENT_SHADER_TYPE(,FSubsurfaceScatteringPixelShader,TEXT("SubsurfaceScatteringPixelShader"),TEXT("Main"),SF_Pixel,0,0);

	/** The subsurface scattering vertex declaration resource type. */
	class FSubsurfaceScatteringVertexDeclaration : public FRenderResource
	{
	public:
		FVertexDeclarationRHIRef VertexDeclarationRHI;

		// Destructor
		virtual ~FSubsurfaceScatteringVertexDeclaration() {}

		virtual void InitRHI()
		{
			FVertexDeclarationElementList Elements;
			Elements.AddItem(FVertexElement(0,0,VET_Float2,VEU_Position,0));
			VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
		}

		virtual void ReleaseRHI()
		{
			VertexDeclarationRHI.SafeRelease();
		}
	};

	/** Vertex declaration for the light function fullscreen 2D quad. */
	TGlobalResource<FSubsurfaceScatteringVertexDeclaration> GSubsurfaceScatteringVertexDeclaration;

	/** The bound shader state for the subsurface scattering shaders without MSAA. */
	FGlobalBoundShaderState GSubsurfaceScatteringBoundShaderStateNoMSAA;
#endif

UBOOL FSceneRenderer::RenderSubsurfaceScattering(UINT DPGIndex)
{
	#if !CONSOLE
		if (DPGIndex == SDPG_World // Only allow materials in the world DPG to use SSS for now.
			&& GRHIShaderPlatform == SP_PCD3D_SM5
			&& GSystemSettings.RenderThreadSettings.bAllowSubsurfaceScattering)
		{
			GSceneRenderTargets.ResolveSubsurfaceScatteringSurfaces();

			static const FVector2D Vertices[4] =
			{
				FVector2D(-1,-1),
				FVector2D(-1,+1),
				FVector2D(+1,+1),
				FVector2D(+1,-1),
			};
			static const WORD Indices[6] =
			{
				0, 1, 2,
				0, 2, 3
			};

			GSceneRenderTargets.BeginRenderingSceneColor();

			for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				const FViewInfo& View = Views(ViewIndex);

				if(View.Family->ShowFlags & SHOW_SubsurfaceScattering)
				{
					SCOPED_CONDITIONAL_DRAW_EVENT(EventRenderSS,ViewIndex == 0)(DEC_SCENE_ITEMS,TEXT("Subsurface Scattering"));

					// Set the device viewport for the view.
					RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
					RHISetViewParameters(View);

					// No depth or stencil tests, no backface culling.
					RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
					RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
					RHISetStencilState(TStaticStencilState<>::GetRHI());

					// Use additive blending for color, and keep the destination alpha.
					RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One,BO_Add,BF_Zero,BF_One>::GetRHI());

					TShaderMapRef<FSubsurfaceScatteringVertexShader> VertexShader(GetGlobalShaderMap());
					TShaderMapRef<FSubsurfaceScatteringPixelShader> PixelShader(GetGlobalShaderMap());
					SetGlobalBoundShaderState(
						GSubsurfaceScatteringBoundShaderStateNoMSAA,
						GSubsurfaceScatteringVertexDeclaration.VertexDeclarationRHI,
						*VertexShader,
						*PixelShader,
						sizeof(FVector2D)
						);
					VertexShader->SetParameters(View);
					PixelShader->SetParameters(View);

					// Draw a quad covering the view.
					RHIDrawIndexedPrimitiveUP(
						PT_TriangleList,
						0,
						ARRAY_COUNT(Vertices),
						2,
						Indices,
						sizeof(Indices[0]),
						Vertices,
						sizeof(Vertices[0])
						);
				}
			}
			return TRUE;
		}
	#endif

	return FALSE;
}
