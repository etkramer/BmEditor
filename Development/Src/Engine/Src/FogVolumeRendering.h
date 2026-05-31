/*=============================================================================
FogVolumeRendering.h: Definitions for rendering fog volumes.
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
enum EFogVolumeDensityFunction
{
	FVDF_None,
	FVDF_Constant,
	FVDF_ConstantHeight,
	FVDF_LinearHalfspace,
	FVDF_Sphere,
	FVDF_Cone
};

/*-----------------------------------------------------------------------------
FFogVolumeShaderParameters - encapsulates the parameters needed to calculate fog from a fog volume in a vertex shader.
-----------------------------------------------------------------------------*/
class FFogVolumeShaderParameters
{
public:

	/** Binds the parameters */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets the parameters on the input VertexShader, using fog volume data from the input DensitySceneInfo. */
	void SetVertexShader(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		FShader* VertexShader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;

#if WITH_D3D11_TESSELLATION
	/** Sets the parameters on the input DomainShader, using fog volume data from the input DensitySceneInfo. */
	void SetDomainShader(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		FShader* DomainShader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FFogVolumeShaderParameters& P);

private:

	template<typename ShaderRHIParamRef>
	void Set(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		ShaderRHIParamRef Shader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;

	FShaderParameter FirstDensityFunctionParameter;
	FShaderParameter SecondDensityFunctionParameter;
	FShaderParameter StartDistanceParameter;
	FShaderParameter FogVolumeBoxMinParameter;
	FShaderParameter FogVolumeBoxMaxParameter;
	FShaderParameter ApproxFogColorParameter;
};

/**
* Base FogSceneInfo - derivatives store render thread density function data.
*/
class FFogVolumeDensitySceneInfo
{
public:

	/** The fog component the scene info is for. */
	const class UFogVolumeDensityComponent* Component;

	/** 
	* Controls whether the fog volume affects intersecting translucency.  
	* If FALSE, the fog volume will sort normally with translucency and not fog intersecting translucent objects.
	*/
	UBOOL bAffectsTranslucency;

	/** 
	 * Controls whether the fog volume affects opaque pixels, or just intersecting translucency.  
	 */
	UBOOL bOnlyAffectsTranslucency;

	/** 
	* A color used to approximate the material color when fogging intersecting translucency. 
	* This is necessary because we can't evaluate the material when applying fog to the translucent object.
	*/
	FLinearColor ApproxFogColor;

	/** The AABB of the associated fog volume primitive component. */
	FBox VolumeBounds;

	/** The Depth Priority Group of the associated primitive component. */
	UINT DPGIndex;

	/** Distance from the camera at which the fog should start, in world units. */
	FLOAT StartDistance;

	FLOAT MaxDistance;

	/** Name of the owner actor, used for debugging */
	FName OwnerName;

	/** Initialization ctor */
	FFogVolumeDensitySceneInfo(const class UFogVolumeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	/** Draw a mesh with this density function. */
	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId) = 0;

	/** Gets the number of pixel shader instructions to render the integral. */
	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const = 0;

	/** Get the density function parameters that will be passed to the integral pixel shader. */
	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& View) const = 0;

	/** Get the density function parameters that will be passed to the integral pixel shader. */
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& View) const
	{
		return FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	/** 
	* Returns an estimate of the maximum integral value that will be calculated by this density function. 
	* This will be used to normalize the integral on platforms that it has to be stored in fixed point.
	* Too large of estimates will increase aliasing, too small will cause the fog volume to be clamped.
	*/
	virtual FLOAT GetMaxIntegral() const = 0;

	virtual EFogVolumeDensityFunction GetDensityFunctionType() const = 0;
};

/**
* Constant density fog
*/
class FFogVolumeConstantDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The constant density factor */
	FLOAT Density;

	/** Default constructor for creating a fog volume scene info without the corresponding component */
	FFogVolumeConstantDensitySceneInfo() :
		FFogVolumeDensitySceneInfo(NULL, FBox(0), SDPG_World),
		Density(0.005f)
	{}

	/** Initialization constructor. */
	FFogVolumeConstantDensitySceneInfo(const class UFogVolumeConstantDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& View) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Constant;
	}
};

