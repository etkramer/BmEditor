/*=============================================================================
	RockAtmosEffect.cpp: Batman: Arkham City atmospheric fog post process.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "DOFAndBloomEffect.h"

IMPLEMENT_CLASS(URockAtmos);

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
		ScreenPositionScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("ScreenPositionScaleBias"),FALSE);
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		SetVertexShaderValue(GetVertexShader(),ScreenPositionScaleBiasParameter,View.ScreenPositionScaleBias);

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
	FAtmosphericParameters
-----------------------------------------------------------------------------*/
class FAtmosphericParameters
{
public:
	FAtmosphericParameters() {}

	FAtmosphericParameters(const FShaderParameterMap& ParameterMap)
	{
		AtmosDensityParameter.Bind(ParameterMap,TEXT("AtmosDensity_xD1yD2zH1wH2"),TRUE);
		AtmosStartEndParameter.Bind(ParameterMap,TEXT("AtmosStartEnd_xD1yD1zD2wD2"),TRUE);
		AtmosGradSizeHeightIntegralParameter.Bind(ParameterMap,TEXT("AtmosGradSizeHeightIntegral_xH1yH1zH2wH2"),TRUE);
		AtmosD1ColourParameter.Bind(ParameterMap,TEXT("AtmosD1_Colour"),TRUE);
		AtmosD2ColourParameter.Bind(ParameterMap,TEXT("AtmosD2_Colour"),TRUE);
		AtmosH1H2AlphaParameter.Bind(ParameterMap,TEXT("AtmosH1H2_Alpha"),TRUE);
		AtmosH1ColourParameter.Bind(ParameterMap,TEXT("AtmosH1_Colour"),TRUE);
		AtmosH2ColourParameter.Bind(ParameterMap,TEXT("AtmosH2_Colour"),TRUE);
		CameraPosParameter.Bind(ParameterMap,TEXT("CameraPos"),TRUE);
		AtmosphericNoiseTextureParameter.Bind(ParameterMap,TEXT("AtmosphericNoiseTexture"),TRUE);
		BufferTexelSizeXYParameter.Bind(ParameterMap,TEXT("BufferTexelSizeXY"),TRUE);
		TimeVectorParameter.Bind(ParameterMap,TEXT("TimeVector"),TRUE);
		AtmosNoiseWindParameter.Bind(ParameterMap,TEXT("AtmosNoiseWind"),TRUE);
		AtmosGlobalGradientColourParameter.Bind(ParameterMap,TEXT("AtmosGlobal_Gradient_Colour"),TRUE);
		AtmosGlobalGradientDirectionParameter.Bind(ParameterMap,TEXT("AtmosGlobal_Gradient_Direction"),TRUE);
	}

