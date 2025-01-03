/*=============================================================================
LandscapeRender.h: New terrain rendering
Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _LANDSCAPERENDER_H
#define _LANDSCAPERENDER_H

class FLandscapeVertexFactory;
class FLandscapeVertexBuffer;
class FLandscapeComponentSceneProxy;
#include "../Src/ScenePrivate.h"


#define MAX_LANDSCAPE_SUBSECTIONS 3

#if WITH_EDITOR
namespace ELandscapeViewMode
{
	enum Type
	{
		Invalid = -1,
		/** Color only */
		Normal = 0,
		EditLayer,
		/** Layer debug only */
		DebugLayer,
	};
}

extern ELandscapeViewMode::Type GLandscapeViewMode;
#endif

#if PS3
//
// FPS3LandscapeHeightVertexBuffer
//
class FPS3LandscapeHeightVertexBuffer : public FVertexBuffer
{
public:
	/** Constructor. */
	FPS3LandscapeHeightVertexBuffer()
	{
		bInitialized = TRUE;
	}

	/** Destructor. */
	virtual ~FPS3LandscapeHeightVertexBuffer()
	{
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
	}
};

#endif

/** vertex factory for VTF-heightmap terrain  */
class FLandscapeVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLandscapeVertexFactory);
	FLandscapeComponentSceneProxy* SceneProxy;
public:

	FLandscapeVertexFactory(FLandscapeComponentSceneProxy* InSceneProxy)
	:	SceneProxy(InSceneProxy)
	{}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	FLandscapeComponentSceneProxy* GetSceneProxy()
	{
		return SceneProxy;
	}

	struct DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;
#if PS3
		/** The stream created by the SPU code on PS3 */
		FVertexStreamComponent PS3HeightComponent;
#endif
	};

	/**
	* Should we cache the material's shadertype on this platform with this vertex factory? 
	*/
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
#if MOBILE
		return FALSE;
#endif

		// only compile landscape materials for landscape vertex factory
		// The special engine materials must be compiled for the landscape vertex factory because they are used with it for wireframe, etc.
		return (Platform==SP_PCD3D_SM3 || Platform==SP_PCD3D_SM4 || Platform==SP_PCD3D_SM5 || Platform==SP_XBOXD3D || Platform==SP_PS3) && (Material->IsUsedWithLandscape() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
	}

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("PER_PIXEL_TANGENT_BASIS"),TEXT("1"));
	}

	// FRenderResource interface.
	virtual void InitRHI();

	static UBOOL SupportsTessellationShaders() { return TRUE; }

	/** stream component data bound to this vertex factory */
	DataType Data;  

#if PS3
	static FPS3LandscapeHeightVertexBuffer GPS3LandscapeHeightVertexBuffer;
#endif
};

//
// FLandscapeVertexBuffer
//
class FLandscapeVertexBuffer : public FVertexBuffer, public FRefCountedObject
{
	INT SizeVerts;
public:
	struct FLandscapeVertex
	{
		FLOAT VertexX;
		FLOAT VertexY;
	};
	
	/** Constructor. */
	FLandscapeVertexBuffer(INT InSizeVerts)
	: 	SizeVerts(InSizeVerts)
	{
		InitResource();
	}

	/** Destructor. */
	virtual ~FLandscapeVertexBuffer()
	{
		ReleaseResource();
	}

	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI();
};

//
// FLandscapeIndexBuffer
//
class FLandscapeIndexBuffer : public FRawStaticIndexBuffer, public FRefCountedObject
{
public:
	FLandscapeIndexBuffer(INT SizeQuads, INT VBSizeVertices);

	/** Destructor. */
	virtual ~FLandscapeIndexBuffer()
	{
		ReleaseResource();
	}
};

//extern UMaterial* GLayerDebugColorMaterial;

//
// FLandscapeEditToolRenderData
//
struct FLandscapeEditToolRenderData
{
	FLandscapeEditToolRenderData()
	:	ToolMaterial(NULL),
		LandscapeComponent(NULL),
		bSelected(FALSE),
		DebugChannelR(INDEX_NONE),
		DebugChannelG(INDEX_NONE),
		DebugChannelB(INDEX_NONE)
	{}

	FLandscapeEditToolRenderData(ULandscapeComponent* InComponent)
		:	ToolMaterial(NULL),
			LandscapeComponent(InComponent),
			bSelected(FALSE),
			DebugChannelR(INDEX_NONE),
			DebugChannelG(INDEX_NONE),
			DebugChannelB(INDEX_NONE)
	{}

	// Material used to render the tool.
	UMaterialInterface* ToolMaterial;

	ULandscapeComponent* LandscapeComponent;

