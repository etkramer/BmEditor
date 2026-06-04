/*=============================================================================
	RockAtmosEffect.cpp: Batman: Arkham City atmospheric fog post process.

	A fullscreen pass that reconstructs world position from scene depth and
	composites up to two distance-based fog layers (D1/D2) and two height-based
	layers (H1/H2), plus an optional scrolling volume noise and a global
	directional gradient. Ported from BM2 ("Rock") to match the original game.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

IMPLEMENT_CLASS(URockAtmos);

/*-----------------------------------------------------------------------------
	Resolved atmosphere settings (effect values blended with world overrides).
-----------------------------------------------------------------------------*/
struct FRockAtmosData
{
	FColor	D1Colour;
	FLOAT	D1Density;
	FLOAT	D1DistanceStart;
	FLOAT	D1DistanceEnd;

	FColor	D2Colour;
	FLOAT	D2Density;
	FLOAT	D2DistanceStart;
	FLOAT	D2DistanceEnd;

	FColor	H1Colour;
	FLOAT	H1Density;
	FLOAT	H1GradientSize;
	FLOAT	H1GradientPosition;

	FColor	H2Colour;
	FLOAT	H2Density;
	FLOAT	H2GradientSize;
	FLOAT	H2GradientPosition;

	FVector	NoiseWind;
	FVector	NoiseOffset;
	FLOAT	NoiseFade;

	FColor	GlobalGradientColour;
	FVector	GlobalGradientDirection;
	FLOAT	GlobalGradientDensity;
};

/*-----------------------------------------------------------------------------
	FRockAtmosVertexShader
-----------------------------------------------------------------------------*/
class FRockAtmosVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockAtmosVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

public:
	FShaderParameter ScreenPositionScaleBiasParameter;
	FShaderParameter ScreenToWorldParameter;

	FRockAtmosVertexShader() {}

	FRockAtmosVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		ScreenPositionScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("ScreenPositionScaleBias"),TRUE);
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		SetVertexShaderValue(GetVertexShader(),ScreenPositionScaleBiasParameter,View.ScreenPositionScaleBias);

		// Transform from screen space to translated (camera-relative) world space.
		const FMatrix ScreenToWorld = FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) *
			View.InvTranslatedViewProjectionMatrix;

		SetVertexShaderValue(GetVertexShader(),ScreenToWorldParameter,ScreenToWorld);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ScreenPositionScaleBiasParameter << ScreenToWorldParameter;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FRockAtmosVertexShader,TEXT("RockAtmosVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/*-----------------------------------------------------------------------------
	TRockAtmosPixelShader - templated on the active layers (32 permutations).
-----------------------------------------------------------------------------*/
template<UBOOL bAllowH1, UBOOL bAllowH2, UBOOL bAllowD1, UBOOL bAllowD2, UBOOL bAllowNoise>
class TRockAtmosPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockAtmosPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosH1"), bAllowH1 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosH2"), bAllowH2 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosD1"), bAllowD1 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosD2"), bAllowD2 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosNoise"), bAllowNoise ? TEXT("1") : TEXT("0"));
	}