	void Set(
		FShader* PixelShader,
		const FViewInfo& View,
		const FColor& D1Colour,
		FLOAT D1Density,
		FLOAT D1DistanceStart,
		FLOAT D1DistanceEnd,
		const FColor& D2Colour,
		FLOAT D2Density,
		FLOAT D2DistanceStart,
		FLOAT D2DistanceEnd,
		const FColor& H1Colour,
		FLOAT H1Density,
		FLOAT H1GradientSize,
		FLOAT H1GradientPosition,
		const FColor& H2Colour,
		FLOAT H2Density,
		FLOAT H2GradientSize,
		FLOAT H2GradientPosition,
		const FVector& NoiseWind,
		const FVector& NoiseOffset,
		FLOAT NoiseFade,
		const FColor& GlobalGradientColour,
		const FVector& GlobalGradientDirection,
		FLOAT GlobalGradientDensity) const
	{
		const FPixelShaderRHIRef PixelShaderRHI = PixelShader->GetPixelShader();
		const FLOAT CameraZ = View.ViewOrigin.Z;
		const FLOAT Inv255 = 1.0f / 255.0f;

		SetPixelShaderValue(PixelShaderRHI,CameraPosParameter,FVector4((FVector)View.ViewOrigin,0.0f));

		UTexture2D* NoiseTexture = GEngine->ImageGrainNoiseTexture;
		if (!NoiseTexture)
		{
			NoiseTexture = GEngine->ScreenDoorNoiseTexture;
		}
		if (NoiseTexture && NoiseTexture->Resource)
		{
			SetTextureParameter(
				PixelShaderRHI,
				AtmosphericNoiseTextureParameter,
				TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				NoiseTexture->Resource->TextureRHI);
		}

		SetPixelShaderValue(
			PixelShaderRHI,
			AtmosDensityParameter,
			FVector4(
				(D1Colour.A * Inv255) * D1Density,
				(D2Colour.A * Inv255) * D2Density,
				H1Density * 1.0e-6f,
				H2Density * 1.0e-6f));

		SetPixelShaderValue(
			PixelShaderRHI,
			AtmosStartEndParameter,
			FVector4(
				D1DistanceStart,
				1.0f / (D1DistanceEnd - D1DistanceStart),
				D2DistanceStart,
				1.0f / (D2DistanceEnd - D2DistanceStart)));

		SetPixelShaderValue(
			PixelShaderRHI,
			AtmosGradSizeHeightIntegralParameter,
			FVector4(
				1.0f / H1GradientSize,
				appExp(Clamp((H1GradientPosition - CameraZ) / H1GradientSize,-50.0f,50.0f)) * H1Density * 1.0e-6f,
				1.0f / H2GradientSize,
				appExp(Clamp((H2GradientPosition - CameraZ) / H2GradientSize,-50.0f,50.0f)) * H2Density * 1.0e-6f));

		SetPixelShaderValue(PixelShaderRHI,AtmosD1ColourParameter,FLinearColor(D1Colour.R * Inv255,D1Colour.G * Inv255,D1Colour.B * Inv255,D1Colour.A * Inv255));
		SetPixelShaderValue(PixelShaderRHI,AtmosD2ColourParameter,FLinearColor(D2Colour.R * Inv255,D2Colour.G * Inv255,D2Colour.B * Inv255,D2Colour.A * Inv255));
		SetPixelShaderValue(PixelShaderRHI,AtmosH1ColourParameter,FLinearColor(H1Colour.R * Inv255,H1Colour.G * Inv255,H1Colour.B * Inv255,H1Colour.A * Inv255));
		SetPixelShaderValue(PixelShaderRHI,AtmosH2ColourParameter,FLinearColor(H2Colour.R * Inv255,H2Colour.G * Inv255,H2Colour.B * Inv255,H2Colour.A * Inv255));
		SetPixelShaderValue(PixelShaderRHI,AtmosH1H2AlphaParameter,FVector(H1Colour.A * Inv255,H2Colour.A * Inv255,NoiseFade));

		FLinearColor GradientColour(GlobalGradientColour.R * Inv255,GlobalGradientColour.G * Inv255,GlobalGradientColour.B * Inv255,GlobalGradientColour.A * Inv255);
		if (GlobalGradientDensity > 0.0f)
		{
			if (GlobalGradientDensity >= 1.0f)
			{
				GradientColour *= GlobalGradientDensity;
			}
			else
			{
				GradientColour.R = (GradientColour.R - 1.0f) * GlobalGradientDensity + 1.0f;
				GradientColour.G = (GradientColour.G - 1.0f) * GlobalGradientDensity + 1.0f;
				GradientColour.B = (GradientColour.B - 1.0f) * GlobalGradientDensity + 1.0f;
			}
		}
		SetPixelShaderValue(PixelShaderRHI,AtmosGlobalGradientColourParameter,GradientColour);
		SetPixelShaderValue(PixelShaderRHI,AtmosGlobalGradientDirectionParameter,GlobalGradientDirection.SafeNormal());

		const FLOAT Time = View.Family ? View.Family->CurrentWorldTime : 0.0f;
		const FVector Scroll = NoiseOffset + NoiseWind * Time;
		SetPixelShaderValue(PixelShaderRHI,TimeVectorParameter,FVector4(Time,Time,Time,Time));
		SetPixelShaderValue(PixelShaderRHI,AtmosNoiseWindParameter,FVector4(Scroll.X,Scroll.Y,Scroll.Z,0.00025f));
		SetPixelShaderValue(PixelShaderRHI,BufferTexelSizeXYParameter,FVector2D(1.0f / GSceneRenderTargets.GetBufferSizeX(),1.0f / GSceneRenderTargets.GetBufferSizeY()));
	}

