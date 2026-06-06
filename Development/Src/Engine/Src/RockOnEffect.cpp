/*=============================================================================
	RockOnEffect.cpp: Batman: Arkham City uber post process (RockOn).

	RockOn replaces UUberPostProcessEffect as the HDR->LDR pass. It performs DOF,
	bloom, motion blur, tone mapping, LUT color grading and image grain in a
	single fixed pipeline. Unlike Uber it does NOT do atmospherics - its blend
	shader does not include RockAtmosCommon.usf - so no Atmos parameters appear
	here. Ported from BM2 ("Rock") to match the original game.

	The half-res producer path uses the original RockOn half-downsample, bloom
	filter, and DOF anti-dither shaders so the final blend shader consumes the same
	BlurredImage/LightShaftBuffer format as BM2/Gangland.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "UnMotionBlurEffect.h"
#include "BokehDOF.h"

IMPLEMENT_CLASS(URockOn);

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/*-----------------------------------------------------------------------------
	FRockOnVertexShader - final blend vertex shader (RockOnVertexShader.usf "Main")
-----------------------------------------------------------------------------*/
template<UBOOL bImageGrain>
class FRockOnVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("USE_IMAGEGRAIN"), bImageGrain ? TEXT("1") : TEXT("0"));
	}

	FRockOnVertexShader() {}

	FRockOnVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneCoordinateScaleBiasParameter.Bind(Initializer.ParameterMap,TEXT("SceneCoordinateScaleBias"),TRUE);
		FilmGrainScaleOffsetParameter.Bind(Initializer.ParameterMap,TEXT("FilmGrainScaleOffset"),TRUE);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("RockOnVertexShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneCoordinateScaleBiasParameter << FilmGrainScaleOffsetParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	FShaderParameter SceneCoordinateScaleBiasParameter;
	FShaderParameter FilmGrainScaleOffsetParameter;
};

#define VARIATION_VS(B) typedef FRockOnVertexShader<B> FRockOnVertexShader##B; \
	IMPLEMENT_SHADER_TYPE2(template<>, FRockOnVertexShader##B, SF_Vertex, 0, 0);
	VARIATION_VS(0) VARIATION_VS(1)
#undef VARIATION_VS

/*-----------------------------------------------------------------------------
	FRockOnBlendPixelShader - the final blend (RockOnBlendPixelShader.usf "Main")
-----------------------------------------------------------------------------*/
template<UBOOL bHighQualityDOF, UBOOL bImageGrain, UBOOL bMotionBlur>
class FRockOnBlendPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnBlendPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform)
	{
		return TRUE;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("USE_HIGHQUALITYDOF"), bHighQualityDOF ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("USE_IMAGEGRAIN"), bImageGrain ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("MOTION_BLUR"), bMotionBlur ? TEXT("1") : TEXT("0"));
	}

	FRockOnBlendPixelShader() {}

	FRockOnBlendPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
		,	DOFParameters(Initializer.ParameterMap)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		BlurredImageParameter.Bind(Initializer.ParameterMap,TEXT("BlurredImage"),TRUE);
		LightShaftBufferParameter.Bind(Initializer.ParameterMap,TEXT("LightShaftBuffer"),TRUE);
		BloomTintAndScreenBlendThresholdParameter.Bind(Initializer.ParameterMap,TEXT("BloomTintAndScreenBlendThreshold"),TRUE);
		BufferTexelSizeXYParameter.Bind(Initializer.ParameterMap,TEXT("BufferTexelSizeXY"),TRUE);
		ColorGradingLUTParameter.Bind(Initializer.ParameterMap,TEXT("ColorGradingLUT"),TRUE);
		PostProcessNoiseTextureParameter.Bind(Initializer.ParameterMap,TEXT("PostProcessNoiseTexture"),TRUE);
		FilmGrainParameter.Bind(Initializer.ParameterMap,TEXT("FilmGrain"),TRUE);
		VelocityBufferParameter.Bind(Initializer.ParameterMap,TEXT("VelocityBuffer"),TRUE);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("RockOnBlendPixelShader");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar	<< DOFParameters << SceneTextureParameters << BlurredImageParameter
			<< LightShaftBufferParameter << BloomTintAndScreenBlendThresholdParameter
			<< BufferTexelSizeXYParameter << ColorGradingLUTParameter
			<< PostProcessNoiseTextureParameter << FilmGrainParameter << VelocityBufferParameter;

#if NGP
		// Hard-coded to match the shader (it's serialized based on the PC shader).
		ColorGradingLUTParameter.SetBaseIndex( 2 );
#endif
		return bShaderHasOutdatedParameters;
	}

public:
	FDOFShaderParameters		DOFParameters;
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderResourceParameter	BlurredImageParameter;
	FShaderResourceParameter	LightShaftBufferParameter;
	FShaderParameter			BloomTintAndScreenBlendThresholdParameter;
	FShaderParameter			BufferTexelSizeXYParameter;
	FShaderResourceParameter	ColorGradingLUTParameter;
	FShaderResourceParameter	PostProcessNoiseTextureParameter;
	FShaderParameter			FilmGrainParameter;
	FShaderResourceParameter	VelocityBufferParameter;
};