public:
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderParameter				AtmosDensityParameter;
	FShaderParameter				AtmosStartEndParameter;
	FShaderParameter				AtmosGradSizeHeightIntegralParameter;
	FShaderParameter				AtmosD1ColourParameter;
	FShaderParameter				AtmosD2ColourParameter;
	FShaderParameter				AtmosH1ColourParameter;
	FShaderParameter				AtmosH2ColourParameter;
	FShaderParameter				AtmosH1H2AlphaParameter;
	FShaderParameter				AtmosGlobalGradientColourParameter;
	FShaderParameter				AtmosGlobalGradientDirectionParameter;
	FShaderParameter				CameraPosParameter;
	FShaderResourceParameter		AtmosphericNoiseTextureParameter;
	FShaderParameter				AtmosNoiseWindParameter;
	FShaderParameter				BufferTexelSizeXYParameter;

	TRockAtmosPixelShader() {}

	TRockAtmosPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		AtmosDensityParameter.Bind(Initializer.ParameterMap,TEXT("AtmosDensity_xD1yD2zH1wH2"),TRUE);
		AtmosStartEndParameter.Bind(Initializer.ParameterMap,TEXT("AtmosStartEnd_xD1yD1zD2wD2"),TRUE);
		AtmosGradSizeHeightIntegralParameter.Bind(Initializer.ParameterMap,TEXT("AtmosGradSizeHeightIntegral_xH1yH1zH2wH2"),TRUE);
		AtmosD1ColourParameter.Bind(Initializer.ParameterMap,TEXT("AtmosD1_Colour"),TRUE);
		AtmosD2ColourParameter.Bind(Initializer.ParameterMap,TEXT("AtmosD2_Colour"),TRUE);
		AtmosH1ColourParameter.Bind(Initializer.ParameterMap,TEXT("AtmosH1_Colour"),TRUE);
		AtmosH2ColourParameter.Bind(Initializer.ParameterMap,TEXT("AtmosH2_Colour"),TRUE);
		AtmosH1H2AlphaParameter.Bind(Initializer.ParameterMap,TEXT("AtmosH1H2_Alpha"),TRUE);
		AtmosGlobalGradientColourParameter.Bind(Initializer.ParameterMap,TEXT("AtmosGlobal_Gradient_Colour"),TRUE);
		AtmosGlobalGradientDirectionParameter.Bind(Initializer.ParameterMap,TEXT("AtmosGlobal_Gradient_Direction"),TRUE);
		CameraPosParameter.Bind(Initializer.ParameterMap,TEXT("CameraPos"),TRUE);
		AtmosphericNoiseTextureParameter.Bind(Initializer.ParameterMap,TEXT("AtmosphericNoiseTexture"),TRUE);
		AtmosNoiseWindParameter.Bind(Initializer.ParameterMap,TEXT("AtmosNoiseWind"),TRUE);
		BufferTexelSizeXYParameter.Bind(Initializer.ParameterMap,TEXT("BufferTexelSizeXY"),TRUE);
	}

	void SetParameters(const FViewInfo& View, const FRockAtmosData& Data)
	{
		SceneTextureParameters.Set(&View, this, SF_Point);

		const FLOAT CameraZ = View.ViewOrigin.Z;
		const FLOAT Inv255 = 1.0f / 255.0f;

		// AtmosDensity mirrors the RockAtmos material fog path.
		const FVector4 Density(
			(Data.D1Colour.A * Inv255) * Data.D1Density,
			(Data.D2Colour.A * Inv255) * Data.D2Density,
			Data.H1Density * 1.0e-6f,
			Data.H2Density * 1.0e-6f);
		SetPixelShaderValue(GetPixelShader(), AtmosDensityParameter, Density);

		// AtmosStartEnd: D1/D2 start and reciprocal of (end-start).
		const FVector4 StartEnd(
			Data.D1DistanceStart,
			1.0f / (Data.D1DistanceEnd - Data.D1DistanceStart),
			Data.D2DistanceStart,
			1.0f / (Data.D2DistanceEnd - Data.D2DistanceStart));
		SetPixelShaderValue(GetPixelShader(), AtmosStartEndParameter, StartEnd);

		// AtmosGradSizeHeightIntegral: gradient size (1/size) and precomputed height integral term.
		const FLOAT H1Norm = Clamp((Data.H1GradientPosition - CameraZ) / Data.H1GradientSize, -50.0f, 50.0f);
		const FLOAT H2Norm = Clamp((Data.H2GradientPosition - CameraZ) / Data.H2GradientSize, -50.0f, 50.0f);

		const FVector4 GradSizeHeightIntegral(
			1.0f / Data.H1GradientSize,
			appExp(H1Norm) * Data.H1Density * 1.0e-6f,
			1.0f / Data.H2GradientSize,
			appExp(H2Norm) * Data.H2Density * 1.0e-6f);
		SetPixelShaderValue(GetPixelShader(), AtmosGradSizeHeightIntegralParameter, GradSizeHeightIntegral);

		// Layer colours (linear, no sRGB conversion).
		SetPixelShaderValue(GetPixelShader(), AtmosD1ColourParameter, FLinearColor(Data.D1Colour.R*Inv255, Data.D1Colour.G*Inv255, Data.D1Colour.B*Inv255, Data.D1Colour.A*Inv255));
		SetPixelShaderValue(GetPixelShader(), AtmosD2ColourParameter, FLinearColor(Data.D2Colour.R*Inv255, Data.D2Colour.G*Inv255, Data.D2Colour.B*Inv255, Data.D2Colour.A*Inv255));
		SetPixelShaderValue(GetPixelShader(), AtmosH1ColourParameter, FLinearColor(Data.H1Colour.R*Inv255, Data.H1Colour.G*Inv255, Data.H1Colour.B*Inv255, Data.H1Colour.A*Inv255));
		SetPixelShaderValue(GetPixelShader(), AtmosH2ColourParameter, FLinearColor(Data.H2Colour.R*Inv255, Data.H2Colour.G*Inv255, Data.H2Colour.B*Inv255, Data.H2Colour.A*Inv255));

		// H1/H2 alpha and noise fade.
		const FVector AtmosH1H2Alpha(Data.H1Colour.A * Inv255, Data.H2Colour.A * Inv255, Data.NoiseFade);
		SetPixelShaderValue(GetPixelShader(), AtmosH1H2AlphaParameter, AtmosH1H2Alpha);

		// Global gradient colour and normalized direction.
		FLinearColor GlobalGradientColour(Data.GlobalGradientColour.R*Inv255, Data.GlobalGradientColour.G*Inv255, Data.GlobalGradientColour.B*Inv255, Data.GlobalGradientColour.A*Inv255);
		if (Data.GlobalGradientDensity > 0.0f)
		{
			if (Data.GlobalGradientDensity >= 1.0f)
			{
				GlobalGradientColour *= Data.GlobalGradientDensity;
			}
			else
			{
				GlobalGradientColour.R = (GlobalGradientColour.R - 1.0f) * Data.GlobalGradientDensity + 1.0f;
				GlobalGradientColour.G = (GlobalGradientColour.G - 1.0f) * Data.GlobalGradientDensity + 1.0f;
				GlobalGradientColour.B = (GlobalGradientColour.B - 1.0f) * Data.GlobalGradientDensity + 1.0f;
			}
		}
		SetPixelShaderValue(GetPixelShader(), AtmosGlobalGradientColourParameter, GlobalGradientColour);
		SetPixelShaderValue(GetPixelShader(), AtmosGlobalGradientDirectionParameter, Data.GlobalGradientDirection.SafeNormal());

		SetPixelShaderValue(GetPixelShader(), CameraPosParameter, FVector4((FVector)View.ViewOrigin, 0.0f));

		if (bAllowNoise)
		{
			UTexture2D* NoiseTexture = GEngine->ImageGrainNoiseTexture;
			if (NoiseTexture && NoiseTexture->Resource)
			{
				SetTextureParameter(
					GetPixelShader(),
					AtmosphericNoiseTextureParameter,
					TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
					NoiseTexture->Resource->TextureRHI);
			}

			// xyz = scrolling offset, w = noise scale.
			const FLOAT Time = View.Family ? View.Family->CurrentWorldTime : 0.0f;
			const FVector Scroll = Data.NoiseOffset + Data.NoiseWind * Time;
			SetPixelShaderValue(GetPixelShader(), AtmosNoiseWindParameter, FVector4(Scroll.X, Scroll.Y, Scroll.Z, 0.00025f));

			SetPixelShaderValue(GetPixelShader(), BufferTexelSizeXYParameter,
				FVector2D(1.0f / GSceneRenderTargets.GetBufferSizeX(), 1.0f / GSceneRenderTargets.GetBufferSizeY()));
		}
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("RockAtmosPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar	<< SceneTextureParameters << AtmosDensityParameter << AtmosStartEndParameter
			<< AtmosGradSizeHeightIntegralParameter << AtmosD1ColourParameter << AtmosD2ColourParameter
			<< AtmosH1ColourParameter << AtmosH2ColourParameter << AtmosH1H2AlphaParameter
			<< AtmosGlobalGradientColourParameter << AtmosGlobalGradientDirectionParameter
			<< CameraPosParameter << AtmosphericNoiseTextureParameter << AtmosNoiseWindParameter
			<< BufferTexelSizeXYParameter;
		return bShaderHasOutdatedParameters;
	}
};