	friend FArchive& operator<<(FArchive& Ar,FAtmosphericParameters& Parameters)
	{
		Ar	<< Parameters.AtmosDensityParameter
			<< Parameters.AtmosStartEndParameter
			<< Parameters.AtmosGradSizeHeightIntegralParameter
			<< Parameters.AtmosD1ColourParameter
			<< Parameters.AtmosD2ColourParameter
			<< Parameters.AtmosH1H2AlphaParameter
			<< Parameters.AtmosH1ColourParameter
			<< Parameters.AtmosH2ColourParameter
			<< Parameters.CameraPosParameter
			<< Parameters.AtmosphericNoiseTextureParameter
			<< Parameters.BufferTexelSizeXYParameter
			<< Parameters.TimeVectorParameter
			<< Parameters.AtmosNoiseWindParameter
			<< Parameters.AtmosGlobalGradientColourParameter
			<< Parameters.AtmosGlobalGradientDirectionParameter;
		return Ar;
	}

private:
	FShaderParameter AtmosDensityParameter;
	FShaderParameter AtmosStartEndParameter;
	FShaderParameter AtmosGradSizeHeightIntegralParameter;
	FShaderParameter AtmosD1ColourParameter;
	FShaderParameter AtmosD2ColourParameter;
	FShaderParameter AtmosH1H2AlphaParameter;
	FShaderParameter AtmosH1ColourParameter;
	FShaderParameter AtmosH2ColourParameter;
	FShaderParameter CameraPosParameter;
	FShaderResourceParameter AtmosphericNoiseTextureParameter;
	FShaderParameter BufferTexelSizeXYParameter;
	FShaderParameter TimeVectorParameter;
	FShaderParameter AtmosNoiseWindParameter;
	FShaderParameter AtmosGlobalGradientColourParameter;
	FShaderParameter AtmosGlobalGradientDirectionParameter;
};

/*-----------------------------------------------------------------------------
	FRockAtmosBlendPixelShader
-----------------------------------------------------------------------------*/
template<UBOOL bAllowH1, UBOOL bAllowH2, UBOOL bAllowD1, UBOOL bAllowD2, UBOOL bAllowNoise>
class FRockAtmosBlendPixelShader : public FDOFAndBloomBlendPixelShader
{
	DECLARE_SHADER_TYPE(FRockAtmosBlendPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosNoise"),bAllowNoise ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosH1"),bAllowH1 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosH2"),bAllowH2 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosD1"),bAllowD1 ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ALLOW_AtmosD2"),bAllowD2 ? TEXT("1") : TEXT("0"));
	}

public:
	FAtmosphericParameters AtmosphericParameters;

	FRockAtmosBlendPixelShader() {}

	FRockAtmosBlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FDOFAndBloomBlendPixelShader(Initializer)
		,	AtmosphericParameters(Initializer.ParameterMap)
	{
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
		UBOOL bShaderHasOutdatedParameters = FDOFAndBloomBlendPixelShader::Serialize(Ar);
		Ar << AtmosphericParameters;
		return bShaderHasOutdatedParameters;
	}
};