// 8 permutations: bHighQualityDOF x bImageGrain x bMotionBlur
#define VARIATION_PS3(A,B,C)	typedef FRockOnBlendPixelShader<A,B,C> FRockOnBlendPixelShader##A##B##C; \
	IMPLEMENT_SHADER_TYPE2(template<>, FRockOnBlendPixelShader##A##B##C, SF_Pixel, 0, 0);
#define VARIATION_PS2(A,B)		VARIATION_PS3(A,B,0) VARIATION_PS3(A,B,1)
#define VARIATION_PS1(A)		VARIATION_PS2(A,0) VARIATION_PS2(A,1)
	VARIATION_PS1(0) VARIATION_PS1(1)
#undef VARIATION_PS1
#undef VARIATION_PS2
#undef VARIATION_PS3

/*-----------------------------------------------------------------------------
	RockOn gather shaders (RockOnGatherVertexShader.usf / RockOnGatherPixelShader.usf)

	Produce the half-res post-process buffer (BlurredImage: rgb=color/4, a=DOF
	mask) and the bloom gather source. Three pixel entries (separate classes) and
	two vertex entries.
-----------------------------------------------------------------------------*/

// Vertex: "Main" (used by the bloom/DOF gather)
class FRockOnGatherVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnGatherVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FRockOnGatherVertexShader() {}
	FRockOnGatherVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		dUVOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("dUVOffsets"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnGatherVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("Main"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << dUVOffsetsParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	FShaderParameter dUVOffsetsParameter;
};
IMPLEMENT_SHADER_TYPE(,FRockOnGatherVertexShader,TEXT("RockOnGatherVertexShader"),TEXT("Main"),SF_Vertex,0,0);

// Vertex: "MainHalfDownsample"
class FRockOnHalfDownsampleVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnHalfDownsampleVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FRockOnHalfDownsampleVertexShader() {}
	FRockOnHalfDownsampleVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		dUVOffsetsParameter.Bind(Initializer.ParameterMap,TEXT("dUVOffsets"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnGatherVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("MainHalfDownsample"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << dUVOffsetsParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	FShaderParameter dUVOffsetsParameter;
};
IMPLEMENT_SHADER_TYPE(,FRockOnHalfDownsampleVertexShader,TEXT("RockOnGatherVertexShader"),TEXT("MainHalfDownsample"),SF_Vertex,0,0);

// Pixel base: shared parameters for the three gather pixel entries.
class FRockOnGatherPixelShaderBase : public FGlobalShader
{
public:
	FRockOnGatherPixelShaderBase() {}
	FRockOnGatherPixelShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
		,	DOFParameters(Initializer.ParameterMap)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		SceneColorTextureParameter.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"),TRUE);
		BufferTexelSizeXYParameter.Bind(Initializer.ParameterMap,TEXT("BufferTexelSizeXY"),TRUE);
	}

	void SetParameters(const FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams)
	{
		SceneTextureParameters.Set(&View, this, SF_Point);
		DOFParameters.SetPS(this, DepthOfFieldParams);

		const FLOAT InvX = 1.0f / GSceneRenderTargets.GetBufferSizeX();
		const FLOAT InvY = 1.0f / GSceneRenderTargets.GetBufferSizeY();
		SetPixelShaderValue(GetPixelShader(), BufferTexelSizeXYParameter, FVector4(InvX, InvY, InvX, InvY));
	}

	void SetParameters(const FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams, const FTexture2DRHIRef& SourceTexture)
	{
		SetParameters(View, DepthOfFieldParams);
		SetTextureParameterDirectly(
			GetPixelShader(),
			SceneColorTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			SourceTexture);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DOFParameters << SceneTextureParameters << SceneColorTextureParameter << BufferTexelSizeXYParameter;
		return bShaderHasOutdatedParameters;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }
	static const TCHAR* GetSourceFilename() { return TEXT("RockOnGatherPixelShader"); }

	FDOFShaderParameters			DOFParameters;
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderResourceParameter		SceneColorTextureParameter;
	FShaderParameter				BufferTexelSizeXYParameter;
};

// Pixel: "MainHalfDownsample"
class FRockOnHalfDownsamplePixelShader : public FRockOnGatherPixelShaderBase
{
	DECLARE_SHADER_TYPE(FRockOnHalfDownsamplePixelShader,Global);
public:
	FRockOnHalfDownsamplePixelShader() {}
	FRockOnHalfDownsamplePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FRockOnGatherPixelShaderBase(Initializer) {}
	static const TCHAR* GetFunctionName() { return TEXT("MainHalfDownsample"); }
};
IMPLEMENT_SHADER_TYPE(,FRockOnHalfDownsamplePixelShader,TEXT("RockOnGatherPixelShader"),TEXT("MainHalfDownsample"),SF_Pixel,0,0);

// Pixel: "MainDownSampleByHalfAndDOF"
class FRockOnHalfsizeDownsampleAndDOFPixelShader : public FRockOnGatherPixelShaderBase
{
	DECLARE_SHADER_TYPE(FRockOnHalfsizeDownsampleAndDOFPixelShader,Global);
public:
	FRockOnHalfsizeDownsampleAndDOFPixelShader() {}
	FRockOnHalfsizeDownsampleAndDOFPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FRockOnGatherPixelShaderBase(Initializer) {}
	static const TCHAR* GetFunctionName() { return TEXT("MainDownSampleByHalfAndDOF"); }
};
IMPLEMENT_SHADER_TYPE(,FRockOnHalfsizeDownsampleAndDOFPixelShader,TEXT("RockOnGatherPixelShader"),TEXT("MainDownSampleByHalfAndDOF"),SF_Pixel,0,0);

// Pixel: "MainDOFAndBloom"
class FRockOnGatherDOFAndBloomPixelShader : public FRockOnGatherPixelShaderBase
{
	DECLARE_SHADER_TYPE(FRockOnGatherDOFAndBloomPixelShader,Global);
public:
	FRockOnGatherDOFAndBloomPixelShader() {}
	FRockOnGatherDOFAndBloomPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FRockOnGatherPixelShaderBase(Initializer) {}
	static const TCHAR* GetFunctionName() { return TEXT("MainDOFAndBloom"); }
};
IMPLEMENT_SHADER_TYPE(,FRockOnGatherDOFAndBloomPixelShader,TEXT("RockOnGatherPixelShader"),TEXT("MainDOFAndBloom"),SF_Pixel,0,0);

/*-----------------------------------------------------------------------------
	RockOn bloom filter shaders (RockOnFilterVertexShader.usf / RockOnFilterPixelShader.usf)
	Gaussian bloom blur, entry "MainBloom".
-----------------------------------------------------------------------------*/
class FRockOnFilterVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnFilterVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FRockOnFilterVertexShader() {}
	FRockOnFilterVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SampleKernelParameter.Bind(Initializer.ParameterMap,TEXT("SampleKernel"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnFilterVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("MainBloom"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SampleKernelParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	void SetParameters(const FVector2D& SampleKernel)
	{
		SetVertexShaderValue(GetVertexShader(), SampleKernelParameter, SampleKernel);
	}

	FShaderParameter SampleKernelParameter;
};
IMPLEMENT_SHADER_TYPE(,FRockOnFilterVertexShader,TEXT("RockOnFilterVertexShader"),TEXT("MainBloom"),SF_Vertex,0,0);

template<UBOOL bInitialBloomPass, UBOOL bFinalBloomPass>
class FRockOnFilterPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnFilterPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("ISINITIALBLOOMPASS"), bInitialBloomPass ? TEXT("1") : TEXT("0"));
		OutEnvironment.Definitions.Set(TEXT("ISFINALBLOOMPASS"), bFinalBloomPass ? TEXT("1") : TEXT("0"));
	}

	FRockOnFilterPixelShader() {}
	FRockOnFilterPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		FilterTextureParameter.Bind(Initializer.ParameterMap,TEXT("FilterTexture"),TRUE);
		LightShaftsTextureParameter.Bind(Initializer.ParameterMap,TEXT("LightShaftsTexture"),TRUE);
		SampleMaskRectParameter.Bind(Initializer.ParameterMap,TEXT("SampleMaskRect"),TRUE);
		BloomTintAndScreenBlendThresholdParameter.Bind(Initializer.ParameterMap,TEXT("BloomTintAndScreenBlendThreshold"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnFilterPixelShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("MainBloom"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FilterTextureParameter << LightShaftsTextureParameter << SampleMaskRectParameter << BloomTintAndScreenBlendThresholdParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	void SetParameters(
		FTextureRHIParamRef FilterTexture,
		FTextureRHIParamRef LightShaftsTexture,
		const FVector2D& SampleMaskMin,
		const FVector2D& SampleMaskMax,
		const FVector4& BloomTintAndScreenBlendThreshold)
	{
		SetTextureParameterDirectly(
			GetPixelShader(),
			FilterTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FilterTexture);

		SetTextureParameterDirectly(
			GetPixelShader(),
			LightShaftsTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			LightShaftsTexture);

		SetPixelShaderValue(
			GetPixelShader(),
			SampleMaskRectParameter,
			FVector4(SampleMaskMin.X, SampleMaskMin.Y, SampleMaskMax.X, SampleMaskMax.Y));

		SetPixelShaderValue(
			GetPixelShader(),
			BloomTintAndScreenBlendThresholdParameter,
			BloomTintAndScreenBlendThreshold);
	}

	FShaderResourceParameter	FilterTextureParameter;
	FShaderResourceParameter	LightShaftsTextureParameter;
	FShaderParameter			SampleMaskRectParameter;
	FShaderParameter			BloomTintAndScreenBlendThresholdParameter;
};

#define VARIATION_FILTER(A,B)	typedef FRockOnFilterPixelShader<A,B> FRockOnFilterPixelShader##A##B; \
	IMPLEMENT_SHADER_TYPE2(template<>, FRockOnFilterPixelShader##A##B, SF_Pixel, 0, 0);
	VARIATION_FILTER(0,0) VARIATION_FILTER(0,1) VARIATION_FILTER(1,0) VARIATION_FILTER(1,1)
#undef VARIATION_FILTER

/*-----------------------------------------------------------------------------
	RockOn DOF anti-dither filter (RockOnDOFFilterVertexShader.usf / RockOnDOFFilterPixelShader.usf)
	entry "MainDOF", templated on NumSamples (4 or 17).
-----------------------------------------------------------------------------*/
class FRockOnDOFFilterVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRockOnDOFFilterVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FRockOnDOFFilterVertexShader() {}
	FRockOnDOFFilterVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer) {}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnDOFFilterVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("MainDOF"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FGlobalShader::Serialize(Ar);
	}
};
IMPLEMENT_SHADER_TYPE(,FRockOnDOFFilterVertexShader,TEXT("RockOnDOFFilterVertexShader"),TEXT("MainDOF"),SF_Vertex,0,0);

template<UINT NumSamples>
class TRockOnDOFFilterPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockOnDOFFilterPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("NUM_SAMPLES"),*FString::Printf(TEXT("%u"),NumSamples));
		new(OutEnvironment.CompilerFlags) ECompilerFlags(CFLAG_AvoidFlowControl);
	}

	TRockOnDOFFilterPixelShader() {}
	TRockOnDOFFilterPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		FilterTextureParameter.Bind(Initializer.ParameterMap,TEXT("FilterTexture"),TRUE);
		VelocityTextureParameter.Bind(Initializer.ParameterMap,TEXT("VelocityTexture"),TRUE);
		DOFOffsetParameter.Bind(Initializer.ParameterMap,TEXT("DOFOffset"),TRUE);
		MoBlurOffsetParameter.Bind(Initializer.ParameterMap,TEXT("MoBlurOffset"),TRUE);
		MoBlurDOFWeightParameter.Bind(Initializer.ParameterMap,TEXT("MoBlurDOFWeight"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockOnDOFFilterPixelShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("MainDOF"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << FilterTextureParameter << VelocityTextureParameter << DOFOffsetParameter << MoBlurOffsetParameter << MoBlurDOFWeightParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		const FTexture2DRHIRef& FilterTexture,
		const FTexture2DRHIRef& VelocityTexture,
		const FVector4* DOFOffsets,
		const FVector4* MoBlurOffsets,
		const FVector2D& MoBlurDOFWeight)
	{
		SetTextureParameterDirectly(
			GetPixelShader(),
			FilterTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FilterTexture);

		if(IsValidRef(VelocityTexture))
		{
			SetTextureParameterDirectly(
				GetPixelShader(),
				VelocityTextureParameter,
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				VelocityTexture);
		}

		SetPixelShaderValues(GetPixelShader(), DOFOffsetParameter, DOFOffsets, NumSamples);
		SetPixelShaderValues(GetPixelShader(), MoBlurOffsetParameter, MoBlurOffsets, 4);
		SetPixelShaderValue(GetPixelShader(), MoBlurDOFWeightParameter, MoBlurDOFWeight);
	}

public:
	FShaderResourceParameter	FilterTextureParameter;
	FShaderResourceParameter	VelocityTextureParameter;
	FShaderParameter			DOFOffsetParameter;
	FShaderParameter			MoBlurOffsetParameter;
	FShaderParameter			MoBlurDOFWeightParameter;
};

#define VARIATION_DOFFILTER(N)	typedef TRockOnDOFFilterPixelShader<N> TRockOnDOFFilterPixelShader##N; \
	IMPLEMENT_SHADER_TYPE2(template<>, TRockOnDOFFilterPixelShader##N, SF_Pixel, 0, 0);
	VARIATION_DOFFILTER(4) VARIATION_DOFFILTER(17)
#undef VARIATION_DOFFILTER

/*-----------------------------------------------------------------------------
	RockOn velocity passes (used only when motion blur is on)
	RockCameraMotionBlur* and RockVelocitySmear*, entry "Main".
-----------------------------------------------------------------------------*/
class TRockCameraMotionBlurVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockCameraMotionBlurVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	TRockCameraMotionBlurVertexShader() {}
	TRockCameraMotionBlurVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer) {}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockCameraMotionBlurVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("Main"); }

	virtual UBOOL Serialize(FArchive& Ar) { return FGlobalShader::Serialize(Ar); }
};
IMPLEMENT_SHADER_TYPE(,TRockCameraMotionBlurVertexShader,TEXT("RockCameraMotionBlurVertexShader"),TEXT("Main"),SF_Vertex,0,0);

class TRockCameraMotionBlurPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockCameraMotionBlurPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	TRockCameraMotionBlurPixelShader() {}
	TRockCameraMotionBlurPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		SceneTextureParameters.Bind(Initializer.ParameterMap);
		PrevViewProjMatrixParameter.Bind(Initializer.ParameterMap,TEXT("PrevViewProjMatrix"),TRUE);
	}

	void SetParameters(const FViewInfo& View)
	{
		SceneTextureParameters.Set(&View, this, SF_Point);
		SetPixelShaderValue(GetPixelShader(), PrevViewProjMatrixParameter, View.PrevViewProjMatrix);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockCameraMotionBlurPixelShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("Main"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneTextureParameters << PrevViewProjMatrixParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	FSceneTextureShaderParameters	SceneTextureParameters;
	FShaderParameter				PrevViewProjMatrixParameter;
};
IMPLEMENT_SHADER_TYPE(,TRockCameraMotionBlurPixelShader,TEXT("RockCameraMotionBlurPixelShader"),TEXT("Main"),SF_Pixel,0,0);

class TRockVelocitySmearVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockVelocitySmearVertexShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	TRockVelocitySmearVertexShader() {}
	TRockVelocitySmearVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer) {}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockVelocitySmearVertexShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("Main"); }

	virtual UBOOL Serialize(FArchive& Ar) { return FGlobalShader::Serialize(Ar); }
};
IMPLEMENT_SHADER_TYPE(,TRockVelocitySmearVertexShader,TEXT("RockVelocitySmearVertexShader"),TEXT("Main"),SF_Vertex,0,0);

class TRockVelocitySmearPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRockVelocitySmearPixelShader,Global);

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	TRockVelocitySmearPixelShader() {}
	TRockVelocitySmearPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		StoredVelocityParameter.Bind(Initializer.ParameterMap,TEXT("StoredVelocity"),TRUE);
		BufferTexelSizeXYParameter.Bind(Initializer.ParameterMap,TEXT("BufferTexelSizeXY"),TRUE);
		UVMinMaxParameter.Bind(Initializer.ParameterMap,TEXT("UVMinMax"),TRUE);
	}

	static const TCHAR* GetSourceFilename()	{ return TEXT("RockVelocitySmearPixelShader"); }
	static const TCHAR* GetFunctionName()	{ return TEXT("Main"); }

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StoredVelocityParameter << BufferTexelSizeXYParameter << UVMinMaxParameter;
		return bShaderHasOutdatedParameters;
	}

public:
	FShaderResourceParameter	StoredVelocityParameter;
	FShaderParameter			BufferTexelSizeXYParameter;
	FShaderParameter			UVMinMaxParameter;
};
IMPLEMENT_SHADER_TYPE(,TRockVelocitySmearPixelShader,TEXT("RockVelocitySmearPixelShader"),TEXT("Main"),SF_Pixel,0,0);

/*-----------------------------------------------------------------------------
	FRockOnSceneProxy
-----------------------------------------------------------------------------*/
class FRockOnSceneProxy : public FDOFAndBloomPostProcessSceneProxy
{
public:
	FRockOnSceneProxy(const URockOn* InEffect, const FPostProcessSettings* WorldSettings, UINT InColorGradingCVar, UBOOL bInMotionBlur, UBOOL bInImageGrain, UBOOL bInHighQualityDOF)
		:	FDOFAndBloomPostProcessSceneProxy(InEffect, WorldSettings)
		,	MotionBlurSoftEdgeKernelSize(InEffect->MotionBlurSoftEdgeKernelSize)
		,	ColorGradingCVar(InColorGradingCVar)
		,	bMotionBlur(bInMotionBlur)
		,	bHighQualityDOF(bInHighQualityDOF)
	{
		checkSlow(IsInGameThread());
		check(InEffect);

		SET_POSTPROCESS_PROPERTY2(Scene, ImageGrainScale);

		ColorTransform.Shadows		= GET_POSTPROCESS_PROPERTY2(Scene, Shadows);
		ColorTransform.HighLights	= GET_POSTPROCESS_PROPERTY2(Scene, HighLights);
		ColorTransform.MidTones		= GET_POSTPROCESS_PROPERTY2(Scene, MidTones);
		ColorTransform.Desaturation	= GET_POSTPROCESS_PROPERTY2(Scene, Desaturation);
		ColorTransform.Colorize		= GET_POSTPROCESS_PROPERTY2(Scene, Colorize);

		MotionBlurParams.RotationThreshold		= GET_POSTPROCESS_PROPERTY1(MotionBlur, CameraRotationThreshold);
		MotionBlurParams.TranslationThreshold	= GET_POSTPROCESS_PROPERTY1(MotionBlur, CameraTranslationThreshold);
		MotionBlurParams.MaxVelocity			= GET_POSTPROCESS_PROPERTY1(MotionBlur, MaxVelocity);
		MotionBlurParams.MotionBlurAmount		= GET_POSTPROCESS_PROPERTY2(MotionBlur, Amount);
		MotionBlurParams.bFullMotionBlur		= InEffect->FullMotionBlur;

		if(WorldSettings && WorldSettings->bOverride_MotionBlur_FullMotionBlur)
		{
			MotionBlurParams.bFullMotionBlur = WorldSettings->MotionBlur_FullMotionBlur;
		}

		// console command override
		{
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomScale"));
			FLOAT Value = CVar->GetFloat();
			if(Value >= 0.0f)
			{
				BloomScale = Value;
			}
		}

		// console command override
		{
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("BloomSize"));
			FLOAT Value = CVar->GetFloat();
			if(Value >= 0.0f)
			{
				BlurBloomKernelSize = Value;
			}
		}