/**
* Constant density fog, limited to a height
*/
class FFogVolumeConstantHeightDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The constant density factor */
	FLOAT Density;

	/** Maximum height */
	FLOAT Height;

	/** Initialization constructor. */
	FFogVolumeConstantHeightDensitySceneInfo(const class UFogVolumeConstantHeightDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_ConstantHeight;
	}
};

/** Halfspace fog defined by a plane, with density increasing linearly away from the plane. */
class FFogVolumeLinearHalfspaceDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** Linear density factor, scales the density contribution from the distance to the plane. */
	FLOAT PlaneDistanceFactor;

	/** Plane in worldspace that defines the fogged halfspace, whose normal points away from the fogged halfspace. */
	FPlane HalfspacePlane;

	/** Initialization constructor. */
	FFogVolumeLinearHalfspaceDensitySceneInfo(const class UFogVolumeLinearHalfspaceDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_LinearHalfspace;
	}
};


/** Spherical fog with density decreasing toward the edges of the sphere for soft edges. */
class FFogVolumeSphericalDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The density at the center of the sphere, which is the maximum density. */
	FLOAT MaxDensity;

	/** The sphere in worldspace */
	FSphere Sphere;

	/** Initialization constructor. */
	FFogVolumeSphericalDensitySceneInfo(const class UFogVolumeSphericalDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Sphere;
	}
};



/**  */
class FFogVolumeConeDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The density along the axis, which is the maximum density. */
	FLOAT MaxDensity;

	/** World space position of the cone vertex. */
	FVector ConeVertex;

	/** Distance from the vertex at which the cone ends. */
	FLOAT ConeRadius;

	/** Axis defining the direction of the cone. */
	FVector ConeAxis;

	/** Angle from axis that defines the cone size. */
	FLOAT ConeMaxAngle;

	/** Initialization constructor. */
	FFogVolumeConeDensitySceneInfo(const class UFogVolumeConeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Cone;
	}
};

class FNoDensityPolicy
{	
public:

	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;
	
	/** Empty parameter component. */
	class ShaderParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
		}
		void SetVertexShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* VertexShader,
			ElementDataType ElementData
			) const
		{
		}

#if WITH_D3D11_TESSELLATION
		void SetDomainShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* VertexShader,
			ElementDataType ElementData
			) const
		{
		}
#endif

		/** Serializer. */
		friend FArchive& operator<<(FArchive& Ar,ShaderParametersType& P)
		{
			return Ar;
		}
	};

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_None;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return TRUE;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_NONE"),TEXT("1"));
	}
};

class FConstantDensityPolicy
{	
public:

	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Constant;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
#if BATMAN
		return FALSE;
#else
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
#endif
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_CONSTANT"),TEXT("1"));
	}
};

class FLinearHalfspaceDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_LinearHalfspace;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
#if BATMAN
		return FALSE;
#else
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
#endif
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_LINEARHALFSPACE"),TEXT("1"));
	}
};

class FSphereDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Sphere;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
#if BATMAN
		return Material->GetBlendMode() == BLEND_Opaque;
#else
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
#endif
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_SPHEREDENSITY"),TEXT("1"));
	}
};

#if BATMAN
class FRockAtmosDensityPolicy
{
public:

	class FFogRockAtmosShaderParameters
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
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
			AtmosGlobalGradientColourParameter.Bind(ParameterMap,TEXT("AtmosGlobal_Gradient_Colour"),TRUE);
			AtmosGlobalGradientDirectionParameter.Bind(ParameterMap,TEXT("AtmosGlobal_Gradient_Direction"),TRUE);
		}

		void SetVertexShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* VertexShader,
			const FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
			) const
		{
			Set(View, VertexShader);
		}

#if WITH_D3D11_TESSELLATION
		void SetDomainShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* DomainShader,
			const FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
			) const
		{
			Set(View, DomainShader);
		}