#define IMPLEMENT_ROCKATMOS_PIXEL_SHADER(H1,H2,D1,D2,N) \
	typedef FRockAtmosBlendPixelShader<H1,H2,D1,D2,N> FRockAtmosBlendPixelShader##H1##H2##D1##D2##N; \
	IMPLEMENT_SHADER_TYPE2(template<>, FRockAtmosBlendPixelShader##H1##H2##D1##D2##N, SF_Pixel, 0, 0);
#define IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D2(H1,H2,D1,D2)	IMPLEMENT_ROCKATMOS_PIXEL_SHADER(H1,H2,D1,D2,0) IMPLEMENT_ROCKATMOS_PIXEL_SHADER(H1,H2,D1,D2,1)
#define IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D1(H1,H2,D1)		IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D2(H1,H2,D1,0) IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D2(H1,H2,D1,1)
#define IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H2(H1,H2)			IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D1(H1,H2,0) IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D1(H1,H2,1)
#define IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H1(H1)				IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H2(H1,0) IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H2(H1,1)
IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H1(0)
IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H1(1)
#undef IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H1
#undef IMPLEMENT_ROCKATMOS_PIXEL_SHADER_H2
#undef IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D1
#undef IMPLEMENT_ROCKATMOS_PIXEL_SHADER_D2
#undef IMPLEMENT_ROCKATMOS_PIXEL_SHADER

/*-----------------------------------------------------------------------------
	FAtmosVertexDeclaration
-----------------------------------------------------------------------------*/
class FAtmosVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FAtmosVertexDeclaration() {}

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
TGlobalResource<FAtmosVertexDeclaration> GAtmosVertexDeclaration;

/*-----------------------------------------------------------------------------
	FRockAtmosSceneProxy
-----------------------------------------------------------------------------*/
template<UBOOL bAllowH1, UBOOL bAllowH2, UBOOL bAllowD1, UBOOL bAllowD2, UBOOL bAllowNoise>
class FRockAtmosSceneProxy : public FDOFAndBloomPostProcessSceneProxy
{
public:
	FRockAtmosSceneProxy(const URockAtmos* InEffect,const FPostProcessSettings* WorldSettings)
		:	FDOFAndBloomPostProcessSceneProxy(InEffect,WorldSettings)
	{
		if (WorldSettings)
		{
			AtmosD1Colour = WorldSettings->AtmosD1_Colour;
			AtmosD1Density = WorldSettings->AtmosD1_Density;
			AtmosD1DistanceStart = WorldSettings->AtmosD1_DistanceStart;
			AtmosD1DistanceEnd = WorldSettings->AtmosD1_DistanceEnd;
			AtmosD2Colour = WorldSettings->AtmosD2_Colour;
			AtmosD2Density = WorldSettings->AtmosD2_Density;
			AtmosD2DistanceStart = WorldSettings->AtmosD2_DistanceStart;
			AtmosD2DistanceEnd = WorldSettings->AtmosD2_DistanceEnd;
			AtmosH1Colour = WorldSettings->AtmosH1_Colour;
			AtmosH1Density = WorldSettings->AtmosH1_Density;
			AtmosH1GradientSize = WorldSettings->AtmosH1_GradientSize;
			AtmosH1GradientPosition = WorldSettings->AtmosH1_GradientPosition;
			AtmosH2Colour = WorldSettings->AtmosH2_Colour;
			AtmosH2Density = WorldSettings->AtmosH2_Density;
			AtmosH2GradientSize = WorldSettings->AtmosH2_GradientSize;
			AtmosH2GradientPosition = WorldSettings->AtmosH2_GradientPosition;
			AtmosNoiseWind = WorldSettings->AtmosNoiseWind;
			AtmosNoiseOffset = WorldSettings->AtmosNoiseOffset;
			AtmosNoiseFade = WorldSettings->AtmosNoiseFade;
			AtmosGlobalGradientColour = WorldSettings->AtmosGlobal_Gradient_Colour;
			AtmosGlobalGradientDirection = WorldSettings->AtmosGlobal_Gradient_Direction;
			AtmosGlobalGradientDensity = WorldSettings->AtmosGlobal_Gradient_Density;
		}
		else
		{
			AtmosD1Colour = InEffect->AtmosD1_Colour_PP;
			AtmosD1Density = InEffect->AtmosD1_Density_PP;
			AtmosD1DistanceStart = InEffect->AtmosD1_DistanceStart_PP;
			AtmosD1DistanceEnd = InEffect->AtmosD1_DistanceEnd_PP;
			AtmosD2Colour = InEffect->AtmosD2_Colour_PP;
			AtmosD2Density = InEffect->AtmosD2_Density_PP;
			AtmosD2DistanceStart = InEffect->AtmosD2_DistanceStart_PP;
			AtmosD2DistanceEnd = InEffect->AtmosD2_DistanceEnd_PP;
			AtmosH1Colour = InEffect->AtmosH1_Colour_PP;
			AtmosH1Density = InEffect->AtmosH1_Density_PP;
			AtmosH1GradientSize = InEffect->AtmosH1_GradientSize_PP;
			AtmosH1GradientPosition = InEffect->AtmosH1_GradientPosition_PP;
			AtmosH2Colour = InEffect->AtmosH2_Colour_PP;
			AtmosH2Density = InEffect->AtmosH2_Density_PP;
			AtmosH2GradientSize = InEffect->AtmosH2_GradientSize_PP;
			AtmosH2GradientPosition = InEffect->AtmosH2_GradientPosition_PP;
			AtmosNoiseWind = InEffect->AtmosNoiseWind_PP;
			AtmosNoiseOffset = InEffect->AtmosNoiseOffset_PP;
			AtmosNoiseFade = InEffect->AtmosNoiseFade_PP;
			AtmosGlobalGradientColour = InEffect->AtmosGlobal_Gradient_Colour_PP;
			AtmosGlobalGradientDirection = InEffect->AtmosGlobal_Gradient_Direction_PP;
			AtmosGlobalGradientDensity = InEffect->AtmosGlobal_Gradient_Density_PP;
		}
	}

	virtual UBOOL Render(const FScene* Scene,UINT InDepthPriorityGroup,FViewInfo& View,const FMatrix& CanvasTransform,FSceneColorLDRInfo& LDRInfo)
	{
		check(!View.bUseLDRSceneColor);

		if (!bAllowH1 && !bAllowH2 && !bAllowD1 && !bAllowD2)
		{
			return TRUE;
		}

		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockAtmos %d Depth|%d Height|Noise %s"),
			(INT)bAllowD1 + (INT)bAllowD2,
			(INT)bAllowH1 + (INT)bAllowH2,
			bAllowNoise ? TEXT("ON") : TEXT("OFF"));

		GSceneRenderTargets.BeginRenderingSceneColor();

		RHISetViewport(
			View.RenderTargetX,
			View.RenderTargetY,
			0.0f,
			View.RenderTargetX + View.RenderTargetSizeX,
			View.RenderTargetY + View.RenderTargetSizeY,
			1.0f);
		RHISetViewParameters(View);
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<BO_Add,BF_SourceAlpha,BF_InverseSourceAlpha,BO_Add,BF_Zero,BF_One>::GetRHI());

		TShaderMapRef<FRockAtmosVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FRockAtmosBlendPixelShader<bAllowH1,bAllowH2,bAllowD1,bAllowD2,bAllowNoise> > PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(BoundShaderState,GAtmosVertexDeclaration.VertexDeclarationRHI,*VertexShader,*PixelShader,sizeof(FVector2D));

		VertexShader->SetParameters(View);
		PixelShader->SceneTextureParameters.Set(&View,*PixelShader,SF_Point);
		PixelShader->AtmosphericParameters.Set(
			*PixelShader,
			View,
			AtmosD1Colour,
			AtmosD1Density,
			AtmosD1DistanceStart,
			AtmosD1DistanceEnd,
			AtmosD2Colour,
			AtmosD2Density,
			AtmosD2DistanceStart,
			AtmosD2DistanceEnd,
			AtmosH1Colour,
			AtmosH1Density,
			AtmosH1GradientSize,
			AtmosH1GradientPosition,
			AtmosH2Colour,
			AtmosH2Density,
			AtmosH2GradientSize,
			AtmosH2GradientPosition,
			AtmosNoiseWind,
			AtmosNoiseOffset,
			AtmosNoiseFade,
			AtmosGlobalGradientColour,
			AtmosGlobalGradientDirection,
			AtmosGlobalGradientDensity);

		static const FVector2D Vertices[4] =
		{
			FVector2D(-1,-1),
			FVector2D(-1,+1),
			FVector2D(+1,+1),
			FVector2D(+1,-1),
		};
		static const WORD Indices[6] = { 0,1,2,0,2,3 };

		RHIDrawIndexedPrimitiveUP(PT_TriangleList,0,ARRAY_COUNT(Vertices),2,Indices,sizeof(Indices[0]),Vertices,sizeof(Vertices[0]));

		GSceneRenderTargets.FinishRenderingSceneColor(TRUE);
		RHISetBlendState(TStaticBlendState<>::GetRHI());
		return TRUE;
	}

	virtual UBOOL MayRenderSceneColorLDR() const
	{
		return TRUE;
	}