	// Component is selected
	UBOOL bSelected;
	INT DebugChannelR, DebugChannelG, DebugChannelB;

#ifdef WITH_EDITOR
	void UpdateDebugColorMaterial();
	void UpdateSelectionMaterial(UBOOL bSelected);
#endif

	// Game thread update
	void Update( UMaterialInterface* InNewToolMaterial )
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateEditToolRenderData,
			FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, this,
			UMaterialInterface*, NewToolMaterial, InNewToolMaterial,
		{
			LandscapeEditToolRenderData->ToolMaterial = NewToolMaterial;
		});
	}

	// Allows game thread to queue the deletion by the render thread
	void Cleanup()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			CleanupEditToolRenderData,
			FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, this,
		{
			delete LandscapeEditToolRenderData;
		});
	}
};

//
// FLandscapeComponentSceneProxy
//
class FLandscapeComponentSceneProxy : public FPrimitiveSceneProxy
{
	class FLandscapeLCI : public FLightCacheInterface
	{
	public:
		/** Initialization constructor. */
		FLandscapeLCI(const ULandscapeComponent* InComponent)
		{
			Component = InComponent;
		}

		// FLightCacheInterface
		virtual FLightInteraction GetInteraction(const class FLightSceneInfo* LightSceneInfo) const
		{
			for (INT LightIndex = 0; LightIndex < Component->IrrelevantLights.Num(); LightIndex++)
			{
				if(Component->IrrelevantLights(LightIndex) == LightSceneInfo->LightGuid)
				{
					return FLightInteraction::Irrelevant();
				}
			}

			if(Component->LightMap && Component->LightMap->LightGuids.ContainsItem(LightSceneInfo->LightmapGuid))
			{
				return FLightInteraction::LightMap();
			}

			for(INT LightIndex = 0;LightIndex < Component->ShadowMaps.Num();LightIndex++)
			{
				const UShadowMap2D* const ShadowMap = Component->ShadowMaps(LightIndex);
				if(ShadowMap && ShadowMap->IsValid() && ShadowMap->GetLightGuid() == LightSceneInfo->LightGuid)
				{
					return FLightInteraction::ShadowMap2D(
						ShadowMap->GetTexture(),
						ShadowMap->GetCoordinateScale(),
						ShadowMap->GetCoordinateBias(),
						ShadowMap->IsShadowFactorTexture()
						);
				}
			}

			return FLightInteraction::Uncached();
		}

		virtual FLightMapInteraction GetLightMapInteraction() const
		{
			if (Component->LightMap)
			{
				return Component->LightMap->GetInteraction();
			}
			else
			{
				return FLightMapInteraction();
			}
		}

	private:
		/** A map from persistent light IDs to information about the light's interaction with the model element. */
		//TMap<FGuid,FLightInteraction> StaticLightInteractionMap;

		/** The light-map used by the element. */
		const ULandscapeComponent* Component;
	};

	INT						MaxLOD;
	INT						ComponentSizeQuads;	// Size of component in quads
	INT						NumSubsections;
	INT						SubsectionSizeQuads;
	INT						SubsectionSizeVerts;
	INT						SectionBaseX;
	INT						SectionBaseY;
	FLOAT					DrawScaleXY;
	FVector					ActorOrigin;
	INT						StaticLightingResolution;

	// values set during rendering
	INT						CurrentLOD;
	INT						CurrentSubX;
	INT						CurrentSubY;

	// Values for light-map
	INT						PatchExpandCount;

	FVector4 WeightmapScaleBias;
	FLOAT WeightmapSubsectionOffset;
	FVector2D LayerUVPan;

	UTexture2D* HeightmapTexture;
	FVector4 HeightmapScaleBias;
	FLOAT HeightmapSubsectionOffset;

	UTexture2D* XYOffsetTexture;
	FVector4 XYOffsetUVScaleBias;
	FLOAT XYOffsetSubsectionOffset;
	FLOAT XYOffsetScale;

	FVector4 LightmapScaleBias;

	FLandscapeVertexFactory* VertexFactory;
	FLandscapeVertexBuffer* VertexBuffer;
	FLandscapeIndexBuffer** IndexBuffers;
	
	UMaterialInterface* MaterialInterface;
	FMaterialViewRelevance MaterialViewRelevance;

	// Reference counted vertex buffer shared among all landscape scene proxies
	static FLandscapeVertexBuffer* SharedVertexBuffer;
	static FLandscapeIndexBuffer** SharedIndexBuffers;

	FLandscapeEditToolRenderData* EditToolRenderData;

	// FLightCacheInterface
	FLandscapeLCI* ComponentLightInfo;

#if PS3
	// for PS3
	void* PlatformData;
#endif