		// console command override
		{
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("MotionBlurSoftEdge"));
			FLOAT Value = CVar->GetFloat();
			if(Value >= 0.0f)
			{
				MotionBlurSoftEdgeKernelSize = Value;
			}
		}

		if(!bInImageGrain)
		{
			SceneImageGrainScale = 0.0f;
		}

		// console command override
		{
			static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("ImageGrain"));
			FLOAT Value = CVar->GetFloat();
			if(Value >= 0.0f)
			{
				SceneImageGrainScale = Value;
			}
		}

		// ensure desat is in range (can happen in the editor)
		ColorTransform.Desaturation = Clamp(ColorTransform.Desaturation, 0.f, 1.f);

		// blend multiple LUT to produce final LUT (3D color lookup texture) for color grading
		if(WorldSettings && ColorGradingCVar != 0)
		{
			if(!WorldSettings->ColorGradingLUT.IsLUTEmpty())
			{
				// game filled out the table (soft transitions)
				WorldSettings->ColorGradingLUT.CopyToRenderThread(ColorGradingBlenderRTCopy);
			}
			else
			{
				// game wasn't filling the table so we set the editor LUT (hard switch)
				FLUTBlender LocalColorGradingLUT;
				LocalColorGradingLUT.Reset();
				if(ColorGrading_LookupTable)
				{
					LocalColorGradingLUT.LerpTo(ColorGrading_LookupTable, 1.0f);
				}
				LocalColorGradingLUT.CopyToRenderThread(ColorGradingBlenderRTCopy);
			}
		}

		extern INT GMotionBlurFullMotionBlur;
		MotionBlurParams.bFullMotionBlur = GMotionBlurFullMotionBlur < 0 ? MotionBlurParams.bFullMotionBlur : (GMotionBlurFullMotionBlur > 0);

		const FLOAT MinRotationThreshold = 5.0f;
		MotionBlurParams.RotationThreshold = Max<FLOAT>(MinRotationThreshold, MotionBlurParams.RotationThreshold);
		const FLOAT MinTranslationThreshold = 10.0f;
		MotionBlurParams.TranslationThreshold = Max<FLOAT>(MinTranslationThreshold, MotionBlurParams.TranslationThreshold);

		if(WorldSettings && !WorldSettings->bEnableSceneEffect)
		{
			ColorTransform.Reset();
		}
	}

	// generate the half-res post process buffer (BlurredImage): rgb=color/4, a=DOF mask.
	void RenderRockOnBloomGatherPass(FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams, FSceneRenderTargetIndex FilterColorIndex)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockOnBloomGatherPass"));

		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT HalfSizeX = BufferSizeX / 2;
		const UINT HalfSizeY = BufferSizeY / 2;
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();

		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		{
			SCOPED_DRAW_EVENT(ScaleEvent)(DEC_SCENE_ITEMS,TEXT("RockOnScaleByHalf"));

			GSceneRenderTargets.BeginRenderingHalfSizeDownsample();

			TShaderMapRef<FRockOnHalfDownsampleVertexShader> VertexShader(GetGlobalShaderMap());
			SetVertexShaderValue(
				VertexShader->GetVertexShader(),
				VertexShader->dUVOffsetsParameter,
				FVector4(1.0f / BufferSizeX, 1.0f / BufferSizeY, -0.5f / BufferSizeX, -0.5f / BufferSizeY));

			TShaderMapRef<FRockOnHalfDownsamplePixelShader> PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DepthOfFieldParams);

			static FGlobalBoundShaderState HalfDownsampleBoundShaderState;
			SetGlobalBoundShaderState(HalfDownsampleBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
			RHIReduceTextureCachePenalty(PixelShader->GetPixelShader());

			DrawDenormalizedQuad(
				0, 0,
				View.SizeX / 2, View.SizeY / 2,
				View.RenderTargetX / 2, View.RenderTargetY / 2,
				View.SizeX, View.SizeY,
				HalfSizeX, HalfSizeY,
				BufferSizeX, BufferSizeY);

			GSceneRenderTargets.FinishRenderingHalfSizeDownsample();
		}

		{
			SCOPED_DRAW_EVENT(ScaleEvent)(DEC_SCENE_ITEMS,TEXT("ScaleToQuarterAndDOF"));

			GSceneRenderTargets.BeginRenderingFilter(FilterColorIndex);

			TShaderMapRef<FRockOnHalfDownsampleVertexShader> VertexShader(GetGlobalShaderMap());
			SetVertexShaderValue(
				VertexShader->GetVertexShader(),
				VertexShader->dUVOffsetsParameter,
				FVector4(1.0f / HalfSizeX, 1.0f / HalfSizeY, -0.5f / HalfSizeX, -0.5f / HalfSizeY));

			TShaderMapRef<FRockOnHalfsizeDownsampleAndDOFPixelShader> PixelShader(GetGlobalShaderMap());
			PixelShader->SetParameters(View, DepthOfFieldParams, GSceneRenderTargets.GetHalfResPostProcessTexture());

			static FGlobalBoundShaderState HalfsizeDownsampleAndDOFBoundShaderState;
			SetGlobalBoundShaderState(HalfsizeDownsampleAndDOFBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
			RHIReduceTextureCachePenalty(PixelShader->GetPixelShader());

			DrawDenormalizedQuad(
				0, 0,
				GSceneRenderTargets.GetFilterBufferSizeX(), GSceneRenderTargets.GetFilterBufferSizeY(),
				View.RenderTargetX / 2, View.RenderTargetY / 2,
				HalfSizeX, HalfSizeY,
				FilterBufferSizeX, FilterBufferSizeY,
				HalfSizeX, HalfSizeY);

			GSceneRenderTargets.FinishRenderingFilter(FilterColorIndex);
		}
	}

	template<UBOOL bInitialBloomPass, UBOOL bFinalBloomPass>
	void RenderRockGaussianBlurPass(
		UINT SizeX,
		UINT SizeY,
		FSceneRenderTargetIndex SourceIndex,
		FSceneRenderTargetIndex DestIndex,
		const FVector2D& SampleMaskMin,
		const FVector2D& SampleMaskMax,
		const FVector4& BloomTintAndScreenBlendThreshold)
	{
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const FVector2D SampleKernel(1.5f / FilterBufferSizeX, 1.5f / FilterBufferSizeY);

		GSceneRenderTargets.BeginRenderingFilter(DestIndex);

		TShaderMapRef<FRockOnFilterVertexShader> VertexShader(GetGlobalShaderMap());
		VertexShader->SetParameters(SampleKernel);

		TShaderMapRef<FRockOnFilterPixelShader<bInitialBloomPass,bFinalBloomPass> > PixelShader(GetGlobalShaderMap());
		PixelShader->SetParameters(
			GSceneRenderTargets.GetFilterColorTexture(SourceIndex),
			GBlackTexture->TextureRHI,
			SampleMaskMin,
			SampleMaskMax,
			BloomTintAndScreenBlendThreshold);

		static FGlobalBoundShaderState BloomFilterBoundShaderState;
		SetGlobalBoundShaderState(BloomFilterBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		RHIReduceTextureCachePenalty(PixelShader->GetPixelShader());

		DrawDenormalizedQuad(
			0, 0,
			SizeX, SizeY,
			0, 0,
			SizeX, SizeY,
			FilterBufferSizeX, FilterBufferSizeY,
			FilterBufferSizeX, FilterBufferSizeY);

		GSceneRenderTargets.FinishRenderingFilter(DestIndex);
	}

	void RockGaussianBlurFilterBuffer(UINT SizeX, UINT SizeY, FSceneRenderTargetIndex SourceIndex, FSceneRenderTargetIndex DestIndex, FVector2D SampleMaskMin, FVector2D SampleMaskMax)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockGaussianBlur"));

		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		const FLOAT BloomPower = (appPow(0.5f, 0.5f * BloomThreshold + 1.0f) - 1.0f) * -2.0f;
		const FVector4 BloomTintAndScreenBlendThreshold(
			BloomTint.R * BloomPower * BloomScale,
			BloomTint.G * BloomPower * BloomScale,
			BloomTint.B * BloomPower * BloomScale,
			1.0f - BloomPower);

		RenderRockGaussianBlurPass<TRUE,FALSE>(
			SizeX,
			SizeY,
			SourceIndex,
			SRTI_FilterColor2,
			SampleMaskMin,
			SampleMaskMax,
			BloomTintAndScreenBlendThreshold);

		RenderRockGaussianBlurPass<FALSE,FALSE>(
			SizeX,
			SizeY,
			SRTI_FilterColor2,
			DestIndex,
			SampleMaskMin,
			SampleMaskMax,
			BloomTintAndScreenBlendThreshold);

		RenderRockGaussianBlurPass<FALSE,FALSE>(
			SizeX,
			SizeY,
			DestIndex,
			SRTI_FilterColor2,
			SampleMaskMin,
			SampleMaskMax,
			BloomTintAndScreenBlendThreshold);

		RenderRockGaussianBlurPass<FALSE,TRUE>(
			SizeX,
			SizeY,
			SRTI_FilterColor2,
			DestIndex,
			SampleMaskMin,
			SampleMaskMax,
			BloomTintAndScreenBlendThreshold);
	}

	void SetRockDOFOffsets(UBOOL bFourSamples, UBOOL bLocalMotionBlur, FVector4* DOFOffsets, FVector4* MoBlurOffsets, FVector2D& MoBlurDOFWeight)
	{
		const FLOAT InvX = 1.0f / GSceneRenderTargets.GetFilterBufferSizeX();
		const FLOAT InvY = 1.0f / GSceneRenderTargets.GetFilterBufferSizeY();

		if(bFourSamples)
		{
			DOFOffsets[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			DOFOffsets[1] = FVector4(-InvX, 0.0f, 1.0f, 0.0f);
			DOFOffsets[2] = FVector4(0.0f, InvY, 1.0f, 0.0f);
			DOFOffsets[3] = FVector4(-InvX, InvY, appSqrt(2.0f), 0.0f);
		}
		else
		{
			DOFOffsets[0] = FVector4(-InvX, 3.0f * InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[1] = FVector4(InvX, 3.0f * InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[2] = FVector4(-2.0f * InvX, 2.0f * InvY, appSqrt(8.0f), 0.0f);
			DOFOffsets[3] = FVector4(0.0f, 2.0f * InvY, 2.6659999f, 0.0f);
			DOFOffsets[4] = FVector4(2.0f * InvX, 2.0f * InvY, appSqrt(8.0f), 0.0f);
			DOFOffsets[5] = FVector4(-3.0f * InvX, InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[6] = FVector4(3.0f * InvX, InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[7] = FVector4(-2.0f * InvX, 0.0f, 2.6659999f, 0.0f);
			DOFOffsets[8] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			DOFOffsets[9] = FVector4(2.0f * InvX, 0.0f, 2.6659999f, 0.0f);
			DOFOffsets[10] = FVector4(-3.0f * InvX, -InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[11] = FVector4(3.0f * InvX, -InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[12] = FVector4(-2.0f * InvX, -2.0f * InvY, appSqrt(8.0f), 0.0f);
			DOFOffsets[13] = FVector4(0.0f, -2.0f * InvY, 2.6659999f, 0.0f);
			DOFOffsets[14] = FVector4(2.0f * InvX, -2.0f * InvY, appSqrt(8.0f), 0.0f);
			DOFOffsets[15] = FVector4(-InvX, -3.0f * InvY, appSqrt(10.0f), 0.0f);
			DOFOffsets[16] = FVector4(InvX, -3.0f * InvY, appSqrt(10.0f), 0.0f);
		}

		MoBlurDOFWeight = FVector2D(0.0f, 1.0f);
		if(bLocalMotionBlur)
		{
			MoBlurOffsets[0] = FVector4((-2.5f * InvX) * 6.0f, (-2.5f * InvY) * 6.0f, 0.0f, 0.0f);
			MoBlurOffsets[1] = FVector4((-0.5f * InvX) * 6.0f, (-0.5f * InvY) * 6.0f, 0.0f, 0.0f);
			MoBlurOffsets[2] = FVector4((0.5f * InvX) * 6.0f, (0.5f * InvY) * 6.0f, 0.0f, 0.0f);
			MoBlurOffsets[3] = FVector4((2.5f * InvX) * 6.0f, (2.5f * InvY) * 6.0f, 0.0f, 0.0f);
			MoBlurDOFWeight.X = 1.0f;
		}
		else
		{
			appMemzero(MoBlurOffsets, sizeof(FVector4) * 4);
		}
	}

	template<UINT NumSamples>
	void RenderRockDOFBlurPass(FViewInfo& View, UBOOL bLocalMotionBlur, FSceneRenderTargetIndex SourceIndex)
	{
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		FVector4 DOFOffsets[17];
		FVector4 MoBlurOffsets[4];
		FVector2D MoBlurDOFWeight;
		SetRockDOFOffsets(NumSamples == 4, bLocalMotionBlur, DOFOffsets, MoBlurOffsets, MoBlurDOFWeight);

		GSceneRenderTargets.BeginRenderingFilter(SRTI_FilterColor0);

		TShaderMapRef<FRockOnDOFFilterVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<TRockOnDOFFilterPixelShader<NumSamples> > PixelShader(GetGlobalShaderMap());
		PixelShader->SetParameters(
			GSceneRenderTargets.GetFilterColorTexture(SourceIndex),
			GSceneRenderTargets.GetVelocityTexture(),
			DOFOffsets,
			MoBlurOffsets,
			MoBlurDOFWeight);

		static FGlobalBoundShaderState DOFFilterBoundShaderState;
		SetGlobalBoundShaderState(DOFFilterBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
		RHIReduceTextureCachePenalty(PixelShader->GetPixelShader());

		DrawDenormalizedQuad(
			0, 0,
			DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder,
			0, 0,
			DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder,
			FilterBufferSizeX, FilterBufferSizeY,
			FilterBufferSizeX, FilterBufferSizeY);

		GSceneRenderTargets.FinishRenderingFilter(SRTI_FilterColor0);
	}

	void RockDOFBlurFilterBuffer(FViewInfo& View, UBOOL bLocalMotionBlur)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockDOFBlur"));

		if(bHighQualityDOF)
		{
			RenderRockDOFBlurPass<17>(View, bLocalMotionBlur, SRTI_FilterColor0);
			RenderRockDOFBlurPass<4>(View, bLocalMotionBlur, SRTI_FilterColor0);
		}
		else
		{
			RenderRockDOFBlurPass<4>(View, bLocalMotionBlur, SRTI_FilterColor0);
		}
	}

	void RenderHalfRes(FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams, UBOOL bLocalMotionBlur)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockOnHalfRes"));

		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		FVector2D SampleMaskMin(0 / (FLOAT)BufferSizeX, 0 / (FLOAT)BufferSizeY);
		FVector2D SampleMaskMax((0 + View.SizeX - 1) / (FLOAT)BufferSizeX, (0 + View.SizeY - 1) / (FLOAT)BufferSizeY);

		RenderRockOnBloomGatherPass(View, DepthOfFieldParams, SRTI_FilterColor0);
		RockGaussianBlurFilterBuffer(DownsampledSizeX + AntiLeakBorder, DownsampledSizeY + AntiLeakBorder, SRTI_FilterColor0, SRTI_FilterColor1, SampleMaskMin, SampleMaskMax);
		RockDOFBlurFilterBuffer(View, bLocalMotionBlur);

		// FilterColor1 is the blurred RockOn bloom/light-shaft buffer; FilterColor0 is the DOF-filtered BlurredImage.
	}

	// the final RockOn blend pass: DOF composite + bloom + grain + tonemapping + LUT grading.
	template<UBOOL bTHighQualityDOF, UBOOL bTImageGrain, UBOOL bTMotionBlur>
	void RenderBlend(FViewInfo& View, const FDepthOfFieldParams& DepthOfFieldParams, FSceneColorLDRInfo& LDRInfo, const FTextureRHIRef& ColorGradingRHI)
	{
		const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
		const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
		const UINT FilterBufferSizeX = GSceneRenderTargets.GetFilterBufferSizeX();
		const UINT FilterBufferSizeY = GSceneRenderTargets.GetFilterBufferSizeY();
		const UINT FilterDownsampleFactor = GSceneRenderTargets.GetFilterDownsampleFactor();
		const UINT DownsampledSizeX = View.RenderTargetSizeX / FilterDownsampleFactor;
		const UINT DownsampledSizeY = View.RenderTargetSizeY / FilterDownsampleFactor;

		// pick the render target exactly like uber's logic
		if( !View.Family->bResolveScene )
		{
			GSceneRenderTargets.BeginRenderingSceneColor();
		}
		else
		{
			DWORD UsageFlags = RTUsage_FullOverwrite;
			if ( LDRInfo.bAdjustPingPong && (LDRInfo.NumPingPongsRemaining & 1) )
			{
				UsageFlags |= RTUsage_DontSwapBuffer;
			}

			if (FinalEffectInGroup && !GSystemSettings.NeedsUpscale())
			{
				GSceneRenderTargets.BeginRenderingBackBuffer( UsageFlags );
			}
			else
			{
				GSceneRenderTargets.BeginRenderingSceneColorLDR( UsageFlags );
			}
		}

		TShaderMapRef<FRockOnVertexShader<bTImageGrain> > BlendVertexShader(GetGlobalShaderMap());
		TShaderMapRef<FRockOnBlendPixelShader<bTHighQualityDOF, bTImageGrain, bTMotionBlur> > BlendPixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState RockOnBlendBoundShaderState;
		SetGlobalBoundShaderState(RockOnBlendBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *BlendVertexShader, *BlendPixelShader, sizeof(FFilterVertex));

		// DOF parameters (FocusDistance/InverseFocusRadius/MinMaxBlurClamp/PackedParameters)
		BlendPixelShader->DOFParameters.SetPS(*BlendPixelShader, DepthOfFieldParams);

		// scene texture (point) for CalcSceneColorAndDepth
		BlendPixelShader->SceneTextureParameters.Set(&View, *BlendPixelShader, SF_Point);

		const FLOAT BloomPower = (appPow(0.5f, 0.5f * BloomThreshold + 1.0f) - 1.0f) * -2.0f;
		const FVector4 BloomTintAndScreenBlendThreshold(
			BloomTint.R * BloomPower * BloomScale,
			BloomTint.G * BloomPower * BloomScale,
			BloomTint.B * BloomPower * BloomScale,
			1.0f - BloomPower);
		SetPixelShaderValue(
			BlendPixelShader->GetPixelShader(),
			BlendPixelShader->BloomTintAndScreenBlendThresholdParameter,
			BloomTintAndScreenBlendThreshold);

		// front buffer texel size
		const FLOAT InvX = 1.0f / BufferSizeX;
		const FLOAT InvY = 1.0f / BufferSizeY;
		SetPixelShaderValue(BlendPixelShader->GetPixelShader(), BlendPixelShader->BufferTexelSizeXYParameter, FVector2D(InvX, InvY));

		// BlurredImage = blurred DOF half/quarter-res buffer (FilterColor0)
		SetTextureParameterDirectly(
			BlendPixelShader->GetPixelShader(),
			BlendPixelShader->BlurredImageParameter,
			TStaticSamplerState<SF_Bilinear>::GetRHI(),
			GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor0));

		// LightShaftBuffer = bloom buffer (FilterColor1)
		SetTextureParameterDirectly(
			BlendPixelShader->GetPixelShader(),
			BlendPixelShader->LightShaftBufferParameter,
			TStaticSamplerState<SF_Bilinear>::GetRHI(),
			GSceneRenderTargets.GetFilterColorTexture(SRTI_FilterColor1));

		// color grading LUT
		SetTextureParameterDirectly(
			BlendPixelShader->GetPixelShader(),
			BlendPixelShader->ColorGradingLUTParameter,
			TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
			ColorGradingRHI);

		// velocity buffer for motion blur
		if(bTMotionBlur)
		{
			SetTextureParameterDirectly(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->VelocityBufferParameter,
				TStaticSamplerState<SF_Bilinear>::GetRHI(),
				GSceneRenderTargets.GetVelocityTexture());
		}

		// image grain
		if(bTImageGrain)
		{
			UTexture2D* NoiseTexture = GEngine->ImageGrainNoiseTexture;
			if(!NoiseTexture)
			{
				NoiseTexture = GEngine->ScreenDoorNoiseTexture;
			}
			if(NoiseTexture && NoiseTexture->Resource)
			{
				SetTextureParameter(
					BlendPixelShader->GetPixelShader(),
					BlendPixelShader->PostProcessNoiseTextureParameter,
					TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
					NoiseTexture->Resource->TextureRHI);
			}

			static INT OldNoiseNum = 0;
			OldNoiseNum = (OldNoiseNum + 1 > 2) ? 0 : OldNoiseNum + 1;
			SetPixelShaderValue(
				BlendPixelShader->GetPixelShader(),
				BlendPixelShader->FilmGrainParameter,
				FVector4(OldNoiseNum == 0 ? 1.0f : 0.0f, OldNoiseNum == 1 ? 1.0f : 0.0f, OldNoiseNum == 2 ? 1.0f : 0.0f, SceneImageGrainScale));

			if(NoiseTexture)
			{
				const FLOAT GrainOffsetW = appFrand();
				const FLOAT GrainOffsetZ = appFrand();
				SetVertexShaderValue(
					BlendVertexShader->GetVertexShader(),
					BlendVertexShader->FilmGrainScaleOffsetParameter,
					FVector4(BufferSizeX / (FLOAT)NoiseTexture->SizeX, BufferSizeY / (FLOAT)NoiseTexture->SizeY, GrainOffsetZ, GrainOffsetW));
			}
		}

		// Vertex shader SceneCoordinateScaleBias - map screen position to scene UV.
		// We need to adjust UV coordinate calculation when rendering directly to the
		// view's render target (editor) - mirror uber's branch.
		UINT TargetSizeX = View.Family->RenderTarget->GetSizeX();
		UINT TargetSizeY = View.Family->RenderTarget->GetSizeY();

		if( FinalEffectInGroup
			&& View.Family->bResolveScene
			&& !GSystemSettings.NeedsUpscale() )
		{
			SetVertexShaderValue(
				BlendVertexShader->GetVertexShader(),
				BlendVertexShader->SceneCoordinateScaleBiasParameter,
				FVector4(
					0.5f * (TargetSizeX / (FLOAT)BufferSizeX),
					-0.5f * (TargetSizeY / (FLOAT)BufferSizeY),
					0.5f * (TargetSizeY / (FLOAT)BufferSizeY) + (GPixelCenterOffset - (View.Y - View.RenderTargetY)) * InvY,
					0.5f * (TargetSizeX / (FLOAT)BufferSizeX) + (GPixelCenterOffset - (View.X - View.RenderTargetX)) * InvX));

			DrawDenormalizedQuad(
				View.X, View.Y, View.SizeX, View.SizeY,
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				TargetSizeX, TargetSizeY,
				BufferSizeX, BufferSizeY);
		}
		else
		{
			SetVertexShaderValue(
				BlendVertexShader->GetVertexShader(),
				BlendVertexShader->SceneCoordinateScaleBiasParameter,
				FVector4(
					0.5f,
					-0.5f,
					0.5f + (GPixelCenterOffset - View.RenderTargetY) * InvY,
					0.5f + (GPixelCenterOffset - View.RenderTargetX) * InvX));

			DrawDenormalizedQuad(
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				View.RenderTargetX, View.RenderTargetY, View.RenderTargetSizeX, View.RenderTargetSizeY,
				BufferSizeX, BufferSizeY,
				BufferSizeX, BufferSizeY);
		}

		// resolve
		if( FinalEffectInGroup
			&& View.Family->bResolveScene
			&& !GSystemSettings.NeedsUpscale() )
		{
			// drew directly to the back buffer; nothing to resolve here
		}
		else
		{
			FResolveRect ResolveRect;
			ResolveRect.X1 = View.RenderTargetX;
			ResolveRect.Y1 = View.RenderTargetY;
			ResolveRect.X2 = View.RenderTargetX + View.RenderTargetSizeX;
			ResolveRect.Y2 = View.RenderTargetY + View.RenderTargetSizeY;

			if( View.Family->bResolveScene )
			{
				GSceneRenderTargets.FinishRenderingSceneColorLDR(TRUE, ResolveRect);
			}
			else
			{
				GSceneRenderTargets.FinishRenderingSceneColor(TRUE);
			}
		}

		if( View.Family->bResolveScene )
		{
			// from now on the scene color is in an LDR surface
			View.bUseLDRSceneColor = TRUE;
		}
	}

	UBOOL Render(const FScene* Scene, UINT InDepthPriorityGroup, FViewInfo& View, const FMatrix& CanvasTransform, FSceneColorLDRInfo& LDRInfo)
	{
		SCOPED_DRAW_EVENT(Event)(DEC_SCENE_ITEMS,TEXT("RockOnPostProcessing"));

		check(SDPG_PostProcess == InDepthPriorityGroup);

		GSceneRenderTargets.ResolveFullResTransluceny();

		check(FALSE == View.bUseLDRSceneColor);

		// blend final LUT (3D color lookup texture) for color grading
		FTextureRHIRef ColorGradingRHI = ColorGradingBlenderRTCopy.ResolveLUT(View, ColorTransform);

		// should we do motion blur for this view?
		UBOOL bLocalMotionBlur = bMotionBlur;
		extern UBOOL GIsTiledScreenshot;
		extern INT GGameScreenshotCounter;
		extern UBOOL GScreenShotRequest;
		if((View.Family->ShowFlags & SHOW_MotionBlur) == 0 || !View.Family->bRealtimeUpdate || !GIsGame || GIsEditor || GIsTiledScreenshot || GGameScreenshotCounter != 0 || GScreenShotRequest)
		{
			bLocalMotionBlur = FALSE;
		}

		FDepthOfFieldParams RockOnDOFParams;
		ComputeDOFParams(View, RockOnDOFParams);

		// NOTE: When motion blur is active the original game runs a camera-velocity pass
		// plus a velocity smear pass (RockCameraMotionBlur*/RockVelocitySmear*) to populate
		// the velocity buffer. The velocity buffer is already produced by the engine's
		// motion-blur prepass (RequiresVelocities) here, so the RockOn blend reads it
		// directly; the bespoke smear is approximated by that prepass.

		// half-res post process buffers: BlurredImage and LightShaftBuffer
		RenderHalfRes(View, RockOnDOFParams, bLocalMotionBlur);

		UBOOL bLocalImageGrain = SceneImageGrainScale > 0.0f;
		if((View.Family->ShowFlags & SHOW_ImageGrain) == 0)
		{
			bLocalImageGrain = FALSE;
		}

		// final blend - dispatch the 8 permutations
#define VARIATION_BLEND(A,B,C) \
		if(A == bHighQualityDOF && B == bLocalImageGrain && C == bLocalMotionBlur) \
		{ \
			RenderBlend<A,B,C>(View, RockOnDOFParams, LDRInfo, ColorGradingRHI); \
		} else
		VARIATION_BLEND(0,0,0) VARIATION_BLEND(0,0,1) VARIATION_BLEND(0,1,0) VARIATION_BLEND(0,1,1)
		VARIATION_BLEND(1,0,0) VARIATION_BLEND(1,0,1) VARIATION_BLEND(1,1,0) VARIATION_BLEND(1,1,1)
		// terminator, should never be reached
		check(0);
#undef VARIATION_BLEND

		return TRUE;
	}

	virtual UBOOL RequiresVelocities( FMotionBlurParams& OutMotionBlurParams ) const
	{
		if ( bMotionBlur )
		{
			OutMotionBlurParams = MotionBlurParams;

			const AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			OutMotionBlurParams.bPlayersOnly = WorldInfo->bPlayersOnly;
		}
		return bMotionBlur;
	}

	virtual UBOOL RequiresPreviousTransforms(const FViewInfo& View) const
	{
		return bMotionBlur;
	}

	virtual UBOOL MayRenderSceneColorLDR() const
	{
		return TRUE;
	}

	void CheckForChanges( const FLUTBlender& PreviousLUTBlender )
	{
		ColorGradingBlenderRTCopy.CheckForChanges( PreviousLUTBlender );
	}

	const FLUTBlender& GetLUTBlender() const
	{
		return ColorGradingBlenderRTCopy;
	}

protected:
	ColorTransformMaterialProperties	ColorTransform;
	FMotionBlurParams					MotionBlurParams;
	FLUTBlender							ColorGradingBlenderRTCopy;
	FLOAT								SceneImageGrainScale;
	FLOAT								MotionBlurSoftEdgeKernelSize;
	UINT								ColorGradingCVar;
	UBOOL								bMotionBlur;
	UBOOL								bHighQualityDOF;

	// to prevent bad color content leaking in (split screen / partial render target)
	const static UINT AntiLeakBorder = 2;
};

/*-----------------------------------------------------------------------------
	URockOn
-----------------------------------------------------------------------------*/
FPostProcessSceneProxy* URockOn::CreateSceneProxy(const FPostProcessSettings* WorldSettings)
{
	if ( GUsingMobileRHI && !GMobileUsePostProcess )
	{
		return NULL;
	}

	UBOOL bLocalMotionBlur = FALSE;

	extern UBOOL GIsTiledScreenshot;
	extern INT GGameScreenshotCounter;
	extern UBOOL GScreenShotRequest;

	if ( (WorldSettings == NULL || WorldSettings->bEnableMotionBlur) && GSystemSettings.bAllowMotionBlur && GIsGame && !GIsEditor && !GIsTiledScreenshot && (GGameScreenshotCounter == 0) && !GScreenShotRequest )
	{
		bLocalMotionBlur = TRUE;
	}

	UBOOL bLocalImageGrain = bEnableImageGrain;
	{
		static IConsoleVariable* CVar = GConsoleManager->FindConsoleVariable(TEXT("ImageGrain"));
		const FLOAT Value = CVar->GetFloat();
		if(Value > 0.0f)
		{
			bLocalImageGrain = TRUE;
		}
		else if(Value == 0.0f)
		{
			bLocalImageGrain = FALSE;
		}
	}
	UBOOL bLocalHighQualityDOF = (DepthOfFieldQuality == DOFQuality_High);
	if(WorldSettings && WorldSettings->bOverride_bEnableHighQualityDOF)
	{
		bLocalHighQualityDOF = WorldSettings->bEnableHighQualityDOF;
	}

	return new FRockOnSceneProxy(this, WorldSettings, GColorGrading, bLocalMotionBlur, bLocalImageGrain, bLocalHighQualityDOF);
}

void URockOn::PostLoad()
{
	Super::PostLoad();

	// RockOn should only ever exist in the SDPG_PostProcess scene.
	SceneDPG = SDPG_PostProcess;

	// clamp desaturation to 0..1 (fixup for old data)
	SceneDesaturation = Clamp(SceneDesaturation, 0.f, 1.f);
}

void URockOn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SceneDesaturation = Clamp(SceneDesaturation, 0.f, 1.f);
	SceneDPG = SDPG_PostProcess;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