private:
	FColor AtmosD1Colour;
	FLOAT AtmosD1Density;
	FLOAT AtmosD1DistanceStart;
	FLOAT AtmosD1DistanceEnd;
	FColor AtmosD2Colour;
	FLOAT AtmosD2Density;
	FLOAT AtmosD2DistanceStart;
	FLOAT AtmosD2DistanceEnd;
	FColor AtmosH1Colour;
	FLOAT AtmosH1Density;
	FLOAT AtmosH1GradientSize;
	FLOAT AtmosH1GradientPosition;
	FColor AtmosH2Colour;
	FLOAT AtmosH2Density;
	FLOAT AtmosH2GradientSize;
	FLOAT AtmosH2GradientPosition;
	FVector AtmosNoiseWind;
	FVector AtmosNoiseOffset;
	FLOAT AtmosNoiseFade;
	FColor AtmosGlobalGradientColour;
	FVector AtmosGlobalGradientDirection;
	FLOAT AtmosGlobalGradientDensity;
};

/*-----------------------------------------------------------------------------
	URockAtmos
-----------------------------------------------------------------------------*/
FPostProcessSceneProxy* URockAtmos::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	const UBOOL bH1 = WorldSettings && WorldSettings->bAtmosH1;
	const UBOOL bH2 = WorldSettings && WorldSettings->bAtmosH2;
	const UBOOL bD1 = WorldSettings && WorldSettings->bAtmosD1;
	const UBOOL bD2 = WorldSettings && WorldSettings->bAtmosD2;
	const UBOOL bNoise = WorldSettings && WorldSettings->AtmosNoise;