// Instantiate all 32 permutations of the atmosphere pixel shader.
#define VARIATION_NOISE(H1,H2,D1,D2,N) \
	typedef TRockAtmosPixelShader<H1,H2,D1,D2,N> TRockAtmosPixelShader##H1##H2##D1##D2##N; \
	IMPLEMENT_SHADER_TYPE2(template<>, TRockAtmosPixelShader##H1##H2##D1##D2##N, SF_Pixel, 0, 0);
#define VARIATION_D2(H1,H2,D1,D2)	VARIATION_NOISE(H1,H2,D1,D2,0)	VARIATION_NOISE(H1,H2,D1,D2,1)
#define VARIATION_D1(H1,H2,D1)		VARIATION_D2(H1,H2,D1,0)		VARIATION_D2(H1,H2,D1,1)
#define VARIATION_H2(H1,H2)			VARIATION_D1(H1,H2,0)			VARIATION_D1(H1,H2,1)
#define VARIATION_H1(H1)			VARIATION_H2(H1,0)				VARIATION_H2(H1,1)
	VARIATION_H1(0) VARIATION_H1(1)
#undef VARIATION_H1
#undef VARIATION_H2
#undef VARIATION_D1
#undef VARIATION_D2
#undef VARIATION_NOISE

/*-----------------------------------------------------------------------------
	FRockAtmosVertexDeclaration - fullscreen quad, float2 position only.
-----------------------------------------------------------------------------*/
class FRockAtmosVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual ~FRockAtmosVertexDeclaration() {}
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
TGlobalResource<FRockAtmosVertexDeclaration> GRockAtmosVertexDeclaration;

/*-----------------------------------------------------------------------------
	FRockAtmosSceneProxy
-----------------------------------------------------------------------------*/
class FRockAtmosSceneProxy : public FPostProcessSceneProxy
{
public:
	FRockAtmosSceneProxy(const URockAtmos* InEffect, const FPostProcessSettings* WorldSettings)
		:	FPostProcessSceneProxy(InEffect)
	{
		#define RESOLVE(Flag,SettingsName,EffectName) \
			((WorldSettings && WorldSettings->Flag) ? WorldSettings->SettingsName : InEffect->EffectName)

		Data.D1Colour			= RESOLVE(bOverride_EnableAtmosD1Col,   AtmosD1_Colour,        AtmosD1_Colour_PP);
		Data.D1Density			= RESOLVE(bOverride_EnableAtmosD1Den,   AtmosD1_Density,       AtmosD1_Density_PP);
		Data.D1DistanceStart	= RESOLVE(bOverride_EnableAtmosD1Start, AtmosD1_DistanceStart, AtmosD1_DistanceStart_PP);
		Data.D1DistanceEnd		= RESOLVE(bOverride_EnableAtmosD1End,   AtmosD1_DistanceEnd,   AtmosD1_DistanceEnd_PP);

		Data.D2Colour			= RESOLVE(bOverride_EnableAtmosD2Col,   AtmosD2_Colour,        AtmosD2_Colour_PP);
		Data.D2Density			= RESOLVE(bOverride_EnableAtmosD2Den,   AtmosD2_Density,       AtmosD2_Density_PP);
		Data.D2DistanceStart	= RESOLVE(bOverride_EnableAtmosD2Start, AtmosD2_DistanceStart, AtmosD2_DistanceStart_PP);
		Data.D2DistanceEnd		= RESOLVE(bOverride_EnableAtmosD2End,   AtmosD2_DistanceEnd,   AtmosD2_DistanceEnd_PP);

		Data.H1Colour			= RESOLVE(bOverride_EnableAtmosH1Col,   AtmosH1_Colour,           AtmosH1_Colour_PP);
		Data.H1Density			= RESOLVE(bOverride_EnableAtmosH1Den,   AtmosH1_Density,          AtmosH1_Density_PP);
		Data.H1GradientSize		= RESOLVE(bOverride_EnableAtmosH1Size,  AtmosH1_GradientSize,     AtmosH1_GradientSize_PP);
		Data.H1GradientPosition	= RESOLVE(bOverride_EnableAtmosH1Pos,   AtmosH1_GradientPosition, AtmosH1_GradientPosition_PP);

		Data.H2Colour			= RESOLVE(bOverride_EnableAtmosH2Col,   AtmosH2_Colour,           AtmosH2_Colour_PP);
		Data.H2Density			= RESOLVE(bOverride_EnableAtmosH2Den,   AtmosH2_Density,          AtmosH2_Density_PP);
		Data.H2GradientSize		= RESOLVE(bOverride_EnableAtmosH2Size,  AtmosH2_GradientSize,     AtmosH2_GradientSize_PP);
		Data.H2GradientPosition	= RESOLVE(bOverride_EnableAtmosH2Pos,   AtmosH2_GradientPosition, AtmosH2_GradientPosition_PP);

		Data.NoiseWind			= RESOLVE(bOverride_EnableAtmosNoiseWind,   AtmosNoiseWind,   AtmosNoiseWind_PP);
		Data.NoiseOffset		= RESOLVE(bOverride_EnableAtmosNoiseOffset, AtmosNoiseOffset, AtmosNoiseOffset_PP);
		Data.NoiseFade			= RESOLVE(bOverride_EnableAtmosNoiseFade,   AtmosNoiseFade,   AtmosNoiseFade_PP);

		Data.GlobalGradientColour		= RESOLVE(bOverride_EnableAtmosGlobal_Gradient_Colour,    AtmosGlobal_Gradient_Colour,    AtmosGlobal_Gradient_Colour_PP);
		Data.GlobalGradientDirection	= RESOLVE(bOverride_EnableAtmosGlobal_Gradient_Direction, AtmosGlobal_Gradient_Direction, AtmosGlobal_Gradient_Direction_PP);
		Data.GlobalGradientDensity		= RESOLVE(bOverride_EnableAtmosGlobal_Gradient_Density,   AtmosGlobal_Gradient_Density,   AtmosGlobal_Gradient_Density_PP);

		#undef RESOLVE

		// A layer contributes only if it has density and opacity; skipping empty layers
		// is identical visually and selects a cheaper shader permutation.
		bAllowD1 = (Data.D1Density > 0.0f) && (Data.D1Colour.A > 0);
		bAllowD2 = (Data.D2Density > 0.0f) && (Data.D2Colour.A > 0);
		bAllowH1 = (Data.H1Density > 0.0f) && (Data.H1Colour.A > 0);
		bAllowH2 = (Data.H2Density > 0.0f) && (Data.H2Colour.A > 0);
		bAllowNoise = (WorldSettings && WorldSettings->bOverride_EnableAtmosNoise) ? (UBOOL)WorldSettings->AtmosNoise : FALSE;
		bAllowNoise = bAllowNoise && (bAllowH1 || bAllowH2);
	}

	template<UBOOL bH1, UBOOL bH2, UBOOL bD1, UBOOL bD2, UBOOL bN>
	void DrawAtmos(FViewInfo& View)
	{
		TShaderMapRef<FRockAtmosVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<TRockAtmosPixelShader<bH1,bH2,bD1,bD2,bN> > PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(BoundShaderState, GRockAtmosVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FVector2D));

		VertexShader->SetParameters(View);
		PixelShader->SetParameters(View, Data);

		static const FVector2D Vertices[4] =
		{
			FVector2D(-1,-1), FVector2D(-1,+1), FVector2D(+1,+1), FVector2D(+1,-1),
		};
		static const WORD Indices[6] = { 0, 1, 2, 0, 2, 3 };

		RHIDrawIndexedPrimitiveUP(PT_TriangleList, 0, ARRAY_COUNT(Vertices), 2, Indices, sizeof(Indices[0]), Vertices, sizeof(Vertices[0]));
	}

	// Selects the shader permutation for the active layers at runtime.
	#define DISPATCH_N(bH1,bH2,bD1,bD2)	if(bAllowNoise) DrawAtmos<bH1,bH2,bD1,bD2,TRUE>(View); else DrawAtmos<bH1,bH2,bD1,bD2,FALSE>(View);
	#define DISPATCH_D2(bH1,bH2,bD1)	if(bAllowD2) { DISPATCH_N(bH1,bH2,bD1,TRUE) } else { DISPATCH_N(bH1,bH2,bD1,FALSE) }
	#define DISPATCH_D1(bH1,bH2)		if(bAllowD1) { DISPATCH_D2(bH1,bH2,TRUE) } else { DISPATCH_D2(bH1,bH2,FALSE) }
	#define DISPATCH_H2(bH1)			if(bAllowH2) { DISPATCH_D1(bH1,TRUE) } else { DISPATCH_D1(bH1,FALSE) }

	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup, FViewInfo& View, const FMatrix& CanvasTransform, FSceneColorLDRInfo& LDRInfo)
	{
		// Nothing enabled - skip entirely.
		if (!bAllowH1 && !bAllowH2 && !bAllowD1 && !bAllowD2)
		{
			return FALSE;
		}

		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockAtmos"));

		GSceneRenderTargets.BeginRenderingSceneColor();

		RHISetViewport(View.RenderTargetX, View.RenderTargetY, 0.0f, View.RenderTargetX + View.RenderTargetSizeX, View.RenderTargetY + View.RenderTargetSizeY, 1.0f);
		RHISetViewParameters(View);

		// No depth test, no culling. Standard alpha blend; preserve scene depth stored in alpha.
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha>::GetRHI());
		RHISetColorWriteMask(CW_RED|CW_GREEN|CW_BLUE);

		if (bAllowH1) { DISPATCH_H2(TRUE) } else { DISPATCH_H2(FALSE) }

		RHISetColorWriteMask(CW_RED|CW_GREEN|CW_BLUE|CW_ALPHA);

		GSceneRenderTargets.FinishRenderingSceneColor(FALSE);
		return TRUE;
	}

	#undef DISPATCH_H2
	#undef DISPATCH_D1
	#undef DISPATCH_D2
	#undef DISPATCH_N

private:
	FRockAtmosData	Data;
	UBOOL			bAllowH1;
	UBOOL			bAllowH2;
	UBOOL			bAllowD1;
	UBOOL			bAllowD2;
	UBOOL			bAllowNoise;
};

/*-----------------------------------------------------------------------------
	URockAtmos
-----------------------------------------------------------------------------*/
FPostProcessSceneProxy* URockAtmos::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	return new FRockAtmosSceneProxy(this, WorldSettings);
}

void URockAtmos::PostLoad()
{
	Super::PostLoad();

	// Atmospheric fog composites into HDR scene color during post processing.
	SceneDPG = SDPG_PostProcess;
}