	virtual ~FLandscapeComponentSceneProxy();

public:

	FLandscapeComponentSceneProxy(ULandscapeComponent* InComponent, FLandscapeEditToolRenderData* InEditToolRenderData);

	// FPrimitiveSceneProxy interface.

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/**
	*	Called when the rendering thread adds the proxy to the scene.
	*	This function allows for generating renderer-side resources.
	*/
	virtual UBOOL CreateRenderThreadResources();

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneInfo			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneInfo* LightSceneInfo, UBOOL& bDynamic, UBOOL& bRelevant, UBOOL& bLightMapped) const;

	friend class FLandscapeVertexFactoryShaderParameters;
	friend class FLandscapeVertexFactoryPixelShaderParameters;
};

class FLandscapeDebugMaterialRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const UTexture2D* RedTexture;
	const UTexture2D* GreenTexture;
	const UTexture2D* BlueTexture;
	const FLinearColor R;
	const FLinearColor G;
	const FLinearColor B;

	/** Initialization constructor. */
	FLandscapeDebugMaterialRenderProxy(const FMaterialRenderProxy* InParent, const UTexture2D* TexR, const UTexture2D* TexG, const UTexture2D* TexB, 
		const FLinearColor& InR, const FLinearColor& InG, const FLinearColor& InB ):
			Parent(InParent),
			RedTexture(TexR),
			GreenTexture(TexG),
			BlueTexture(TexB),
			R(InR),
			G(InG),
			B(InB)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const
	{
		return Parent->GetMaterial();
	}
	virtual UBOOL GetVectorValue(const FName& ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == NAME_Landscape_RedMask )
		{
			*OutValue = R;
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_GreenMask )
		{
			*OutValue = G;
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_BlueMask )
		{
			*OutValue = B;
			return TRUE;
		}
		else
		{
			return Parent->GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual UBOOL GetScalarValue(const FName& ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName& ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == NAME_Landscape_RedTexture )
		{
			*OutValue = (RedTexture ? RedTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_GreenTexture )
		{
			*OutValue = (GreenTexture ? GreenTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else if (ParameterName == NAME_Landscape_BlueTexture )
		{
			*OutValue = (BlueTexture ? BlueTexture->Resource : GBlackTexture);
			return TRUE;
		}
		else
		{
			return Parent->GetTextureValue(ParameterName, OutValue, Context);
		}
	}
};


namespace 
{
	static FLOAT GetTerrainExpandPatchCount(INT LightMapRes, INT& X, INT& Y, INT ComponentSize, INT& DesiredSize)
	{
		if (LightMapRes <= 0) return 0.f;
		// Assuming DXT_1 compression at the moment...
		INT PixelPaddingX = GPixelFormats[PF_DXT1].BlockSizeX;
		INT PixelPaddingY = GPixelFormats[PF_DXT1].BlockSizeY;
/*
		if (GAllowLightmapCompression == FALSE)
		{
			PixelPaddingX = GPixelFormats[PF_A8R8G8B8].BlockSizeX;
			PixelPaddingY = GPixelFormats[PF_A8R8G8B8].BlockSizeY;
		}
*/

		INT PatchExpandCountX = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingX) / LightMapRes;
		INT PatchExpandCountY = (TERRAIN_PATCH_EXPAND_SCALAR * PixelPaddingY) / LightMapRes;

		X = Max<INT>(1, PatchExpandCountX);
		Y = Max<INT>(1, PatchExpandCountY);

		DesiredSize = Min<INT>((ComponentSize + 1) * LightMapRes, 4096);
		INT CurrentSize = Min<INT>((2*X + ComponentSize + 1) * LightMapRes, 4096);

		// Find proper Lightmap Size
		if (CurrentSize > DesiredSize)
		{
			// Find maximum bit
			INT PriorSize = DesiredSize;
			while (DesiredSize > 0)
			{
				PriorSize = DesiredSize;
				DesiredSize = DesiredSize & ~(DesiredSize & ~(DesiredSize-1));
			}

			DesiredSize = PriorSize << 1; // next bigger size
			if ( CurrentSize * CurrentSize <= ((PriorSize * PriorSize) << 1)  )
			{
				DesiredSize = PriorSize;
			}
		}

		INT DestSize = (FLOAT)DesiredSize / CurrentSize * (ComponentSize*LightMapRes);
		//FLOAT LightMapRatio = (FLOAT)DestSize / ComponentSize;
		FLOAT LightMapRatio = (FLOAT)DestSize / (ComponentSize*LightMapRes) * CurrentSize / DesiredSize;
		return LightMapRatio;
		//X = Y = 1;
		//return 1.0f;
	}
}

#endif // _LANDSCAPERENDER_H