#define RETURN_ROCKATMOS_PROXY(H1,H2,D1,D2,N) \
	if (bH1 == H1 && bH2 == H2 && bD1 == D1 && bD2 == D2 && bNoise == N) \
	{ \
		return new FRockAtmosSceneProxy<H1,H2,D1,D2,N>(this,WorldSettings); \
	}
#define RETURN_ROCKATMOS_PROXY_D2(H1,H2,D1,D2) RETURN_ROCKATMOS_PROXY(H1,H2,D1,D2,0) RETURN_ROCKATMOS_PROXY(H1,H2,D1,D2,1)
#define RETURN_ROCKATMOS_PROXY_D1(H1,H2,D1) RETURN_ROCKATMOS_PROXY_D2(H1,H2,D1,0) RETURN_ROCKATMOS_PROXY_D2(H1,H2,D1,1)
#define RETURN_ROCKATMOS_PROXY_H2(H1,H2) RETURN_ROCKATMOS_PROXY_D1(H1,H2,0) RETURN_ROCKATMOS_PROXY_D1(H1,H2,1)
#define RETURN_ROCKATMOS_PROXY_H1(H1) RETURN_ROCKATMOS_PROXY_H2(H1,0) RETURN_ROCKATMOS_PROXY_H2(H1,1)
	RETURN_ROCKATMOS_PROXY_H1(0)
	RETURN_ROCKATMOS_PROXY_H1(1)
#undef RETURN_ROCKATMOS_PROXY_H1
#undef RETURN_ROCKATMOS_PROXY_H2
#undef RETURN_ROCKATMOS_PROXY_D1
#undef RETURN_ROCKATMOS_PROXY_D2
#undef RETURN_ROCKATMOS_PROXY

	return new FRockAtmosSceneProxy<0,0,0,0,0>(this,WorldSettings);
}

void URockAtmos::PostLoad()
{
	Super::PostLoad();
	SceneDPG = SDPG_PostProcess;
}