#endif

		friend FArchive& operator<<(FArchive& Ar,FFogRockAtmosShaderParameters& P)
		{
			Ar << P.AtmosDensityParameter;
			Ar << P.AtmosStartEndParameter;
			Ar << P.AtmosGradSizeHeightIntegralParameter;
			Ar << P.AtmosD1ColourParameter;
			Ar << P.AtmosD2ColourParameter;
			Ar << P.AtmosH1H2AlphaParameter;
			Ar << P.AtmosH1ColourParameter;
			Ar << P.AtmosH2ColourParameter;
			Ar << P.CameraPosParameter;
			Ar << P.AtmosGlobalGradientColourParameter;
			Ar << P.AtmosGlobalGradientDirectionParameter;
			return Ar;
		}

	private:
		void Set(const FSceneView& View,FShader* Shader) const
		{
			const FPostProcessSettings* Settings = View.PostProcessSettings;
			if (Settings)
			{
				const FVector4 CameraPos(View.ViewOrigin.X, View.ViewOrigin.Y, View.ViewOrigin.Z, View.ViewOrigin.W);
				SetShaderValue(Shader->GetVertexShader(), CameraPosParameter, CameraPos);

				const FLinearColor D1Colour = Settings->AtmosD1_Colour.ReinterpretAsLinear();
				const FLinearColor D2Colour = Settings->AtmosD2_Colour.ReinterpretAsLinear();
				const FLinearColor H1Colour = Settings->AtmosH1_Colour.ReinterpretAsLinear();
				const FLinearColor H2Colour = Settings->AtmosH2_Colour.ReinterpretAsLinear();
				SetShaderValue(Shader->GetVertexShader(), AtmosD1ColourParameter, D1Colour);
				SetShaderValue(Shader->GetVertexShader(), AtmosD2ColourParameter, D2Colour);
				SetShaderValue(Shader->GetVertexShader(), AtmosH1ColourParameter, H1Colour);
				SetShaderValue(Shader->GetVertexShader(), AtmosH2ColourParameter, H2Colour);

				const FVector4 AtmosDensity(
					D1Colour.A * Settings->AtmosD1_Density,
					D2Colour.A * Settings->AtmosD2_Density,
					Settings->AtmosH1_Density * 0.000001f,
					Settings->AtmosH2_Density * 0.000001f);
				SetShaderValue(Shader->GetVertexShader(), AtmosDensityParameter, AtmosDensity);

				const FVector4 AtmosStartEnd(
					Settings->AtmosD1_DistanceStart,
					1.0f / (Settings->AtmosD1_DistanceEnd - Settings->AtmosD1_DistanceStart),
					Settings->AtmosD2_DistanceStart,
					1.0f / (Settings->AtmosD2_DistanceEnd - Settings->AtmosD2_DistanceStart));
				SetShaderValue(Shader->GetVertexShader(), AtmosStartEndParameter, AtmosStartEnd);

				const FLOAT H1Exponent = Clamp<FLOAT>((Settings->AtmosH1_GradientPosition - View.ViewOrigin.Z) / Settings->AtmosH1_GradientSize, -50.0f, 50.0f);
				const FLOAT H2Exponent = Clamp<FLOAT>((Settings->AtmosH2_GradientPosition - View.ViewOrigin.Z) / Settings->AtmosH2_GradientSize, -50.0f, 50.0f);
				const FVector4 AtmosGradSizeHeightIntegral(
					1.0f / Settings->AtmosH1_GradientSize,
					appExp(H1Exponent) * Settings->AtmosH1_Density * 0.000001f,
					1.0f / Settings->AtmosH2_GradientSize,
					appExp(H2Exponent) * Settings->AtmosH2_Density * 0.000001f);
				SetShaderValue(Shader->GetVertexShader(), AtmosGradSizeHeightIntegralParameter, AtmosGradSizeHeightIntegral);

				FLinearColor GlobalGradientColour = Settings->AtmosGlobal_Gradient_Colour.ReinterpretAsLinear();
				if (Settings->AtmosGlobal_Gradient_Density > 0.0f)
				{
					if (Settings->AtmosGlobal_Gradient_Density >= 1.0f)
					{
						GlobalGradientColour *= Settings->AtmosGlobal_Gradient_Density;
					}
					else
					{
						GlobalGradientColour.R = (GlobalGradientColour.R - 1.0f) * Settings->AtmosGlobal_Gradient_Density + 1.0f;
						GlobalGradientColour.G = (GlobalGradientColour.G - 1.0f) * Settings->AtmosGlobal_Gradient_Density + 1.0f;
						GlobalGradientColour.B = (GlobalGradientColour.B - 1.0f) * Settings->AtmosGlobal_Gradient_Density + 1.0f;
					}
				}
				SetShaderValue(Shader->GetVertexShader(), AtmosGlobalGradientColourParameter, GlobalGradientColour);

				FVector GlobalGradientDirection = Settings->AtmosGlobal_Gradient_Direction;
				GlobalGradientDirection.Normalize();
				SetShaderValue(Shader->GetVertexShader(), AtmosGlobalGradientDirectionParameter, GlobalGradientDirection);

				const FVector4 AtmosH1H2Alpha(D1Colour.A * Settings->AtmosD1_Density, D2Colour.A * Settings->AtmosD2_Density, 0.0f, 0.0f);
				SetShaderValue(Shader->GetVertexShader(), AtmosH1H2AlphaParameter, AtmosH1H2Alpha);
			}
		}

		FShaderParameter AtmosDensityParameter;
		FShaderParameter AtmosStartEndParameter;
		FShaderParameter AtmosGradSizeHeightIntegralParameter;
		FShaderParameter AtmosD1ColourParameter;
		FShaderParameter AtmosD2ColourParameter;
		FShaderParameter AtmosH1H2AlphaParameter;
		FShaderParameter AtmosH1ColourParameter;
		FShaderParameter AtmosH2ColourParameter;
		FShaderParameter CameraPosParameter;
		FShaderParameter AtmosGlobalGradientColourParameter;
		FShaderParameter AtmosGlobalGradientDirectionParameter;
	};

	typedef FFogRockAtmosShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_LinearHalfspace;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		const EBlendMode BlendMode = Material->GetBlendMode();
		return Material->IsUsedWithPerVertexRockAtmosFog() && (BlendMode == BLEND_Translucent || BlendMode == BLEND_Additive);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_ROCKATMOS"),TEXT("1"));
	}
};
#endif

class FConeDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Cone;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
#if BATMAN
		return FALSE;
#else
		//not fully implemented
		return FALSE;
#endif
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_CONEDENSITY"),TEXT("1"));
	}
};

/**
* A vertex shader for rendering meshes during the integral accumulation passes.
*/
template<class DensityFunctionPolicy>
class TFogIntegralVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(TFogIntegralVertexShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes()
			&& DensityFunctionPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TFogIntegralVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	TFogIntegralVertexShader()
	{
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
};

/**
* A pixel shader for rendering meshes during the integral accumulation passes.
* Density function-specific parameters are packed into FirstDensityFunctionParameters and SecondDensityFunctionParameters
*/
template<class DensityFunctionPolicy>
class TFogIntegralPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TFogIntegralPixelShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes()
			&& DensityFunctionPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TFogIntegralPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		DepthFilterSampleOffsets.Bind(Initializer.ParameterMap,TEXT("DepthFilterSampleOffsets"),TRUE);
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
		CameraPosParameter.Bind(Initializer.ParameterMap,TEXT("FogCameraPosition"),TRUE);
		FaceParameter.Bind(Initializer.ParameterMap,TEXT("FaceScale"),TRUE);
		FirstDensityFunctionParameters.Bind(Initializer.ParameterMap,TEXT("FirstDensityFunctionParameters"),TRUE);
		SecondDensityFunctionParameters.Bind(Initializer.ParameterMap,TEXT("SecondDensityFunctionParameters"),TRUE);
		StartDistanceParameter.Bind(Initializer.ParameterMap,TEXT("StartDistance"),TRUE);
#if !BATMAN
		MaxDistanceParameter.Bind(Initializer.ParameterMap,TEXT("MaxDistance"),TRUE);
#endif
		InvMaxIntegralParameter.Bind(Initializer.ParameterMap,TEXT("InvMaxIntegral"), TRUE);
	}

	TFogIntegralPixelShader()
	{
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bBackFace)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);

		const FVector2D SceneDepthTexelSize = FVector2D( 1.0f / (FLOAT)GSceneRenderTargets.GetBufferSizeX(), 1.0f / (FLOAT)GSceneRenderTargets.GetBufferSizeY());

		const UINT NumDepthFilterSamples = 2;
		static const FVector4 SampleOffsets[NumDepthFilterSamples] =
		{
			//sample the texel to the left and above
			FVector4(-SceneDepthTexelSize.X, 0, 0, SceneDepthTexelSize.Y), 
			//sample the texel to the right and below
			FVector4(SceneDepthTexelSize.X, 0, 0, -SceneDepthTexelSize.Y)
		};

		SetPixelShaderValues(GetPixelShader(),DepthFilterSampleOffsets,SampleOffsets,NumDepthFilterSamples);

		//Transform from post projection space to world space, used to reconstruct world space positions to calculate the fog integral
		FMatrix ScreenToWorld = 
			FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) * View.InvTranslatedViewProjectionMatrix;

		SetPixelShaderValue( GetPixelShader(), ScreenToWorldParameter, ScreenToWorld );

		const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
		SetPixelShaderValue( GetPixelShader(), CameraPosParameter, TranslatedViewOrigin);

		//set the face parameter with 1 if a backface is being rendered, or -1 if a frontface is being rendered
		SetPixelShaderValue( GetPixelShader(), FaceParameter, bBackFace ? 1.0f : -1.0f);
		
		//set the parameters specific to the DensityFunctionPolicy
		SetPixelShaderValue( GetPixelShader(), FirstDensityFunctionParameters, DensitySceneInfo->GetFirstDensityFunctionParameters(View));
		SetPixelShaderValue( GetPixelShader(), SecondDensityFunctionParameters, DensitySceneInfo->GetSecondDensityFunctionParameters(View));
		SetPixelShaderValue( GetPixelShader(), StartDistanceParameter, DensitySceneInfo->StartDistance);
#if !BATMAN
		SetPixelShaderValue( GetPixelShader(), MaxDistanceParameter, DensitySceneInfo->MaxDistance);
#endif
		SetPixelShaderValue( GetPixelShader(), InvMaxIntegralParameter, 1.0f / DensitySceneInfo->GetMaxIntegral());
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;	
		Ar << DepthFilterSampleOffsets;
		Ar << ScreenToWorldParameter;
		Ar << CameraPosParameter;
		Ar << FaceParameter;
		Ar << FirstDensityFunctionParameters;
		Ar << SecondDensityFunctionParameters;
		Ar << StartDistanceParameter;
#if !BATMAN
		Ar << MaxDistanceParameter;
#endif
		Ar << InvMaxIntegralParameter;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter DepthFilterSampleOffsets;
	FShaderParameter ScreenToWorldParameter;
	FShaderParameter CameraPosParameter;
	FShaderParameter FaceParameter;
	FShaderParameter FirstDensityFunctionParameters;
	FShaderParameter SecondDensityFunctionParameters;
	FShaderParameter StartDistanceParameter;
#if !BATMAN
	FShaderParameter MaxDistanceParameter;
#endif
	FShaderParameter InvMaxIntegralParameter;
};


/**
* Policy for accumulating fog line integrals
*/
template<class DensityFunctionPolicy>
class TFogIntegralDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	*/
	TFogIntegralDrawingPolicy(const FVertexFactory* InVertexFactory,const FMaterialRenderProxy* InMaterialRenderProxy);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return TRUE if the draw policies are a match
	*/
	UBOOL Matches(const TFogIntegralDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void DrawShared(
		const FViewInfo* View,
		FBoundShaderStateRHIParamRef BoundShaderState, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bBackFace) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

private:
	TFogIntegralVertexShader<DensityFunctionPolicy>* VertexShader;
	TFogIntegralPixelShader<DensityFunctionPolicy>* PixelShader;
};

/**
* Fog integral mesh drawing policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
template<class DensityFunctionPolicy>
class TFogIntegralDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };

	/**
	* Render a dynamic mesh using a fog integral mesh drawing policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo)
	{
		TFogIntegralDrawingPolicy<DensityFunctionPolicy> DrawingPolicy(
			Mesh.VertexFactory,
			Mesh.MaterialRenderProxy
			);
		DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()), DensitySceneInfo, bBackFace);
		DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,bBackFace,typename TFogIntegralDrawingPolicy<DensityFunctionPolicy>::ElementDataType());
		DrawingPolicy.DrawMesh(Mesh);

		return TRUE;
	}
};


/**
* A vertex shader for rendering meshes during the fog apply pass.
*/
class FFogVolumeApplyVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FFogVolumeApplyVertexShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes();
	}

	FFogVolumeApplyVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	FFogVolumeApplyVertexShader()
	{
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
};


/**
* A pixel shader for rendering meshes during the fog apply pass.
*/
class FFogVolumeApplyPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FFogVolumeApplyPixelShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes();
	}

	FFogVolumeApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		MaxIntegralParameter.Bind(Initializer.ParameterMap,TEXT("MaxIntegral"), TRUE);
		MaterialParameters.Bind(Initializer.ParameterMap);
		AccumulatedFrontfacesLineIntegralTextureParam.Bind(Initializer.ParameterMap,TEXT("AccumulatedFrontfacesLineIntegralTexture"), TRUE);
		AccumulatedBackfacesLineIntegralTextureParam.Bind(Initializer.ParameterMap,TEXT("AccumulatedBackfacesLineIntegralTexture"), TRUE);
	}

	FFogVolumeApplyPixelShader()
	{
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const FFogVolumeDensitySceneInfo* DensitySceneInfo)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);

		//set the fog integral accumulation samplers so that the apply pass can lookup the result of the accumulation passes
		//linear filter since we're upsampling
		SetTextureParameter(
			GetPixelShader(),
			AccumulatedFrontfacesLineIntegralTextureParam,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFogFrontfacesIntegralAccumulationTexture()
			);

		SetTextureParameter(
			GetPixelShader(),
			AccumulatedBackfacesLineIntegralTextureParam,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFogBackfacesIntegralAccumulationTexture()
			);

		SetPixelShaderValue( GetPixelShader(), MaxIntegralParameter, DensitySceneInfo->GetMaxIntegral());
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshElement& Mesh,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaxIntegralParameter;
		Ar << MaterialParameters;	
		Ar << AccumulatedFrontfacesLineIntegralTextureParam;
		Ar << AccumulatedBackfacesLineIntegralTextureParam;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FShaderParameter MaxIntegralParameter;
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderResourceParameter AccumulatedFrontfacesLineIntegralTextureParam;
	FShaderResourceParameter AccumulatedBackfacesLineIntegralTextureParam;
};


/**
* Policy for applying fog contribution to scene color
*/
class FFogVolumeApplyDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	*/
	FFogVolumeApplyDrawingPolicy(
		const FVertexFactory* InVertexFactory, 
		const FMaterialRenderProxy* InMaterialRenderProxy, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bOverrideWithShaderComplexity);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return TRUE if the draw policies are a match
	*/
	UBOOL Matches(const FFogVolumeApplyDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void DrawShared(
		const FViewInfo* View,
		FBoundShaderStateRHIParamRef BoundShaderState, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

private:
	FFogVolumeApplyVertexShader* VertexShader;
	FFogVolumeApplyPixelShader* PixelShader;
	UINT NumIntegralInstructions;
};

/**
* Fog apply mesh drawing policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
class FFogVolumeApplyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };
	struct ContextType {};

	/**
	* Render a dynamic mesh using a fog apply mesh draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshElement& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo
		);
};

extern void ResetFogVolumeIndex();

/**
* Render a Fog Volume.  The density function to use is found in PrimitiveSceneInfo->Scene's FogVolumes map.
* @return TRUE if the mesh rendered
*/
extern UBOOL RenderFogVolume(
	 const FViewInfo* View,
	 const FMeshElement& Mesh,
	 UBOOL bBackFace,
	 UBOOL bPreFog,
	 const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	 FHitProxyId HitProxyId);
