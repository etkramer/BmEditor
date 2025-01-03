/******************************************************************************/
/*                                                                            */
/*      Copyright DARKWORKS, 2008-2009. Patent-pending technology by TRIOVIZ.         */
/*                                                                            */
/******************************************************************************/

#include "EnginePrivate.h"
#include "DwTrioviz/DwTriovizImpl.h"
#include "../Src/ScenePrivate.h"

IMPLEMENT_CLASS(UDwTriovizImplEffect);

#if DWTRIOVIZSDK

#include "ScenePrivate.h"
#include "SceneRenderTargets.h"
#include "SceneFilterRendering.h"

#define DWTRIOVIZ_SPLITSCREEN_PLAYER_MAX	16

FDwSceneViewCache::FDwSceneViewCache():DeltaWorldTime(1.0f / 30.0f), NumView(1), 
	X(0), Y(0), SizeX(1280), SizeY(720),
	RenderTargetX(0), RenderTargetY(0), RenderTargetSizeX(1280), RenderTargetSizeY(720), DisplayGamma(1.0f), DOF_FocusDistance(0.0f),
	bUseLDRSceneColor(TRUE), bResolveScene(FALSE), bIsLastView(FALSE), bUseNormalDevSpace(FALSE), bUseMoviePackingWithDisparity(FALSE), bUseZFromAlpha(FALSE),
	bUseSmoothTransition(TRUE)
{
}

FDwSceneViewCache::FDwSceneViewCache(FViewInfo * ViewInfo):DeltaWorldTime(1.0f / 30.0f), NumView(1), 
	X(0), Y(0), SizeX(1280), SizeY(720),
	RenderTargetX(0), RenderTargetY(0), RenderTargetSizeX(1280), RenderTargetSizeY(720), DisplayGamma(1.0f), DOF_FocusDistance(0.0f),
	bUseLDRSceneColor(TRUE), bResolveScene(FALSE), bIsLastView(FALSE), bUseNormalDevSpace(FALSE), bUseMoviePackingWithDisparity(FALSE), bUseZFromAlpha(FALSE),
	bUseSmoothTransition(TRUE)
{
	if (ViewInfo && ViewInfo->Family)
	{
		DeltaWorldTime = ViewInfo->Family->DeltaWorldTime;
		NumView = ViewInfo->Family->Views.Num();
		RenderTargetX = ViewInfo->RenderTargetX;
		RenderTargetY = ViewInfo->RenderTargetY;
		RenderTargetSizeX = ViewInfo->RenderTargetSizeX;
		RenderTargetSizeY = ViewInfo->RenderTargetSizeY;
		InvDeviceZToWorldZTransform = ViewInfo->InvDeviceZToWorldZTransform;
		ProjectionMatrix = ViewInfo->ProjectionMatrix;
		bUseLDRSceneColor = ViewInfo->bUseLDRSceneColor;
		bResolveScene = ViewInfo->Family->bResolveScene;
		X = ViewInfo->X; 
		Y = ViewInfo->Y;
		SizeX = ViewInfo->SizeX;
		SizeY = ViewInfo->SizeY;
		OverlayColor = ViewInfo->OverlayColor;
		ColorScale = ViewInfo->ColorScale;
		DOF_FocusDistance = ViewInfo->DepthOfFieldParams.FocusDistance;
		
		if (ViewInfo->Family->RenderTarget)
		{
			DisplayGamma = ViewInfo->Family->RenderTarget->GetDisplayGamma();
			RenderTargetSurface = ViewInfo->Family->RenderTarget->GetRenderTargetSurface();
		}		

		bIsLastView = (ViewInfo->Family->Views.Last() == ViewInfo);
	}
}

FVector4 CreateInvDeviceZToWorldZTransform(FMatrix const & ProjectionMatrix, UBOOL InvertZ);

FDwSceneViewCache g_DwSplitScreeenViewInfo[DWTRIOVIZ_SPLITSCREEN_PLAYER_MAX];
INT g_DwSplitPlayerCount = 0;
UBOOL g_DwCaptureDepth = FALSE;

/*-----------------------------------------------------------------------------
>>>>>>>> UDwTriovizImplEffect
-----------------------------------------------------------------------------*/

class FDwTriovizProcessSceneProxy : public FPostProcessSceneProxy
{
public: 
	/** 
	* Initialization constructor. 
	* @param InEffect - effect to mirror in this proxy
	*/
	FDwTriovizProcessSceneProxy(const UDwTriovizImplEffect* InEffect,const FPostProcessSettings* WorldSettings) :
	  FPostProcessSceneProxy(InEffect)
	  {
		  // Not use WorldSettings (use ProfileMgr instead)
	  }

	// Render is done after gamma correction in LDR space
#ifdef DECLARE_CLASS_INTRINSIC
	virtual UBOOL Render(const class FScene* Scene, UINT InDepthPriorityGroup,class FViewInfo& View,const FMatrix& CanvasTransform,struct FSceneColorLDRInfo& LDRInfo)
	{
		DWORD UsageFlagsLDR = RTUsage_FullOverwrite;
		if ( LDRInfo.bAdjustPingPong && (LDRInfo.NumPingPongsRemaining & 1) )
		{
			UsageFlagsLDR |= RTUsage_DontSwapBuffer;
		}
#else
	virtual UBOOL Render(const class FScene* Scene, UINT InDepthPriorityGroup,class FViewInfo& View,const FMatrix& CanvasTransform)
	{
		DWORD UsageFlagsLDR = RTUsage_FullOverwrite;
#endif	

		if (Scene == NULL)//Be sure we are WorldPostProcess
		{
			return FALSE;
		}

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		if (g_DwUseFramePacking && View.Family->Views.Num() == 1)
		{
			g_DwSplitScreeenViewInfo[0] = FDwSceneViewCache(&View);
		}		
#endif
		//For splitscreen postpone the effect after the rendering of all the post process of all views
		if (View.Family->Views.Num() > 1 && g_DwSplitPlayerCount < DWTRIOVIZ_SPLITSCREEN_PLAYER_MAX)
		{
			g_DwSplitScreeenViewInfo[g_DwSplitPlayerCount++] = FDwSceneViewCache(&View);
		}
		else
		{
			FDwSceneViewCache ViewCache(&View);
			DwTriovizImpl_Render(&ViewCache, FinalEffectInGroup, UsageFlagsLDR);
		}		

		// Scene color needs to be resolved before being read again.
		return TRUE;
	}
};

/**
* Creates a proxy to represent the render info for a post process effect
* @param WorldSettings - The world's post process settings for the view.
* @return The proxy object.
*/
FPostProcessSceneProxy* UDwTriovizImplEffect::CreateSceneProxy(const FPostProcessSettings* /* WorldSettings */)
{
	return new FDwTriovizProcessSceneProxy(this, NULL);
}

/**
* @param View - current view
* @return TRUE if the effect should be rendered
*/
UBOOL UDwTriovizImplEffect::IsShown(const FSceneView* View) const
{
	return (DwTriovizImpl_IsTriovizActive() && UPostProcessEffect::IsShown(View));
}

/**
* Switches the capture of depth
*/
void DwTriovizImpl_SwitchCaptureDepth(void)
{
	g_DwCaptureDepth = !g_DwCaptureDepth;
}

/**
* Tell if Trioviz captures depth
*/
UBOOL DwTriovizImpl_IsTriovizCapturingDepth(void)
{
// EPICSTART
	return g_DwCaptureDepth;
// EPICEND
}


/**
* Called when properties change.
*/
void UDwTriovizImplEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// DwTriovizProcessEffect should only ever exists in the SDPG_PostProcess scene
	SceneDPG=SDPG_PostProcess;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}





/*-----------------------------------------------------------------------------
>>>>>>> TriovizSDK Implementation
-----------------------------------------------------------------------------*/


/// "__OVERWRITE____xxx" defines must be defined before including "__DwTriovizSDK_Implementation.h" file

#if WIN32

	#define __OVERWRITE____D3D9_DwTrioviz_DebugDrawText				1
	#define __OVERWRITE____D3D9_DwTrioviz_DebugDrawLine				1
	#define __OVERWRITE____D3D9_DwTrioviz_RestoreEngineRenderStates 1
	#define __OVERWRITE____D3D9_DwTrioviz_SetRenderTarget			1

#elif XBOX

	#define __OVERWRITE____XBOX_DwTrioviz_DebugDrawText				1
	#define __OVERWRITE____XBOX_DwTrioviz_DebugDrawLine				1
	#define __OVERWRITE____XBOX_DwTrioviz_RestoreEngineRenderStates 1
	#define __OVERWRITE____XBOX_DwTrioviz_SetRenderTarget			1

#elif PS3

	#define __OVERWRITE____PS3_DwTrioviz_DebugDrawText				1
	#define __OVERWRITE____PS3_DwTrioviz_DebugDrawLine				1
	#define __OVERWRITE____PS3_DwTrioviz_RestoreEngineRenderStates	1
	#define __OVERWRITE____PS3_DwTrioviz_Memory						1
	#define __OVERWRITE____PS3_DwTrioviz_SetRenderTarget			1

#endif



/// Include SDK main Header "__DwTriovizSDK_Implementation.h" file

#if WIN32
	#if _WIN64
	#pragma pack(push,16)
	#else
	#pragma pack(push,8)
	#endif
#endif

// EPICSTART
// Trioviz headers are looking for FINAL_RELEASE, so define it here for ShippingPC, so that it can use the most-optimized libs
// (which only work when FINAL_RELEASE is 1)
#if SHIPPING_PC_GAME
#undef FINAL_RELEASE
#define FINAL_RELEASE   1
#endif
// EPICEND

#include "../../../../External/DwTriovizSDK/Inc/DwTriovizSDK/__DwTriovizSDK_Implementation.h"
#include "../../../../External/DwTriovizSDK/Inc/DwTriovizSDK/__DwTriovizSDK_Interface.h"

#if WIN32
	#pragma pack(pop)
#endif



/// Declare global vars & common functions

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
CellGcmSurface GDwLeftEyeSurface;
CellGcmSurface GDwRightEyeSurface;
UBOOL g_DwPendingSwitch3DModeFirst = FALSE;
static FPS3RHISurface * GDwLeftEyeStereoSurface = NULL;
static FPS3RHISurface * GDwRightEyeStereoSurface = NULL;
static FSurfaceRHIRef GDwLeftEyeStereoSurfaceRef = GDwLeftEyeStereoSurface;
static FSurfaceRHIRef GDwRightEyeStereoSurfaceRef = GDwRightEyeStereoSurface;
#endif

static bool bIsLeftEyeRendering = true;

bool DwTriovizImpl_IsRenderingLeftEye()
{
	return bIsLeftEyeRendering;
}

bool bEnable3DUI = false;

void DwTriovizImpl_Disable3DForUI()
{
	bEnable3DUI = false;
}

void DwTriovizImpl_Enable3DForUI(bool bEnable = true)
{
	bEnable3DUI = bEnable;
}

void DwTriovizImpl_Switch3DForUI()
{
	bEnable3DUI = !bEnable3DUI;
}

float g_fDwTrioviz3DIntensity = 0.50f;

void DwTriovizImpl_Set3DMode(int eDwTrioviz3DMode)
{
	SDwTriovizParms *  pCurProfile = NULL;
	switch((ETrioviz3DMode)eDwTrioviz3DMode)
	{
		case ETrioviz3DMode_Off:
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		if(g_DwUseFramePacking)
		{
			DwTriovizImpl_SwitchFramePacking3D();
		}
#endif
		DwTrioviz_SetFlag(EDwTriovizFlag_InGame, false);
		break;
		case ETrioviz3DMode_ColorFilter:
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
			if(g_DwUseFramePacking)
			{
				DwTriovizImpl_SwitchFramePacking3D();
			}
#endif
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGameColorFilter);
			DwTriovizImpl_SetCurrentProfileMultiplier(g_fDwTrioviz3DIntensity);
			pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = EDwTrioviz3DMode_ColorFilter;
			break;
		case ETrioviz3DMode_SideBySide:
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
			if(g_DwUseFramePacking)
			{
				DwTriovizImpl_SwitchFramePacking3D();
			}
#endif
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGame3DTV);
			DwTriovizImpl_SetCurrentProfileMultiplier(g_fDwTrioviz3DIntensity);
			pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = EDwTrioviz3DMode_SideBySide;
			break;
		case ETrioviz3DMode_TopBottom:
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
			if(g_DwUseFramePacking)
			{
				DwTriovizImpl_SwitchFramePacking3D();
			}
#endif
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGame3DTV);
			DwTriovizImpl_SetCurrentProfileMultiplier(g_fDwTrioviz3DIntensity);
			pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = EDwTrioviz3DMode_TopBottom;
			break;
		
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		case ETrioviz3DMode_FramePacking:
			if(!g_DwUseFramePacking)
			{
				DwTriovizImpl_SwitchFramePacking3D();
			}
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGameFramePacking);
			DwTriovizImpl_SetCurrentProfileMultiplier(g_fDwTrioviz3DIntensity);
		break;
#endif
		default:
		break;
	}
}

void DwTriovizImpl_Set3DIntesity(float DwTriovizIntensityValue)
{
	if(DwTriovizIntensityValue > 1.0f)
	{
		DwTriovizIntensityValue = 1.0f;
	}else if(DwTriovizIntensityValue < 0.0f)
	{
		DwTriovizIntensityValue = 0.0f;
	}

	g_fDwTrioviz3DIntensity = DwTriovizIntensityValue;

	DwTriovizImpl_SetCurrentProfileMultiplier(g_fDwTrioviz3DIntensity);
}

bool DwTriovizImpl_SetCanvasForUIRendering(FCanvas * Canvas, UINT nSizeX, UINT nSizeY)
{
	int nDW_ScreenSizeX = nSizeX;
	int nDW_ScreenSizeY = nSizeY;
	bool bDW_RenderUI = true;
	if(!DwTriovizImpl_IsTriovizActive())
	{
		Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f, 0.0f, 0.0f)));
		return bDW_RenderUI;
	}
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280

	if(g_DwUseFramePacking)
	{
		float fDW_EyeOffset = (float)nDW_ScreenSizeX * ((float)DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5f);		//get this value from current Trioviz parameters
		if(bEnable3DUI == false)
		{
			fDW_EyeOffset = 0.0f;
		}
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, 0.0f, 0.0f)));
		}
		else
		{
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, 0.0f, 0.0f)));
		}
	}
	else
	{
		float fDW_EyeOffset = (float)nDW_ScreenSizeX * ((float)DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5f);		//get this value from current Trioviz parameters
		if(bEnable3DUI == false)
		{
			fDW_EyeOffset = 0.0f;
		}
		SDwTriovizParms *  pCurProfile;
		pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());

		switch(pCurProfile->m_3DMode.m_3DMode)
		{
		case EDwTrioviz3DMode_ColorFilter:
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f, 0.0f, 0.0f)));
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		case EDwTrioviz3DMode_SideBySide:
			if (pCurProfile->m_Global.m_InvertEyes == false)
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					Canvas->PushMaskRegion((float)(nDW_ScreenSizeX/2), 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
					Canvas->PushAbsoluteTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((float)(nDW_ScreenSizeX/2) - (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
				}
				else
				{
					Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
					Canvas->PushAbsoluteTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
				}
			}
			else
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					Canvas->PushMaskRegion((float)(nDW_ScreenSizeX/2), 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
					Canvas->PushRelativeTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((float)(nDW_ScreenSizeX/2) + (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
				}
				else
				{
					Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
					Canvas->PushRelativeTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
				}
			}
			break;
		case EDwTrioviz3DMode_TopBottom:
			if (pCurProfile->m_Global.m_InvertEyes == false)
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					Canvas->PushMaskRegion(0.0f, (float)(nDW_ScreenSizeY/2), (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
					Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, (float)(nDW_ScreenSizeY/2), 0.0f)));
				}
				else
				{
					Canvas->PushMaskRegion(0.0f, 0.0f , (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
					Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, 0.0f, 0.0f)));
				}
			}
			else
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					Canvas->PushMaskRegion(0.0f, 0.0f , (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
					Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, 0.0f, 0.0f)));
				}
				else
				{
					Canvas->PushMaskRegion(0.0f, (float)(nDW_ScreenSizeY/2), (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
					Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, (float)(nDW_ScreenSizeY/2), 0.0f)));
				}
			}
			break;
		case EDwTrioviz3DMode_LeftEye:
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, 0.0f, 0.0f)));
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		case EDwTrioviz3DMode_RightEye:
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, 0.0f, 0.0f)));
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		default:
			Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
			Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f, 0.0f, 0.0f)));
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		}

	}
#elif DWTRIOVIZSDK

	float fDW_EyeOffset = (float)nDW_ScreenSizeX * ((float)DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5f);		//get this value from current Trioviz parameters
	if(bEnable3DUI == false)
	{
		fDW_EyeOffset = 0.0f;
	}
	SDwTriovizParms *  pCurProfile;
	pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());

	switch(pCurProfile->m_3DMode.m_3DMode)
	{
	case EDwTrioviz3DMode_ColorFilter:
		Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
		Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f, 0.0f, 0.0f)));
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	case EDwTrioviz3DMode_SideBySide:
		if (pCurProfile->m_Global.m_InvertEyes == false)
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				Canvas->PushMaskRegion((float)(nDW_ScreenSizeX/2), 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
				Canvas->PushAbsoluteTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((float)(nDW_ScreenSizeX/2) - (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
			}
			else
			{
				Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
				Canvas->PushAbsoluteTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
			}
		}
		else
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				Canvas->PushMaskRegion((float)(nDW_ScreenSizeX/2), 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
				Canvas->PushRelativeTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector((float)(nDW_ScreenSizeX/2) + (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
			}
			else
			{
				Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX/2), (float)(nDW_ScreenSizeY));
				Canvas->PushRelativeTransform(FMatrix(FVector(0.5f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - (fDW_EyeOffset/2.0f), 0.0f, 0.0f)));
			}
		}
		break;
	case EDwTrioviz3DMode_TopBottom:
		if (pCurProfile->m_Global.m_InvertEyes == false)
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				Canvas->PushMaskRegion(0.0f, (float)(nDW_ScreenSizeY/2), (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
				Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, (float)(nDW_ScreenSizeY/2), 0.0f)));
			}
			else
			{
				Canvas->PushMaskRegion(0.0f, 0.0f , (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
				Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, 0.0f, 0.0f)));
			}
		}
		else
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				Canvas->PushMaskRegion(0.0f, 0.0f , (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
				Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, 0.0f, 0.0f)));
			}
			else
			{
				Canvas->PushMaskRegion(0.0f, (float)(nDW_ScreenSizeY/2), (float)nDW_ScreenSizeX, (float)(nDW_ScreenSizeY/2));
				Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,0.5f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, (float)(nDW_ScreenSizeY/2), 0.0f)));
			}
		}

		break;
	case EDwTrioviz3DMode_LeftEye:
		Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
		Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f - fDW_EyeOffset, 0.0f, 0.0f)));
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	case EDwTrioviz3DMode_RightEye:
		Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
		Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(fDW_EyeOffset, 0.0f, 0.0f)));
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	default:
		Canvas->PushMaskRegion(0.0f, 0.0f , (float)(nDW_ScreenSizeX), (float)(nDW_ScreenSizeY));
		Canvas->PushRelativeTransform(FMatrix(FVector(1.0f,0.0f,0.0f), FVector(0.0f,1.0f,0.0f), FVector(0.0f,0.0f,1.0f), FVector(0.0f, 0.0f, 0.0f)));
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	}

#endif
	return bDW_RenderUI;
}



#include "../../GFxUI/Inc/GFxUI.h"

#include "../../GFxUI/Inc/GFxUIEngine.h"
#include "../../GFxUI/Inc/GFxUIRenderer.h"

void  DwSetViewport(GFxMovieView * movie, int bufw, int bufh, int left, int top, int w, int h, float AspectRatio, int flags = 0)
{
	GViewport src;
	movie->GetViewport(&src);
	GViewport desc(bufw, bufh, left, top, w, h, src.Flags);
	desc.AspectRatio = AspectRatio;
	movie->SetViewport(desc);
}

bool DwTriovizImpl_SetViewportForGFxUIRendering(GFxMovieView * movie, UINT nSizeX, UINT nSizeY)
{
	bool bDW_RenderUI = true;
	if(!DwTriovizImpl_IsTriovizActive())
	{
		int nDW_ScreenSizeX = nSizeX;
		int nDW_ScreenSizeY = nSizeY;
		DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0, 0, nDW_ScreenSizeX, nDW_ScreenSizeY, 1.0f);
		return bDW_RenderUI;
	}

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	int nDW_ScreenSizeX = 1280;
	int nDW_ScreenSizeY = 720;
	if(g_DwUseFramePacking)
	{
		int nDW_EyeOffset = (int)((double)nDW_ScreenSizeX * (DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5));		//get this value from current Trioviz parameters
		if(bEnable3DUI == false)
		{
			nDW_EyeOffset = 0;
		}
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY, 1.0f);
		}else
		{
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY, 1.0f);
		}
	}else
	{
		int nDW_EyeOffset = (int)((double)nDW_ScreenSizeX * (DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5));		//get this value from current Trioviz parameters
		if(bEnable3DUI == false)
		{
			nDW_EyeOffset = 0;
		}
		SDwTriovizParms *  pCurProfile;
		pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());

		switch(pCurProfile->m_3DMode.m_3DMode)
		{
		case EDwTrioviz3DMode_ColorFilter:
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0, 0, nDW_ScreenSizeX, nDW_ScreenSizeY, 1.0f);
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		case EDwTrioviz3DMode_SideBySide:
			if (pCurProfile->m_Global.m_InvertEyes == false)
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, (nDW_ScreenSizeX/2) - (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2)*2, nDW_ScreenSizeY, 2.0f);
				}else
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2), nDW_ScreenSizeY, 2.0f);					
				}
			}
			else
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, (nDW_ScreenSizeX/2) + (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2), nDW_ScreenSizeY, 2.0f);
				}else
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2)*2, nDW_ScreenSizeY, 2.0f);
				}
			}
			break;
		case EDwTrioviz3DMode_TopBottom:
			if (pCurProfile->m_Global.m_InvertEyes == false)
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, nDW_ScreenSizeY/2, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY/2, 0.5f);
				}else
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY/2, 0.5f);
				}
			}
			else
			{
				if(DwTriovizImpl_IsRenderingLeftEye())
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY/2, 0.5f);
				}else
				{
					DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, nDW_ScreenSizeY/2, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY/2, 0.5f);
				}
			}
			break;
		case EDwTrioviz3DMode_LeftEye:
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY, 1.0f);
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		case EDwTrioviz3DMode_RightEye:
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY, 1.0f);
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		default:
			DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0, 0, nDW_ScreenSizeX, nDW_ScreenSizeY, 1.0f);
			if(!DwTriovizImpl_IsRenderingLeftEye())
			{
				bDW_RenderUI = false;
			}
			break;
		}

	}
#elif DWTRIOVIZSDK

	int nDW_ScreenSizeX = nSizeX;
	int nDW_ScreenSizeY = nSizeY;
	int nDW_EyeOffset = nDW_ScreenSizeX * (DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor() * 1.5);		//get this value from current Trioviz parameters
	if(bEnable3DUI == false)
	{
		nDW_EyeOffset = 0;
	}
	SDwTriovizParms *  pCurProfile;
	pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());

	switch(pCurProfile->m_3DMode.m_3DMode)
	{
	case EDwTrioviz3DMode_ColorFilter:
		DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0, 0, nDW_ScreenSizeX, nDW_ScreenSizeY, 1.0f);
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	case EDwTrioviz3DMode_SideBySide:
		if (pCurProfile->m_Global.m_InvertEyes == false)
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, (nDW_ScreenSizeX/2) - (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2)*2, nDW_ScreenSizeY, 2.0f);
			}else
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2), nDW_ScreenSizeY, 2.0f);					
			}
		}
		else
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, (nDW_ScreenSizeX/2) + (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2), nDW_ScreenSizeY, 2.0f);
			}else
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - (nDW_EyeOffset/2), 0, nDW_ScreenSizeX/2 - (nDW_EyeOffset/2)*2, nDW_ScreenSizeY, 2.0f);
			}
		}
		break;
	case EDwTrioviz3DMode_TopBottom:
		if (pCurProfile->m_Global.m_InvertEyes == false)
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, nDW_ScreenSizeY/2, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY/2, 0.5f);
			}else
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY/2, 0.5f);
			}
		}
		else
		{
			if(DwTriovizImpl_IsRenderingLeftEye())
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY/2, 0.5f);
			}else
			{
				DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, nDW_ScreenSizeY/2, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY/2, 0.5f);
			}
		}
		break;
	case EDwTrioviz3DMode_LeftEye:
		DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 - nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset*2, nDW_ScreenSizeY, 1.0f);
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	case EDwTrioviz3DMode_RightEye:
		DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0 + nDW_EyeOffset, 0, nDW_ScreenSizeX - nDW_EyeOffset, nDW_ScreenSizeY, 1.0f);
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	default:
		DwSetViewport(movie, nDW_ScreenSizeX, nDW_ScreenSizeY, 0, 0, nDW_ScreenSizeX, nDW_ScreenSizeY, 1.0f);
		if(!DwTriovizImpl_IsRenderingLeftEye())
		{
			bDW_RenderUI = false;
		}
		break;
	}

#endif
	return bDW_RenderUI;
}


void DwTriovizImpl_SetWhichEyeRendering(bool bLeftEye)
{
	bIsLeftEyeRendering = bLeftEye;
}

struct SDwUETriovizRenderTarget
{
	FSurfaceRHIRef const *	m_pRenderTarget;
	DWORD					m_UsageFlags;

	SDwUETriovizRenderTarget():
		m_pRenderTarget(NULL),
		m_UsageFlags(0)
	{}
};
SDwUETriovizRenderTarget GUETriovizRenderTarget;

SDwTriovizCreateDesc	GDwTriovizCreateDesc;
SDwTriovizRenderDesc	GDwTriovizRenderDesc;
UBOOL					g_DwUseHDRSurface = FALSE;
UBOOL					g_DwUseCinematicMode = FALSE;
FLOAT					g_DwFocalCinematicMode = 0.0f;
FVector4				g_DwInvDeviceZToWorldZTransform;

#if !FINAL_RELEASE
FCanvas* g_pDrawTextCanvas	= NULL;
FANSIToTCHAR_Convert		g_ToTCHAR;
#endif


#if PS3
CellGcmContextData * DwTriovizImpl_GetCurrentContext()
{
	return gCellGcmCurrentContext;
}
#endif

void DwTriovizImpl_ResetStates()
{
#ifdef PS3
	cellGcmSetAlphaFunc( DwTriovizImpl_GetCurrentContext(), CELL_GCM_ALWAYS, 0);
	cellGcmSetAlphaTestEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetBackStencilFunc(DwTriovizImpl_GetCurrentContext(), CELL_GCM_ALWAYS, 0, 0xff);
	cellGcmSetBackStencilMask(DwTriovizImpl_GetCurrentContext(), 0xff);
	cellGcmSetBackStencilOp(DwTriovizImpl_GetCurrentContext(), CELL_GCM_KEEP, CELL_GCM_KEEP, CELL_GCM_KEEP);
	cellGcmSetBlendColor(DwTriovizImpl_GetCurrentContext(), 0, 0);
	cellGcmSetBlendEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetBlendEnableMrt(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	cellGcmSetBlendEquation(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD);
	cellGcmSetBlendFunc(DwTriovizImpl_GetCurrentContext(), CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ONE, CELL_GCM_ZERO);
	cellGcmSetClearDepthStencil(DwTriovizImpl_GetCurrentContext(), 0xffffff00);
	cellGcmSetClearSurface(DwTriovizImpl_GetCurrentContext(), 0);
	cellGcmSetColorMask(DwTriovizImpl_GetCurrentContext(), CELL_GCM_COLOR_MASK_A|CELL_GCM_COLOR_MASK_R|CELL_GCM_COLOR_MASK_G|CELL_GCM_COLOR_MASK_B);
	cellGcmSetCullFaceEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetCullFace(DwTriovizImpl_GetCurrentContext(), CELL_GCM_BACK);
	cellGcmSetDepthBounds(DwTriovizImpl_GetCurrentContext(), 0.0f, 1.0f);
	cellGcmSetDepthBoundsTestEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetDepthFunc(DwTriovizImpl_GetCurrentContext(), CELL_GCM_LESS);
	cellGcmSetDepthMask(DwTriovizImpl_GetCurrentContext(), CELL_GCM_TRUE);
	cellGcmSetDepthTestEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetDitherEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_TRUE);
	cellGcmSetFragmentProgramGammaEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetFrequencyDividerOperation(DwTriovizImpl_GetCurrentContext(), 0);
	cellGcmSetFrontFace(DwTriovizImpl_GetCurrentContext(), CELL_GCM_CCW);
	cellGcmSetLineWidth(DwTriovizImpl_GetCurrentContext(), 8); // fixed point [0:6:3]
	cellGcmSetLogicOpEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetLogicOp(DwTriovizImpl_GetCurrentContext(), CELL_GCM_COPY);
	cellGcmSetNotifyIndex(DwTriovizImpl_GetCurrentContext(),0); // initial value is an invalid system reserved area
	cellGcmSetPointSize(DwTriovizImpl_GetCurrentContext(), 1.0f);
	cellGcmSetPolygonOffsetFillEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetPolygonOffset(DwTriovizImpl_GetCurrentContext(), 0.0f, 0.0f);
	cellGcmSetRestartIndexEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetRestartIndex(DwTriovizImpl_GetCurrentContext(), 0xffffffff);
	cellGcmSetScissor(DwTriovizImpl_GetCurrentContext(), 0,0,4096,4096);
	cellGcmSetShadeMode(DwTriovizImpl_GetCurrentContext(), CELL_GCM_SMOOTH);
	cellGcmSetStencilFunc(DwTriovizImpl_GetCurrentContext(), CELL_GCM_ALWAYS, 0, 0xff);
	cellGcmSetStencilMask(DwTriovizImpl_GetCurrentContext(), 0xff);
	cellGcmSetStencilOp(DwTriovizImpl_GetCurrentContext(), CELL_GCM_KEEP, CELL_GCM_KEEP, CELL_GCM_KEEP);
	cellGcmSetStencilTestEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);

	for ( unsigned long tid = 0; tid < 16; tid++ )
	{
		cellGcmSetTextureAddress(DwTriovizImpl_GetCurrentContext(), tid, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_NEVER, 0);
		cellGcmSetTextureBorderColor(DwTriovizImpl_GetCurrentContext(), tid, 0);
		cellGcmSetTextureControl(DwTriovizImpl_GetCurrentContext(), tid, CELL_GCM_FALSE, 0, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
		cellGcmSetTextureFilter(DwTriovizImpl_GetCurrentContext(), tid, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
		cellGcmSetTextureOptimization(DwTriovizImpl_GetCurrentContext(), tid, 8, CELL_GCM_TEXTURE_ISO_HIGH, CELL_GCM_TEXTURE_ANISO_HIGH );

		cellGcmSetVertexDataArray(DwTriovizImpl_GetCurrentContext(), tid, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	}

	cellGcmSetTwoSidedStencilTestEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);

	float scale[4] = {2048.0f, 2048.0f, 0.5f, 0.0f};
	float offset[4] = {2048.0f, 2048.0f, 0.5f, 0.0f};

	cellGcmSetViewport(DwTriovizImpl_GetCurrentContext(), 0, 0, 4096, 4096, 0.0f, 1.0f, scale, offset);
	cellGcmSetZcullStatsEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetAntiAliasingControl(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE, 0xffff);
	cellGcmSetBackPolygonMode(DwTriovizImpl_GetCurrentContext(), CELL_GCM_POLYGON_MODE_FILL);
	cellGcmSetClearColor(DwTriovizImpl_GetCurrentContext(), 0);
	cellGcmSetColorMaskMrt(DwTriovizImpl_GetCurrentContext(), 0);
	cellGcmSetFrontPolygonMode(DwTriovizImpl_GetCurrentContext(), CELL_GCM_POLYGON_MODE_FILL);
	cellGcmSetLineSmoothEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetLineStippleEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetPointSpriteControl(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE, 0, 0);
	cellGcmSetPolySmoothEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetPolygonStippleEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetRenderEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_TRUE, 0);
	cellGcmSetUserClipPlaneControl(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE);
	cellGcmSetVertexAttribInputMask(DwTriovizImpl_GetCurrentContext(), 0xffff);
	cellGcmSetZpassPixelCountEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);

	for ( unsigned long tid = 0; tid < 4; tid++ )
	{
		cellGcmSetVertexTextureAddress(DwTriovizImpl_GetCurrentContext(), tid, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP);
		cellGcmSetVertexTextureBorderColor(DwTriovizImpl_GetCurrentContext(), tid, 0);
		cellGcmSetVertexTextureControl(DwTriovizImpl_GetCurrentContext(), tid, CELL_GCM_FALSE, 0, 12<<8);
		cellGcmSetVertexTextureFilter(DwTriovizImpl_GetCurrentContext(), tid, 0);
	}


	cellGcmSetTransformBranchBits(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetTwoSideLightEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetZMinMaxControl(DwTriovizImpl_GetCurrentContext(), CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	cellGcmSetCylindricalWrap(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE, 0);
	cellGcmSetTwoSideLightEnable(DwTriovizImpl_GetCurrentContext(), CELL_GCM_FALSE);
	cellGcmSetTransformBranchBits(DwTriovizImpl_GetCurrentContext(), 0);
	cellGcmSetVertexDataBase(DwTriovizImpl_GetCurrentContext(), 0,0);
	cellGcmSetZcullEnable( DwTriovizImpl_GetCurrentContext(), CELL_GCM_TRUE, CELL_GCM_FALSE );
	// Force the texture cache to flush.
	// This is very important if we've just rendered TO a texture and then expect to render FROM it immediately after,
	// such as during bloom/filtering passes, especially as we get to smaller render targets.
	// This flush ensures the rendering has been completed.
	cellGcmSetInvalidateTextureCache( DwTriovizImpl_GetCurrentContext(), CELL_GCM_INVALIDATE_TEXTURE );
#endif
}

void DwTriovizImpl_DrawText(DwInt X, DwInt Y, DwChar const * const pText, SDwLinearColor const &Color)
{
#if !FINAL_RELEASE
	if (NULL != g_pDrawTextCanvas)
	{
		TCHAR* pTCharText = g_ToTCHAR.Convert( pText, 0, 0 );
		UFont* pFont = GEngine->GetTinyFont();
		if (pFont == NULL)
		{
			pFont = GEngine->GetSmallFont();
		}
		if (pFont == NULL)
		{
			pFont = GEngine->GetMediumFont();
		}
		if (pFont == NULL)
		{
			pFont = GEngine->GetLargeFont();
		}
		if (pFont == NULL)
		{
			pFont = GEngine->GetSubtitleFont();
		}
		DrawShadowedString(g_pDrawTextCanvas, X, Y, pTCharText, pFont, FLinearColor(Color.m_R,Color.m_G,Color.m_B,Color.m_A));
		g_pDrawTextCanvas->Flush();
	}
#endif
}

void DwTriovizImpl_DrawLine2D(SDwVector2 const &DwSrc, SDwVector2 const &DwDst, SDwLinearColor const &DwCol)
{
#if !FINAL_RELEASE
 	if (NULL != g_pDrawTextCanvas)
	{
		FIntPoint		Src = FVector2D(DwSrc.m_X, DwSrc.m_Y).IntPoint();
		FIntPoint		Dst = FVector2D(DwDst.m_X, DwDst.m_Y).IntPoint();
		FLinearColor	Col = FLinearColor(DwCol.m_R,DwCol.m_G,DwCol.m_B,DwCol.m_A);
		DrawLine2D(g_pDrawTextCanvas, Src, Dst, Col);
		g_pDrawTextCanvas->Flush();
	}
#endif
}

void DwTriovizImpl_RestoreEngineRenderStates()
{
	DwTriovizImpl_ResetStates();
}

#if WIN32


#include "../../D3D9Drv/Inc/D3D9Drv.h"

extern IDirect3DDevice9* GLegacyDirect3DDevice9;

/// Setup per-platform functions so following DwTriovizImpl_ functions would be X-Platform
void DwTriovizImpl_SetDevice()
{
	GDwTriovizCreateDesc.m_pD3d9Device = GLegacyDirect3DDevice9;
	GDwTriovizCreateDesc.m_DataPath = SDwString("..\\..\\Engine\\Content\\DwTrioviz\\");
}

template<typename TRsrc, typename TPlatformRef, typename TDynRHIRef>
TRsrc GetPlatformResource(TDynRHIRef DynRHIRef)
{
	return (*((TPlatformRef*)&(*DynRHIRef)));	
}

IDirect3DSurface9* GetPlatformSurface(const FSurfaceRHIRef& RHIRef)
{
	return (GetPlatformResource<IDirect3DSurface9*, FD3D9Surface>(RHIRef));	
}

IDirect3DTexture9* GetPlatformTexture(const FTexture2DRHIRef& RHIRef)
{
	return (GetPlatformResource<IDirect3DTexture9*, FD3D9Texture2D>(RHIRef));
}

void DwTriovizImpl_SetSceneColorSurface(const FSurfaceRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurfaceD3D9 = GetPlatformSurface(RHIRef);
}

void DwTriovizImpl_SetSceneColorTexture(const FTexture2DRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorTexture.m_pTextureD3D9 = GetPlatformTexture(RHIRef);
}

void DwTriovizImpl_SetExtraDepthTexture(const FTexture2DRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_DepthControl.m_ExtraDepthTexture.m_pTextureD3D9 = GetPlatformTexture(RHIRef);
}

/// Overwrite functions contained in overwritten defines
#if __OVERWRITE____D3D9_DwTrioviz_DebugDrawText
	DwInt __D3D9_DwTrioviz_GetTextFontHeight()
	{
		return (16);
	}

	void __D3D9_DwTrioviz_DebugDrawText(DwInt X, DwInt Y, DwChar const * const pText, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawText(X, Y, pText, Color);
	}
#endif // __OVERWRITE____D3D9_DwTrioviz_DebugDrawText

#if __OVERWRITE____D3D9_DwTrioviz_DebugDrawLine
	void __D3D9_DwTrioviz_DebugDrawLine(SDwVector2 const &Source, SDwVector2 const &Dest, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawLine2D(Source, Dest, Color);
	}
#endif // __OVERWRITE____D3D9_DwTrioviz_DebugDrawLine

#if __OVERWRITE____D3D9_DwTrioviz_RestoreEngineRenderStates
	void __D3D9_DwTrioviz_RestoreEngineRenderStates()
	{
		DwTriovizImpl_RestoreEngineRenderStates();
	}
#endif // __OVERWRITE____D3D9_DwTrioviz_RestoreEngineRenderStates

#if __OVERWRITE____D3D9_DwTrioviz_SetRenderTarget
	void __D3D9_DwTrioviz_SetRenderTarget(SDwSceneRenderTarget& SceneColorRT)
	{
		DwTrioviz_SetRenderTargetSurface(SceneColorRT.m_SceneColorSurface);
	}

	void __D3D9_DwTrioviz_SetRenderTarget(SDwTriovizRenderTarget& RT)
	{
		DwTrioviz_SetRenderTargetSurface(RT.m_Surface);
	}
#endif // __OVERWRITE____D3D9_DwTrioviz_SetRenderTarget


#elif XBOX


#include "../../XeD3DDrv/Inc/XeD3DDrv.h"

/// Setup per-platform functions so following DwTriovizImpl_ functions would be X-Platform
void DwTriovizImpl_SetDevice()
{
	extern IDirect3DDevice9* GDirect3DDevice;
	GDwTriovizCreateDesc.m_pXBoxDevice	= GDirect3DDevice;
#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	GDwTriovizCreateDesc.m_StereoParams = g_DwStereoParams;
#endif
}

IDirect3DSurface9* GetPlatformSurface(const FSurfaceRHIRef& RHIRef)
{
	return (RHIRef.GetReference());	
}

IDirect3DTexture9* GetPlatformTexture(const FTexture2DRHIRef& RHIRef)
{
	return ((IDirect3DTexture9*)RHIRef);	
}

void DwTriovizImpl_SetSceneColorSurface(const FSurfaceRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface = GetPlatformSurface(RHIRef);
}

void DwTriovizImpl_SetSceneColorTexture(const FTexture2DRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorTexture.m_pTexture = GetPlatformTexture(RHIRef);
}

void DwTriovizImpl_SetExtraDepthTexture(const FTexture2DRHIRef& RHIRef)
{
	GDwTriovizRenderDesc.m_DepthControl.m_ExtraDepthTexture.m_pTexture = GetPlatformTexture(RHIRef);
}

/// Overwrite functions contained in overwritten defines
#if __OVERWRITE____XBOX_DwTrioviz_DebugDrawText
	DwInt __XBOX_DwTrioviz_GetTextFontHeight()
	{
		return (16);
	}

	void __XBOX_DwTrioviz_DebugDrawText(DwInt X, DwInt Y, DwChar const * const pText, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawText(X, Y, pText, Color);
	}
#endif // __OVERWRITE____XBOX_DwTrioviz_DebugDrawText

#if __OVERWRITE____XBOX_DwTrioviz_DebugDrawLine
	void __XBOX_DwTrioviz_DebugDrawLine(SDwVector2 const &Source, SDwVector2 const &Dest, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawLine2D(Source, Dest, Color);
	}
#endif // __OVERWRITE____XBOX_DwTrioviz_DebugDrawLine

#if __OVERWRITE____XBOX_DwTrioviz_RestoreEngineRenderStates
	void __XBOX_DwTrioviz_RestoreEngineRenderStates()
	{
		DwTriovizImpl_RestoreEngineRenderStates();
	}
#endif // __OVERWRITE____XBOX_DwTrioviz_RestoreEngineRenderStates

#if __OVERWRITE____XBOX_DwTrioviz_SetRenderTarget
	void __XBOX_DwTrioviz_SetRenderTarget(SDwSceneRenderTarget& SceneColorRT)
	{
		DwTrioviz_SetRenderTargetSurface(SceneColorRT.m_SceneColorSurface);
	}

	void __XBOX_DwTrioviz_SetRenderTarget(SDwTriovizRenderTarget& RT)
	{
		DwTrioviz_SetRenderTargetSurface(RT.m_Surface);
	}
#endif // __OVERWRITE____XBOX_DwTrioviz_SetRenderTarget


#elif PS3


#include "../../RHI/Inc/PS3RHI.h"

/// Setup per-platform functions so following DwTriovizImpl_ functions would be X-Platform
void DwTriovizImpl_SetDevice()
{
}


void GetPlatformSurface(const FSurfaceRHIRef& RHIRef, CellGcmSurface &Surface)
{
	FPS3RHISurface const * const pRHISurface = (FPS3RHISurface const * const)&(*RHIRef);
	Surface.colorFormat			= pRHISurface->Format;
	Surface.colorOffset[0]		= pRHISurface->MemoryOffset;
	Surface.colorPitch[0]		= pRHISurface->Pitch;
	Surface.width				= pRHISurface->SizeX;
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
	if (GDwScreenHeight3D == pRHISurface->SizeY)
	{
		Surface.height				= GScreenHeight;
	}
	else
#endif
	{
		Surface.height				= pRHISurface->SizeY;
	}
}

void GetPlatformTexture(const FTexture2DRHIRef& RHIRef, CellGcmTexture &Texture)
{
	Texture = RHIRef.GetReferenced()->GetTexture();
}

void DwTriovizImpl_SetSceneColorSurface(const FSurfaceRHIRef& RHIRef)
{
	GetPlatformSurface(RHIRef, GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface);
}

void DwTriovizImpl_SetSceneColorTexture(const FTexture2DRHIRef& RHIRef)
{
	GetPlatformTexture(RHIRef, GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorTexture.m_pTexture);
}

void DwTriovizImpl_SetExtraDepthTexture(const FTexture2DRHIRef& RHIRef)
{
	GetPlatformTexture(RHIRef, GDwTriovizRenderDesc.m_DepthControl.m_ExtraDepthTexture.m_pTexture);
}

/// Overwrite functions contained in overwritten defines
#if __OVERWRITE____PS3_DwTrioviz_DebugDrawText
	DwInt __PS3_DwTrioviz_GetTextFontHeight()
	{
		return (16);
	}

	void __PS3_DwTrioviz_DebugDrawText(DwInt X, DwInt Y, DwChar const * const pText, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawText(X, Y, pText, Color);
	}
#endif // __OVERWRITE____PS3_DwTrioviz_DebugDrawText

#if __OVERWRITE____PS3_DwTrioviz_DebugDrawLine
	void __PS3_DwTrioviz_DebugDrawLine(SDwVector2 const &Source, SDwVector2 const &Dest, SDwLinearColor const &Color)
	{
		DwTriovizImpl_DrawLine2D(Source, Dest, Color);
	}
#endif // __OVERWRITE____PS3_DwTrioviz_DebugDrawLine

#if __OVERWRITE____PS3_DwTrioviz_RestoreEngineRenderStates
	void __PS3_DwTrioviz_RestoreEngineRenderStates()
	{
		DwTriovizImpl_RestoreEngineRenderStates();
	}
#endif // __OVERWRITE____PS3_DwTrioviz_RestoreEngineRenderStates

#if __OVERWRITE____PS3_DwTrioviz_Memory
	DwBool __PS3_DwTrioviz_AllocateMemory(SDwMemory& Memory, DwByte Location, DwUint TotalSize, DwUint Alignment)
	{
		Memory.m_Location	= Location;
		Memory.m_Size		= TotalSize;
		if (TotalSize != 0)
		{
			Memory.m_pAddress = (Location == CELL_GCM_LOCATION_LOCAL ? GPS3Gcm->GetLocalAllocator() : GPS3Gcm->GetHostAllocator())->Malloc(TotalSize, AT_ResourceArray, Alignment);
			if (Memory.m_pAddress == NULL)
			{
				GPS3Gcm->GetHostAllocator()->Dump(TEXT("Host"), FALSE);
				GPS3Gcm->GetLocalAllocator()->Dump(TEXT("Local"), FALSE);
				checkf(Memory.m_pAddress, TEXT("\nPS3RHIGPUResource failed to allocate %d bytes of GPU data (type = %d"), TotalSize, AT_ResourceArray);
			}

			// calculate offset
			cellGcmAddressToOffset(Memory.m_pAddress, &Memory.m_Offset);
		}

		return ((TotalSize == 0) || (Memory.m_pAddress != NULL));
	}

	DwBool __PS3_DwTrioviz_AllocateTextureMemory(SDwTexture& DwTexture, DwByte Location, DwUint SizeX, DwUint SizeY, DwUint Format, DwUint Alignment)
	{
		DW_ERROR(SizeX && SizeY);
		DwUint BlockBytes	= (Format == CELL_GCM_TEXTURE_A8R8G8B8) ? 4 : 8;
		DwUint Pitch		= SizeX * BlockBytes;
		Pitch				= cellGcmGetTiledPitchSize( Pitch );
		DwUint TotalSize	= (Pitch * SizeY);

		DwTexture.m_Memory.m_Location	= Location;
		DwTexture.m_Memory.m_Size		= TotalSize;

		bool bResult = __PS3_DwTrioviz_AllocateMemory(DwTexture.m_Memory, Location, TotalSize, Alignment);
		if (bResult)
		{
			DwTexture.m_pTexture.offset	= DwTexture.m_Memory.m_Offset;
		}

		return (bResult);
	}

	DwBool __PS3_DwTrioviz_FreeMemory(SDwMemory& Memory)
	{
		if (Memory.m_Location == CELL_GCM_LOCATION_LOCAL)
		{
			GPS3Gcm->GetLocalAllocator()->Free(Memory.m_pAddress);
		}
		else
		{
			GPS3Gcm->GetHostAllocator()->Free(Memory.m_pAddress);
		}

		Memory.m_pAddress = NULL;
		Memory.m_Size = 0;
		Memory.m_Offset = 0;

		return (true);	
	}
#endif // __OVERWRITE____PS3_DwTrioviz_Memory

#if __OVERWRITE____PS3_DwTrioviz_SetRenderTarget
	void __PS3_DwTrioviz_SetRenderTarget(SDwSceneRenderTarget& SceneColorRT)
	{
		DwTrioviz_SetRenderTargetSurface(SceneColorRT.m_SceneColorSurface);
	}

	void __PS3_DwTrioviz_SetRenderTarget(SDwTriovizRenderTarget& RT)
	{
		DwTrioviz_SetRenderTargetSurface(RT.m_Surface);
	}
#endif // __OVERWRITE____XBOX_DwTrioviz_SetRenderTarget


#else

#error

#endif


UBOOL DwTriovizImpl_IsTriovizAllowed() 
{
	return (!GIsEditor && !GIsPlayInEditorWorld);
}

UBOOL DwTriovizImpl_IsTriovizActive() 
{
	return (DwTriovizImpl_IsTriovizAllowed() && DwTrioviz_GetFlag(EDwTriovizFlag_InGame));
}

void DwTriovizImpl_ToggleCinematicMode(UBOOL Cinematic)
{
	if( IsInRenderingThread() )
	{
		g_DwUseCinematicMode = Cinematic;
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DwTriovizImpl_ToggleCinematicMode_Command,
			UBOOL,bUseCinematicMode,Cinematic,
		{
			g_DwUseCinematicMode = bUseCinematicMode;
		});	
	}
}

void DwTriovizImpl_SetFocalDistanceCinematicMode(FLOAT Distance)
{
	if( IsInRenderingThread() )
	{
		g_DwFocalCinematicMode = Distance;
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DwTriovizImpl_SetFocalDistanceCinematicMode_Command,
			FLOAT,FocalDistance,Distance,
		{
			g_DwFocalCinematicMode = FocalDistance;
		});	
	}
}

void DwTriovizImpl_SetInvDeviceZToWorldZTransform(const FVector4& InvDeviceZToWorldZTransform)
{
	if( IsInRenderingThread() )
	{
		g_DwInvDeviceZToWorldZTransform = InvDeviceZToWorldZTransform;
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DwTriovizImpl_SetInvDeviceZToWorldZTransform_Command,
			FVector4,Inv,InvDeviceZToWorldZTransform,
		{
			g_DwInvDeviceZToWorldZTransform = Inv;
		});	
	}
}

UBOOL DwTriovizImpl_IsCinematicMode(void)
{
	return (g_DwUseCinematicMode);
}

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280

void DwTriovizImpl_Strong3D(void)
{
	//Positive + negative parallax, stronger effect with things popping but a little less comfortable in the long run
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.555000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.470000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 0.110000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_W = 0.120000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.545000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.495000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 0.480000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 0.300000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_Global.m_Multiplier = 2.5f;
}

void DwTriovizImpl_Subtle3D(void)
{
	//Almost no negative parallax: use this for a more subtle and comfortable effect
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_W = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_Global.m_Multiplier = 2.5f;
}

#endif

void DwTriovizImpl_InitializeProfiles()
{
#if !FINAL_RELEASE
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning] = SDwTriovizParms();
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_ColorCorrection.m_ColorCorrectionMapId = 0;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_ColorCorrection.m_UseTriovizColorCorrection = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_3DMode.m_3DMode = EDwTrioviz3DMode_SideBySide;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_Global.m_Multiplier = 2.5f;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_ZLimit = 2000.0f;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.340000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.470000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 0.215000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetShapeCenterAndScale.m_W = 0.120000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.390000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.380000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 0.420000;
	g_pDwTriovizProfiles[EDwTriovizProfile_Tuning].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 0.300000;
#endif

	//Use for videos
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu] = SDwTriovizParms();
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_3DMode.m_3DMode = EDwTrioviz3DMode_ColorFilter;
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_ColorCorrection.m_ColorCorrectionMapId = 0;
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_ColorCorrection.m_UseTriovizColorCorrection = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_DOF.m_UseTriovizDof = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_Global.m_Multiplier = 3.0f;
	g_pDwTriovizProfiles[EDwTriovizProfile_Menu].m_MinMax.m_ZLimit = 2000.0f;

	//Use for anaglyph
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter] = SDwTriovizParms();
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_3DMode.m_3DMode = EDwTrioviz3DMode_ColorFilter;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_ColorCorrection.m_ColorCorrectionMapId = 0;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_ColorCorrection.m_UseTriovizColorCorrection = true;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_Global.m_Multiplier = 1.1f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_ZLimit = 2000.0f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.575000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.450000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 0.150000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetShapeCenterAndScale.m_W = 0.200000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.635000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.400000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 0.420000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameColorFilter].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 0.300000;

	//Use for old 3DTV (obsolete)
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV] = SDwTriovizParms();
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_3DMode.m_3DMode = EDwTrioviz3DMode_SideBySide;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_ColorCorrection.m_ColorCorrectionMapId = 0;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_ColorCorrection.m_UseTriovizColorCorrection = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_Global.m_Multiplier = 2.5;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_DMap.m_DMapCurveY.m_X = 0.70f;//0.50f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_DMap.m_DMapCurveY.m_Y = 0.00f;//0.50f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_DMap.m_DMapCurveY.m_Z = 1.00f;//0.45f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_DMap.m_DMapCurveY.m_W = 0.00f;//0.65f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_ZLimit = 2000.0f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetShapeCenterAndScale.m_W = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGame3DTV].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 1.000000;

	//Use for modern 3DTV
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking] = SDwTriovizParms();
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_3DMode.m_3DMode = EDwTrioviz3DMode_LeftEye;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_ColorCorrection.m_ColorCorrectionMapId = 0;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_ColorCorrection.m_UseTriovizColorCorrection = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_DOF.m_UseTriovizDof = false;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_Global.m_Multiplier = 2.5;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_DMap.m_DMapCurveY.m_X = 0.70f;//0.50f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_DMap.m_DMapCurveY.m_Y = 0.00f;//0.50f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_DMap.m_DMapCurveY.m_Z = 1.00f;//0.45f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_DMap.m_DMapCurveY.m_W = 0.00f;//0.65f;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_ZLimit = 2000.0f;

	//Almost no negative parallax: use this for a more subtle and comfortable effect
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetShapeCenterAndScale.m_W = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_X = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Y = 0.500000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_Z = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_MinMax.m_TargetMaxZShapeCenterAndScale.m_W = 1.000000;
	g_pDwTriovizProfiles[EDwTriovizProfile_InGameFramePacking].m_Global.m_Multiplier = 2.5f;
}

void DwTriovizImpl_Initialize()
{	
	if (DwTriovizImpl_IsTriovizAllowed() )
	{
		DwTriovizImpl_SetDevice();

		GDwTriovizCreateDesc.m_ScreenWidth			= GSceneRenderTargets.GetBufferSizeX();
		GDwTriovizCreateDesc.m_ScreenHeight			= GSceneRenderTargets.GetBufferSizeY();
		GDwTriovizCreateDesc.m_DisparityMapFactor	= 2;
		GDwTriovizCreateDesc.m_FilterMapFactor		= GSceneRenderTargets.GetFilterDownsampleFactor();

		//DwTrioviz_SetFlag(EDwTriovizFlag_InGame);
#if !FINAL_RELEASE
//		DwTrioviz_SetFlag(EDwTriovizFlag_Menu);
#endif

		//Share memory with existing Unreal render targets
#if XBOX
		if ( GSystemSettings.RenderThreadSettings.bAllowMotionBlur )
		{
			g_pDwTriovizRenderTarget[EDwTriovizRenderTargetId_DisparityMap].m_IsExternallyAllocated = true;
			g_pDwTriovizRenderTarget[EDwTriovizRenderTargetId_DisparityMap].m_Surface.m_pSurface = GetPlatformSurface(GSceneRenderTargets.GetVelocitySurface());
			g_pDwTriovizRenderTarget[EDwTriovizRenderTargetId_DisparityMap].m_Texture.m_pTexture = GetPlatformTexture(GSceneRenderTargets.GetVelocityTexture());
		}
#endif

		DwTrioviz_Create(GDwTriovizCreateDesc);

		DwTrioviz_CaptureInvertAlpha(!GUsesInvertedZ);

		DwTriovizImpl_InitializeProfiles();

		#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
		if(g_DwUseFramePacking)
		{
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGameFramePacking);
		}
		else
		{
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGame3DTV);
		}
		#endif

		g_DwInvDeviceZToWorldZTransform = FVector4(10.0f, 0.001f, 0.1f, 0.0001f);

// EPICSTART
		// allow commandline to set depth dumping, without needing the console command (TriovizSwitchCaptureDepth)
		if (ParseParam(appCmdLine(), TEXT("DumpTrioviz")))
		{
			g_DwCaptureDepth = TRUE;
		}
// EPICEND
	}
}

void DwTriovizImpl_Release()
{
	if (DwTriovizImpl_IsTriovizAllowed() )
	{
		DwTrioviz_Destroy();
	}
}

void DwTriovizImpl_Render(FDwSceneViewCache * View, UBOOL FinalEffectInGroup, DWORD UsageFlagsLDR, UBOOL RenderRightOnly)
{	
	/// Common
	GDwTriovizRenderDesc.m_DeltaWorldTime			= View->DeltaWorldTime;
	GDwTriovizRenderDesc.m_UseSmoothTransition		= (View->NumView == 1 && View->bUseSmoothTransition);//smooth transition only for singleplayer

#if !FINAL_RELEASE	/// Handling Screenhots & Movie capture
	static UBOOL gs_bWasCapturing = FALSE;

	UBOOL bIsCapturing = DwTrioviz_IsCaptureInProgress();		

	if (bIsCapturing)
	{	/// Capture is in progress, force use fixed delata time
 		GUseFixedTimeStep = TRUE;
	}
	else
	{
		static UBOOL gs_OldUseFixedTimeStep = GUseFixedTimeStep;

		if (gs_bWasCapturing)
		{	/// No capture at the moment, but was capturing at previous frame, so restore the flag
			GUseFixedTimeStep = gs_OldUseFixedTimeStep;
		}
		else
		{	/// No capture at the moment, and was not capturing at previous frame, so backup the flag
			gs_OldUseFixedTimeStep = GUseFixedTimeStep;			
		}
	}

	gs_bWasCapturing = bIsCapturing;

	if (bIsCapturing)
	{
		GDwTriovizRenderDesc.m_DeltaWorldTime		= GFixedDeltaTime;
	}
	else
#endif	// !FINAL_RELEASE
	if (!g_DwUseHDRSurface)
	{
		extern void DwTriovizImpl_HDR_To_LDR(FDwSceneViewCache * pView);
		DwTriovizImpl_HDR_To_LDR(View);		
	}

	GDwTriovizRenderDesc.m_ViewInfo.m_ViewIndex		= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetX	= View->RenderTargetX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetY	= View->RenderTargetY;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeX		= View->RenderTargetSizeX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeY		= View->RenderTargetSizeY;

	/// Depth management

	GDwTriovizRenderDesc.m_DepthControl.m_InvDeviceZToWorldZTransform = SDwVector4(View->InvDeviceZToWorldZTransform.X, View->InvDeviceZToWorldZTransform.Y, View->InvDeviceZToWorldZTransform.Z, View->InvDeviceZToWorldZTransform.W);
	if (GEngineVersion >= 7977)
	{
		//With Dx11 changes, the depth access has changed for other platforms.
		//Our depth access is based on the old method, so compute we recompute InvDeviceZToWorldZTransform as before (with Z inverted)
		FVector4 InvDeviceZToWorldZTransform = CreateInvDeviceZToWorldZTransform(View->ProjectionMatrix, TRUE);
		GDwTriovizRenderDesc.m_DepthControl.m_InvDeviceZToWorldZTransform = SDwVector4(InvDeviceZToWorldZTransform.X, InvDeviceZToWorldZTransform.Y, InvDeviceZToWorldZTransform.Z, InvDeviceZToWorldZTransform.W);
	}


	GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureFactor = 1;
	GDwTriovizRenderDesc.m_DepthControl.m_DepthEncodeMode = (View->bUseNormalDevSpace == TRUE) ? EDwTriovizDepthEncodeMode_StandardEncoding_NormalDevSpace : EDwTriovizDepthEncodeMode_SpecificEncoding_InvertDevSpace;

	UBOOL bDrawToHDR = FALSE;

	if (View->bUseZFromAlpha)
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_SceneColorTextureAlpha;
	}
	else if (GSupportsDepthTextures && IsValidRef(GSceneRenderTargets.GetSceneDepthTexture()))
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_ExtraDepthTextureRed;
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneDepthTexture());
	}
	else if (View->bUseLDRSceneColor)
	{
		/// Depth is stored in SceneColorTexture (not the LDR one)
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_ExtraDepthTextureAlpha;
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorTexture());	
	}
	else
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_SceneColorTextureAlpha;
	}

	if (View->bUseMoviePackingWithDisparity)
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_MoviePackingWithDisparity;
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorLDRTexture());
	}
	
	/// RenderTarget Surface
	if(RenderRightOnly || !View->bResolveScene)
	{
		if (View->bUseLDRSceneColor)
		{
			GSceneRenderTargets.BeginRenderingSceneColorLDR(RTUsage_FullOverwrite);
			DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorLDRSurface());
		}
		else
		{
			// Using 64-bit (HDR) surface
			GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_FullOverwrite);
			DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorSurface());	
			bDrawToHDR = TRUE;
		}
	}
	else
	{
		// If this is the final effect in chain, render to the view's output render target
		// unless an upscale is needed, in which case render to LDR scene color.
		if (FinalEffectInGroup && !GSystemSettings.NeedsUpscale())
		{
			if (!View->bUseLDRSceneColor)
			{
				GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_FullOverwrite);
				// Using 64-bit (HDR) surface
				DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorSurface());	
				bDrawToHDR = TRUE;
			}
			else
			{
				// Render to the final render target				
				GSceneRenderTargets.BeginRenderingBackBuffer(RTUsage_FullOverwrite);	
				//DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetBackBuffer());				
				DwTriovizImpl_SetSceneColorSurface(View->RenderTargetSurface);//, UsageFlagsLDR);
			}
		}
		else
		{
			GSceneRenderTargets.BeginRenderingSceneColorLDR(RTUsage_FullOverwrite);
			DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorLDRSurface());
		}
	}

	/// RenderTarget Texture
	DwTriovizImpl_SetSceneColorTexture(View->bUseLDRSceneColor ? GSceneRenderTargets.GetSceneColorLDRTexture() : GSceneRenderTargets.GetSceneColorTexture());	

	/// EndProcess : Resolve needed ?
	GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve			= false;
	GDwTriovizRenderDesc.m_RenderTarget.m_IsGammaCorrected	= false;
	/// Do the stuff !

#if DW_XBOX
	//Set the proper color exp bias for XBox
	if (!View->bUseLDRSceneColor && bDrawToHDR)
	{
		extern INT XeGetRenderTargetColorExpBias(D3DFORMAT SurfaceD3DFormat, D3DFORMAT TextureD3DFormat);		

		D3DFORMAT SurfaceD3DFormat = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface->Format;
		D3DFORMAT TextureD3DFormat = (D3DFORMAT)GPixelFormats[PF_FloatRGB].PlatformFormat;

		GDwTriovizRenderDesc.m_SceneColorBiasFactor = appPow(2.0f, XeGetRenderTargetColorExpBias(SurfaceD3DFormat, TextureD3DFormat));
	}
	else
	{
		GDwTriovizRenderDesc.m_SceneColorBiasFactor = 1.0f;
	}
#endif

	GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Full;

	#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 
	if(g_DwUseFramePacking)
	{
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.type = CELL_GCM_SURFACE_PITCH;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[0]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[1]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[2]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[3]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[1]  = 64;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[2]  = 64;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[3]  = 64;

		GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Prepare;
	}
	#endif
	#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280 
	if(g_DwUseFramePacking)
	{
		SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
		if (!RenderRightOnly)
		{
			GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Full;
			SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_LeftEye : EDwTrioviz3DMode_RightEye;
		}
		else
		{
			GDwTriovizRenderDesc.m_Pass = (g_DwSplitPlayerCount > 0) ? EDwTriovizProcessPass_Full : EDwTriovizProcessPass_Uber;
			SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_RightEye : EDwTrioviz3DMode_LeftEye;
		}
		
	}
	#endif

	//If cinematic mode, map the disparity level to the DOF plane
	UBOOL UseAutomaticDisparityLevel = TRUE;
	if (g_DwUseCinematicMode)
	{
		SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
		UseAutomaticDisparityLevel = pCurProfile->m_ZMap.m_UseAutomaticDisparityLevel;
		pCurProfile->m_ZMap.m_UseAutomaticDisparityLevel = FALSE;
		pCurProfile->m_ZMap.m_DisparityLevel = View->DOF_FocusDistance;
	}

	DwTriovizImpl_ResetRenderingStates();

	DwTrioviz_Render(GDwTriovizRenderDesc); 	


	#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
	if(g_DwUseFramePacking)
	{
		SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
		pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_LeftEye : EDwTrioviz3DMode_RightEye;
		GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Uber;
		GDwLeftEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
		DwTrioviz_Render(GDwTriovizRenderDesc); 	

		pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_RightEye : EDwTrioviz3DMode_LeftEye;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorOffset[0] += GDwRightOffsetStereo;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 6;
		GDwRightEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
		DwTrioviz_Render(GDwTriovizRenderDesc); 		

		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 0;
	}	
	#endif

	if (!(View->bUseZFromAlpha && View->bUseLDRSceneColor)) //Special case when there is no need to resolve
	{
		if(RenderRightOnly || !View->bResolveScene )
		{
			if (View->bUseLDRSceneColor)
			{
				GSceneRenderTargets.FinishRenderingSceneColorLDR(!RenderRightOnly);
			}
			else
			{
				// Using 64-bit (HDR) surface
				GSceneRenderTargets.FinishRenderingSceneColor();
			}
		}
		else
		{
			// If this is the final effect in chain, render to the view's output render target
			// unless an upscale is needed, in which case render to LDR scene color.
			if (FinalEffectInGroup && !GSystemSettings.NeedsUpscale())
			{
				if (!View->bUseLDRSceneColor)
				{
					GSceneRenderTargets.FinishRenderingSceneColor();
				}
				else
				{
					// Render to the final render target				
					//GSceneRenderTargets.FinishRenderingBackBuffer();	
				}
			}
			else
			{
				GSceneRenderTargets.FinishRenderingSceneColorLDR();
			}
		}
	}

#if PS3
	if (!RenderRightOnly && View->NumView > 1 && !View->bIsLastView)
	{
		GSceneRenderTargets.BeginRenderingBackBuffer(RTUsage_FullOverwrite);
	}
#endif

	g_DwUseHDRSurface = FALSE;

	#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
	if (!g_DwPendingSwitch3DModeFirst)
	{		
		g_DwPendingSwitch3DMode = FALSE;
	}	
	g_DwPendingSwitch3DModeFirst = FALSE;
	#endif

#if XBOX
	//Reset texture states
	for(DwInt Index = 0;Index < 16;Index++)
	{
		g_pDwTriovizXBoxDevice->SetTexture(Index,NULL);
	}
#endif

	//If cinematic mode, restore automatic disparity
	if (g_DwUseCinematicMode)
	{
		SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
		pCurProfile->m_ZMap.m_UseAutomaticDisparityLevel = UseAutomaticDisparityLevel ? true : false;
	}
}

void DwTriovizImpl_RenderTriovizSplitscreen(void)
{
	DW_DRAW_EVENT(DwTriovizImpl_RenderTriovizAllSplitscreen);

#if XBOX
	GSceneRenderTargets.FinishRenderingSceneColorLDR();//Resolve on Xbox
#endif

	for (INT Index = 0; Index < g_DwSplitPlayerCount; ++Index)
	{
		DW_DRAW_EVENT(DwTriovizImpl_RenderTriovizSplitscreen);
		DwTriovizImpl_Render(&g_DwSplitScreeenViewInfo[Index], TRUE, RTUsage_FullOverwrite) ;
	}

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	if (!g_DwUseFramePacking)
#endif
	g_DwSplitPlayerCount = 0;
}


void DwTriovizImpl_ResetRenderingStates()
{
#if PS3 //Fixes dirty RSX States
	for ( int tid = 0; tid < 16; tid++ )
	{
		cellGcmSetTextureAddress( tid, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_NEVER, 0);
		cellGcmSetTextureBorderColor( tid, 0);
		cellGcmSetTextureControl( tid, CELL_GCM_FALSE, 0, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
		cellGcmSetTextureFilter( tid, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
		cellGcmSetTextureOptimization( tid, 8, CELL_GCM_TEXTURE_ISO_HIGH, CELL_GCM_TEXTURE_ANISO_HIGH );

		cellGcmSetVertexDataArray( tid, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	}

	cellGcmSetVertexAttribInputMask( 0xffff);

	for ( int tid = 0; tid < 4; tid++ )
	{
		cellGcmSetVertexTextureAddress( tid, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP);
		cellGcmSetVertexTextureBorderColor( tid, 0);
		cellGcmSetVertexTextureControl( tid, CELL_GCM_FALSE, 0, 12<<8);
		cellGcmSetVertexTextureFilter( tid, 0);
	}

	cellGcmSetVertexDataBase( 0,0);
	cellGcmSetInvalidateTextureCache(  CELL_GCM_INVALIDATE_TEXTURE );
	cellGcmSetAlphaTestEnable( false );
#endif
}

UBOOL DwTriovizImpl_NeedsFinalize(void)
{
	return DwTrioviz_IsSideBySide() || DwTrioviz_IsTopBottom();
}

UBOOL DwTriovizImpl_IsSideBySide(void)
{
	return DwTrioviz_IsSideBySide();
}

INT DwTriovizImpl_RenderMenu(FCanvas* pCanvas, INT X, INT Y)
{
#if !FINAL_RELEASE
	if (DwTriovizImpl_IsTriovizAllowed())
	{
		g_pDrawTextCanvas = pCanvas;	
		INT NewPos = DwTrioviz_RenderMenu();
		g_pDrawTextCanvas = NULL;	
	}
#endif
	return (Y);
}

UBOOL DwTriovizImpl_ProcessMenuInput(INT ControllerId, FName const InputKeyName, BYTE EventType)
{
	UBOOL bResult = FALSE;

#if !FINAL_RELEASE
	if (DwTriovizImpl_IsTriovizAllowed())
	{
		static FName const Up(TEXT("XboxTypeS_DPad_Up"));
		static FName const Down(TEXT("XboxTypeS_DPad_Down"));
		static FName const Left(TEXT("XboxTypeS_DPad_Left"));
		static FName const Right(TEXT("XboxTypeS_DPad_Right"));
		static FName const AButton(TEXT("XboxTypeS_A"));
		static FName const BButton(TEXT("XboxTypeS_B"));
		static FName const UnlockEditUp(TEXT("XboxTypeS_LeftShoulder"));
		static FName const UnlockEditDown(TEXT("XboxTypeS_RightShoulder"));

		if ((EventType == IE_Pressed) || (EventType == IE_Repeat))
		{		
			EDwTriovizMenuAction CurrentInput = EDwTriovizMenuAction_MAX;
			if (InputKeyName == AButton)
			{
				CurrentInput = EDwTriovizMenuAction_Validate;
			}
			else if (InputKeyName == BButton)
			{
				CurrentInput = EDwTriovizMenuAction_Back;
			}
			else if (InputKeyName == Up)
			{
				CurrentInput = EDwTriovizMenuAction_Up;
			}
			else if (InputKeyName == Down)
			{
				CurrentInput = EDwTriovizMenuAction_Down;
			}
			else if (InputKeyName == Left)
			{
				CurrentInput = EDwTriovizMenuAction_Left;
			}
			else if (InputKeyName == Right)
			{
				CurrentInput = EDwTriovizMenuAction_Right;
			}
			else if (InputKeyName == UnlockEditUp)
			{
				CurrentInput = EDwTriovizMenuAction_UnlockEditUp;
			}
			else if (InputKeyName == UnlockEditDown)
			{
				CurrentInput = EDwTriovizMenuAction_UnlockEditDown;
			}

			bResult = DwTrioviz_ProcessInput(CurrentInput);
		}
	}
#endif

	return (bResult);
}

void DwTriovizImpl_ToggleMenu()
{
	DwTrioviz_ProcessInput(EDwTriovizMenuAction_ToggleMenu);
}

#if USE_BINK_CODEC
void DwTriovizImpl_BinkRender(FSurfaceRHIRef Backbuffer, int RenderTargetX, int RenderTargetY, int RenderTargetSizeX, int RenderTargetSizeY)
{
	/// Common
	GDwTriovizRenderDesc.m_DeltaWorldTime			= 0.033;//View.Family->DeltaWorldTime;
	GDwTriovizRenderDesc.m_UseSmoothTransition		= TRUE;

	GDwTriovizRenderDesc.m_ViewInfo.m_ViewIndex		= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetX	= RenderTargetX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetY	= RenderTargetY;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeX		= RenderTargetSizeX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeY		= RenderTargetSizeY;

	/// Depth management
	GDwTriovizRenderDesc.m_DepthControl.m_InvDeviceZToWorldZTransform = SDwVector4(1.0f, 1.0f, 1.0f, 1.0f);//View.InvDeviceZToWorldZTransform.X, View.InvDeviceZToWorldZTransform.Y, View.InvDeviceZToWorldZTransform.Z, View.InvDeviceZToWorldZTransform.W);
	GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureFactor = 1;
	GDwTriovizRenderDesc.m_DepthControl.m_DepthEncodeMode = EDwTriovizDepthEncodeMode_StandardEncoding_NormalDevSpace;//EDwTriovizDepthEncodeMode_SpecificEncoding_InvertDevSpace;

	UBOOL bDrawToHDR = FALSE;

	if(DwTriovizImpl_NeedsFinalize())
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_ExtraDepthTextureAlpha;

#if DW_XBOX
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorLDRTexture());
		DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorLDRSurface());
		GSceneRenderTargets.BeginRenderingSceneColorLDR();
		DwTriovizImpl_SetSceneColorTexture(GSceneRenderTargets.GetSceneColorLDRTexture());	
#elif DW_PS3
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorTexture());
		DwTriovizImpl_SetSceneColorTexture(GSceneRenderTargets.GetSceneColorTexture());	
		RHISetRenderTarget(GSceneRenderTargets.GetSceneColorLDRSurface(), FSurfaceRHIRef()); // Render to the final render target				
		DwTriovizImpl_SetSceneColorSurface(GSceneRenderTargets.GetSceneColorLDRSurface());	
		//FPS3RHISurface const * const pRHISurface = (FPS3RHISurface const * const)&(*GSceneRenderTargets.GetSceneColorSurface());
		//GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorOffset[0] = pRHISurface->ResolveTargetMemoryOffset;
		//GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[0] = pRHISurface->Pitch;
#endif
	}
	else
	{
		GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_ExtraDepthTextureAlpha;

#if DW_XBOX
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorLDRTexture());
		DwTriovizImpl_SetSceneColorSurface(Backbuffer);
		DwTriovizImpl_SetSceneColorTexture(GSceneRenderTargets.GetSceneColorLDRTexture());	
#elif DW_PS3
		DwTriovizImpl_SetExtraDepthTexture(GSceneRenderTargets.GetSceneColorTexture());
		DwTriovizImpl_SetSceneColorTexture(GSceneRenderTargets.GetSceneColorTexture());	
		RHISetRenderTarget(Backbuffer, FSurfaceRHIRef()); // Render to the final render target				
		DwTriovizImpl_SetSceneColorSurface(Backbuffer);	
#endif
	}

	/// EndProcess : Resolve needed ?
	GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve			= false;
	GDwTriovizRenderDesc.m_RenderTarget.m_IsGammaCorrected	= !GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve ;//|| View.bUseLDRSceneColor;
	/// Do the stuff !

	GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Full;

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 
		if(g_DwUseFramePacking)
		{
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.type = CELL_GCM_SURFACE_PITCH;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[0]  = CELL_GCM_LOCATION_LOCAL;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[1]  = CELL_GCM_LOCATION_LOCAL;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[2]  = CELL_GCM_LOCATION_LOCAL;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[3]  = CELL_GCM_LOCATION_LOCAL;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[1]  = 64;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[2]  = 64;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[3]  = 64;

			GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Prepare;
		}
#endif

		DwTriovizImpl_ResetRenderingStates(); 

		DwTrioviz_Render(GDwTriovizRenderDesc); 	

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
		if(g_DwUseFramePacking)
		{
			SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
			pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_PS3FramePackingInvert) ? EDwTrioviz3DMode_LeftEye : EDwTrioviz3DMode_RightEye;
			GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Uber;
			GDwLeftEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
			DwTrioviz_Render(GDwTriovizRenderDesc); 	

			pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_PS3FramePackingInvert) ? EDwTrioviz3DMode_RightEye : EDwTrioviz3DMode_LeftEye;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorOffset[0] += GDwRightOffsetStereo;
			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 6;
			GDwRightEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
			DwTrioviz_Render(GDwTriovizRenderDesc); 		

			GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 0;
		}	
#endif
		if(DwTriovizImpl_NeedsFinalize())
		{
#if DW_XBOX
			GSceneRenderTargets.FinishRenderingSceneColorLDR();
#endif
		}

	g_DwUseHDRSurface = FALSE;
}

void DwTriovizImpl_FinalizeBink(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget)
{
	if (!DwTrioviz_UpdateParameters(GDwTriovizRenderDesc))
	{
		return;
	}

	GDwTriovizRenderDesc.m_ViewInfo.m_ViewIndex		= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetX	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetY	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeX		= SizeX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeY		= SizeY;

	// Render to the final render target				
	DwTriovizImpl_ResetRenderingStates();

	DwTrioviz_SetPostProcessRenderStates(EDwTriovizColorChannel_Solid);

	GDwTriovizRenderDesc.m_RenderTarget.m_IsGammaCorrected	= false;
	GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve = false;

	DwTriovizImpl_SetSceneColorTexture(SceneColorTexture);	
	DwTriovizImpl_SetSceneColorSurface((RenderTarget != NULL) ? RenderTarget : GSceneRenderTargets.GetBackBuffer());				

	DwTrioviz_Finalize(GDwTriovizRenderDesc);  

	RHISetShaderRegisterAllocation(64, 64);
}
#endif//USE_BINK_CODEC

#if WITH_GFx_FULLSCREEN_MOVIE
void DwTriovizImpl_GfxRender(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget)
{
	DwTrioviz_UpdateParameters(GDwTriovizRenderDesc);

	GDwTriovizRenderDesc.m_ViewInfo.m_ViewIndex		= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetX	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetY	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeX		= SizeX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeY		= SizeY;

	// Render to the final render target				
	DwTriovizImpl_ResetRenderingStates();

	DwTrioviz_SetPostProcessRenderStates(EDwTriovizColorChannel_Solid);

	GDwTriovizRenderDesc.m_RenderTarget.m_IsGammaCorrected	= false;
	GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve = false;

	DwTriovizImpl_SetSceneColorTexture(SceneColorTexture);	
#if DWTRIOVIZSDK_PC_VIDEO
	DwTriovizImpl_SetSceneColorSurface(RenderTarget);
#else
	DwTriovizImpl_SetSceneColorSurface((RenderTarget != NULL) ? RenderTarget : GSceneRenderTargets.GetBackBuffer());
#endif

	GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureFactor = 1;
	//Use this if the depth is packed
	//GDwTriovizRenderDesc.m_DepthControl.m_DepthEncodeMode = EDwTriovizDepthEncodeMode_StandardEncoding_NormalDevSpace;
	//GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_MoviePacking;
	//Use this if the depth is stored in the alpha channel
	GDwTriovizRenderDesc.m_DepthControl.m_DepthTextureMode = EDwTriovizDepthTextureMode_SceneColorTextureAlpha;
	GDwTriovizRenderDesc.m_DepthControl.m_DepthEncodeMode = EDwTriovizDepthEncodeMode_StandardEncoding_NormalDevSpace;

	UBOOL bDrawToHDR = FALSE;

#if XBOX
	if (SceneColorTexture == GSceneRenderTargets.GetSceneColorTexture())
	{
		extern INT XeGetRenderTargetColorExpBias(D3DFORMAT SurfaceD3DFormat, D3DFORMAT TextureD3DFormat);		

		D3DFORMAT SurfaceD3DFormat = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface->Format;
		D3DFORMAT TextureD3DFormat = (D3DFORMAT)GPixelFormats[PF_FloatRGB].PlatformFormat;

		GDwTriovizRenderDesc.m_SceneColorBiasFactor = appPow(2.0f, XeGetRenderTargetColorExpBias(SurfaceD3DFormat, TextureD3DFormat));
	}
#endif

	GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Full;

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 
	if(g_DwUseFramePacking)
	{
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.type = CELL_GCM_SURFACE_PITCH;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[0]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[1]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[2]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorLocation[3]  = CELL_GCM_LOCATION_LOCAL;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[1]  = 64;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[2]  = 64;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorPitch[3]  = 64;

		GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Prepare;
	}
#endif

	DwTrioviz_Render(GDwTriovizRenderDesc); 

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
	if(g_DwUseFramePacking)
	{
		SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
		pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_LeftEye : EDwTrioviz3DMode_RightEye;
		GDwTriovizRenderDesc.m_Pass = EDwTriovizProcessPass_Uber;
		GDwLeftEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
		DwTrioviz_Render(GDwTriovizRenderDesc); 	

		pCurProfile->m_3DMode.m_3DMode = (!pCurProfile->m_Global.m_InvertEyes) ? EDwTrioviz3DMode_RightEye : EDwTrioviz3DMode_LeftEye;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.colorOffset[0] += GDwRightOffsetStereo;
		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 6;
		GDwRightEyeSurface = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface;
		DwTrioviz_Render(GDwTriovizRenderDesc); 		

		GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface.y = 0;
	}	
#endif

#if XBOX
	GDwTriovizRenderDesc.m_SceneColorBiasFactor = 1.0;
#endif

	RHISetShaderRegisterAllocation(64, 64);
}

void DwTriovizImpl_FinalizeGfxVideo(const FTexture2DRHIRef& SceneColorTexture, UINT SizeX, UINT SizeY, FSurfaceRHIRef RenderTarget)
{
#if DWTRIOVIZSDK
	if (!DwTrioviz_UpdateParameters(GDwTriovizRenderDesc))
	{
		return;
	}

	GDwTriovizRenderDesc.m_ViewInfo.m_ViewIndex		= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetX	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewOffsetY	= 0;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeX		= SizeX;
	GDwTriovizRenderDesc.m_ViewInfo.m_ViewSizeY		= SizeY;

	// Render to the final render target				
	DwTriovizImpl_ResetRenderingStates();

	DwTrioviz_SetPostProcessRenderStates(EDwTriovizColorChannel_Solid);

	GDwTriovizRenderDesc.m_RenderTarget.m_IsGammaCorrected	= false;
	GDwTriovizRenderDesc.m_RenderTarget.m_DoResolve = false;

	DwTriovizImpl_SetSceneColorTexture(SceneColorTexture);	
	DwTriovizImpl_SetSceneColorSurface((RenderTarget != NULL) ? RenderTarget : GSceneRenderTargets.GetBackBuffer());				

#if XBOX
	if (SceneColorTexture == GSceneRenderTargets.GetSceneColorTexture())
	{
		extern INT XeGetRenderTargetColorExpBias(D3DFORMAT SurfaceD3DFormat, D3DFORMAT TextureD3DFormat);		

		D3DFORMAT SurfaceD3DFormat = GDwTriovizRenderDesc.m_RenderTarget.m_SceneColorSurface.m_pSurface->Format;
		D3DFORMAT TextureD3DFormat = (D3DFORMAT)GPixelFormats[PF_FloatRGB].PlatformFormat;

		GDwTriovizRenderDesc.m_SceneColorBiasFactor = appPow(2.0f, XeGetRenderTargetColorExpBias(SurfaceD3DFormat, TextureD3DFormat));
	}
#endif //XBOX

	DwTrioviz_Finalize(GDwTriovizRenderDesc);  

	RHISetShaderRegisterAllocation(64, 64);

#if XBOX
	GDwTriovizRenderDesc.m_SceneColorBiasFactor = 1.0;
#endif //XBOX
#endif //DWTRIOVIZSDK
}
#endif //WITH_GFx_FULLSCREEN_MOVIE

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
#include <sysutil/sysutil_sysparam.h>

const FSurfaceRHIRef& FDwSceneRenderTargetBackbufferStereoProxy::GetRenderTargetSurface() const
{
	FPS3RHISurface  *  pRHISurface = (FPS3RHISurface  * )&(*GSceneRenderTargets.GetBackBuffer());

	if (GDwLeftEyeStereoSurface == NULL)
	{
		GDwLeftEyeStereoSurface = new FPS3RHISurface();
		GDwRightEyeStereoSurface = new FPS3RHISurface();
		GDwLeftEyeStereoSurfaceRef = GDwLeftEyeStereoSurface;
		GDwRightEyeStereoSurfaceRef = GDwRightEyeStereoSurface;
	}

	if (EyeIndex == 0)
	{
		*GDwLeftEyeStereoSurface = *pRHISurface;
		GDwLeftEyeStereoSurface->SizeY = GScreenHeight;
		GDwLeftEyeStereoSurface->bUseRightStereo = FALSE;
		return (GetLeftEye()); 
	}

	*GDwRightEyeStereoSurface = *pRHISurface;
	GDwRightEyeStereoSurface->SizeY = GScreenHeight;
	GDwRightEyeStereoSurface->MemoryOffset += GDwRightOffsetStereo;
	GDwRightEyeStereoSurface->bUseRightStereo = TRUE;
	return (GetRightEye()); 
}

void DwTriovizImpl_SwitchFramePacking3D(void)
{
	extern UBOOL g_Dw720p3DAvailable;
	if (IsInRenderingThread())
	{
		GPS3Gcm->BlockUntilGPUIdle();

		if (g_DwUseFramePacking)
		{			
			// Change resolution and wait
			INT BufferWidth = ((GScreenWidth + 0x3f) & ~0x3f) * 4;
			DWORD ColorBufferPitch = cellGcmGetTiledPitchSize(BufferWidth);

			CellVideoOutConfiguration videoCfg;
			memset(&videoCfg, 0, sizeof(CellVideoOutConfiguration));
			videoCfg.resolutionId = CELL_VIDEO_OUT_RESOLUTION_720;
			videoCfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
			videoCfg.pitch = ColorBufferPitch;

			if(cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &videoCfg, NULL, 0) !=0 ) 
			{
				check(FALSE);
			}		
			cellGcmSetFlipMode( CELL_GCM_DISPLAY_HSYNC );
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGame3DTV);
			g_DwUseFramePacking = FALSE;
			g_DwPendingSwitch3DMode = TRUE;
			g_DwPendingSwitch3DModeFirst = TRUE;
		}
		else if(g_Dw720p3DAvailable)
		{
			// Change resolution and wait
			INT BufferWidth = ((GScreenWidth + 0x3f) & ~0x3f) * 4;
			DWORD ColorBufferPitch = cellGcmGetTiledPitchSize(BufferWidth);

			CellVideoOutConfiguration videoCfg;
			memset(&videoCfg, 0, sizeof(CellVideoOutConfiguration));
			videoCfg.resolutionId = CELL_VIDEO_OUT_RESOLUTION_720_3D_FRAME_PACKING;
			videoCfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
			videoCfg.pitch = ColorBufferPitch;

			if(cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &videoCfg, NULL, 0) !=0 ) 
			{
				check(FALSE);
			}	
			cellGcmSetFlipMode( CELL_GCM_DISPLAY_VSYNC );
			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGameFramePacking);
			g_DwUseFramePacking = TRUE;
		}

		GPS3Gcm->BlockUntilGPUIdle();

		DwTrioviz_UpdateParameters(GDwTriovizRenderDesc);
	}
}
#endif//DWTRIOVIZSDK_PS3_FRAMEPACKING_1280

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
UBOOL g_DwUseFramePacking = FALSE;
UBOOL g_Dw720p3DAvailable = FALSE;
XGSTEREOPARAMETERS g_DwStereoParams;
FSharedTexture2DRHIRef g_DwFrontBuffer;
FSharedMemoryResourceRHIRef g_DwMemoryFrontBuffer;
FTexture2DRHIRef g_DwStereoFrontBuffer;
IDirect3DTexture9* g_DwStereoFrontBufferDx9;
IDirect3DTexture9* g_DwStereoSecondFrontBufferDx9;
IDirect3DSurface9* g_DwStereoBackBuffer;
D3DPRESENT_PARAMETERS g_DwPresentParameters;
extern IDirect3DDevice9* GDirect3DDevice;

void DwTriovizImpl_DrawSceneColor(void);

FSharedTexture2DRHIRef DwCreateSharedTexture2D(UINT SizeX,UINT SizeY,D3DFORMAT Format,UINT NumMips,FSharedMemoryResourceRHIParamRef SharedMemory,DWORD Flags)
{
	// create the D3D resource
	IDirect3DTexture9* Resource = new IDirect3DTexture9;

	// set the texture header
	UINT TextureSize = XGSetTextureHeader(
		SizeX,
		SizeY,
		NumMips,
		0,
		Format,
		0,
		0,
		0,
		0,
		Resource,
		NULL,
		NULL
		);

	UINT AlignedSize = Align(TextureSize,D3DTEXTURE_ALIGNMENT);
	check( AlignedSize <= SharedMemory->Size );

	// setup the shared memory
	XGOffsetResourceAddress( Resource, SharedMemory->BaseAddress );

	// keep track of the fact that the D3D resource is using the shared memory
	SharedMemory->AddResource(Resource);

	// create the 2d texture resource
	FSharedTexture2DRHIRef SharedTexture2D(new FXeSharedTexture2D(Resource, PF_A8R8G8B8, SharedMemory));

	return SharedTexture2D;
}

void DwTriovizImpl_SwitchFramePacking3D(void)
{
	if (IsInRenderingThread() && g_Dw720p3DAvailable)
	{
		RHIBlockUntilGPUIdle();

		if (g_DwUseFramePacking)
		{			
			//Reset Dx9 device
			g_DwPresentParameters.BackBufferWidth = GScreenWidth;
			g_DwPresentParameters.BackBufferHeight = GScreenHeight;

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
			g_DwPresentParameters.PresentationInterval = GSystemSettings.bUseVSync ? D3DPRESENT_INTERVAL_TWO : D3DPRESENT_INTERVAL_IMMEDIATE;
#endif
			g_DwPresentParameters.Flags &= ~D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;

			GDirect3DDevice->BlockUntilIdle();
			VERIFYD3DRESULT(GDirect3DDevice->Reset(&g_DwPresentParameters));

			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, false);
			//DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGame3DTV);

			g_DwUseFramePacking = FALSE;
		}
		else
		{
			//Reset Dx9 device
			g_DwPresentParameters.BackBufferWidth = g_DwStereoParams.FrontBufferWidth;
			g_DwPresentParameters.BackBufferHeight = g_DwStereoParams.FrontBufferHeight;

#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_VSYNC
			g_DwPresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
#endif
			g_DwPresentParameters.Flags |= D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;

			GDirect3DDevice->BlockUntilIdle();
			VERIFYD3DRESULT(GDirect3DDevice->Reset(&g_DwPresentParameters));

			DwTrioviz_SetFlag(EDwTriovizFlag_InGame, true);
			DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_InGameFramePacking);

			g_DwUseFramePacking = TRUE;
		}

		DwTrioviz_UpdateParameters(GDwTriovizRenderDesc);
	}
}

void DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(INT EyeIndex, UBOOL UseTrioviz, UBOOL ApplyLeftEyeEffect)
{
	//Copy the Backbuffer to the stereo front buffer (1280 * 1470
	//For left eye, resolve 1280 * 720 backbuffer to the front buffer texture
	//For right eye, resolve 1280 * 720 backbuffer to a temporary texture and redraw it to a 1280 * 744 surface then resolve it to front buffer
	//		as a 1280 * 720 surface can not be resolved to the stereo front buffer
	if (g_DwUseFramePacking)
	{
		if (EyeIndex == 0)
		{
			DW_DRAW_EVENT(DwTriovizLeftEyeCopyBackbuffer);

			if (UseTrioviz && ApplyLeftEyeEffect)
			{
				FDwSceneViewCache View;
				View.DeltaWorldTime = 1.0f / 30.0f;
				View.NumView = 1;
				View.SizeX = GScreenWidth;
				View.SizeY = GScreenHeight;
				View.RenderTargetSizeX  = GScreenWidth;
				View.RenderTargetSizeY = GScreenHeight;
				View.InvDeviceZToWorldZTransform = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
				View.bUseLDRSceneColor = TRUE;
				View.bUseMoviePackingWithDisparity = TRUE;
				View.bUseNormalDevSpace = TRUE;
				View.bUseZFromAlpha = TRUE;
				View.bUseSmoothTransition = FALSE;
				g_DwSplitScreeenViewInfo[0] = View;
				DwTriovizImpl_Render(&g_DwSplitScreeenViewInfo[0], TRUE, RTUsage_FullOverwrite);
			}

			//Resolve the backbuffer to the left eye region in frontbuffer
			VERIFYD3DRESULT(GDirect3DDevice->Resolve(0, &g_DwStereoParams.LeftEye.ResolveSourceRect, ((IDirect3DTexture9*)g_DwStereoFrontBufferDx9), &g_DwStereoParams.LeftEye.ResolveDestPoint, 0, 0, 0, 0, 0, NULL));

			//Render right eye
			if (UseTrioviz)
			{
				if (g_DwSplitPlayerCount > 0)
				{
					for (INT Index = 0; Index < g_DwSplitPlayerCount; ++Index)
					{
						DW_DRAW_EVENT(DwTriovizImpl_RenderTriovizSplitscreen);
						DwTriovizImpl_Render(&g_DwSplitScreeenViewInfo[Index], TRUE, RTUsage_FullOverwrite, TRUE) ;
					}

					g_DwSplitPlayerCount = 0;
				}
				else
				{
					DwTriovizImpl_Render(&g_DwSplitScreeenViewInfo[0], TRUE, RTUsage_FullOverwrite, TRUE);
				}
			}
		}
		else
		{
			DW_DRAW_EVENT(DwTriovizRightEyeCopyBackbuffer);

			//Resolve the bacbuffer to the SceneColorLDR texture
			IDirect3DTexture9 * pSceneColorLDR = ((IDirect3DTexture9*)GSceneRenderTargets.GetSceneColorLDRTexture());
			VERIFYD3DRESULT(GDirect3DDevice->Resolve(0, &g_DwStereoParams.LeftEye.ResolveSourceRect, ((IDirect3DTexture9*)pSceneColorLDR), &g_DwStereoParams.LeftEye.ResolveDestPoint, 0, 0, 0, 0, 0, NULL));

			//Setup the stereo backbuffer (1280 * 734)
			GDirect3DDevice->SetRenderTarget(0, g_DwStereoBackBuffer);
			GDirect3DDevice->SetDepthStencilSurface(NULL);

			D3DVIEWPORT9 RenderViewport = { 0, g_DwStereoParams.RightEye.ViewportYOffset, GScreenWidth, GScreenHeight, 0, 1 };
			GDirect3DDevice->SetViewport(&RenderViewport);
			
			//Render the SceneColorLDR to the stereo backbuffer
			DwTriovizImpl_DrawSceneColor();

			//Resolve the stereo backbuffer to the right eye region in frontbuffer
			VERIFYD3DRESULT(GDirect3DDevice->Resolve(0, &g_DwStereoParams.RightEye.ResolveSourceRect, ((IDirect3DTexture9*)g_DwStereoFrontBufferDx9), &g_DwStereoParams.RightEye.ResolveDestPoint, 0, 0, NULL, 0.0f, 0, NULL));
		}
	}
}

void DwTriovizImpl_XboxFramePackingCopyBackbuffer(INT EyeIndex)
{
	if( IsInRenderingThread() )
	{
		DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(EyeIndex);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DwTriovizImpl_XboxFramePackingCopyBackbuffer_Command,
			INT,Index,EyeIndex,
		{
			DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(Index);
		});	
	}
}

#if USE_BINK_CODEC
void DwTriovizImpl_RenderBink(UBOOL ApplyEffect)
{
	//Switch to menu profile
	UINT CurrentProfileId = DwTrioviz_GetCurrentProfileId();
	DwTrioviz_SetCurrentProfileId(EDwTriovizProfile_Menu);

	DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(0, ApplyEffect, TRUE);
	DwTriovizImpl_XboxFramePackingCopyBackbuffer_RenderThread(1, ApplyEffect);

	DwTrioviz_SetCurrentProfileId(CurrentProfileId);
}
#endif

#endif//DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280


void DwTriovizImpl_UseHDRSurface(UBOOL bUseHDR)
{
	if( IsInRenderingThread() )
	{
		g_DwUseHDRSurface = bUseHDR;
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DwTriovizImpl_UseHDRSurface_Command,
			UBOOL,bUseHDRValue,bUseHDR,
		{
			g_DwUseHDRSurface = bUseHDRValue;
		});	
	}
}

float DwTriovizImpl_GetCurrentProfileForegroundDisparityFactor()
{
	SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
	return pCurProfile->m_Disparity.m_PercentFactorForeground;
}

void DwTriovizImpl_SetCurrentProfileMultiplier(float fDwMultiplier)
{
	SDwTriovizParms *  pCurProfile = (SDwTriovizParms *) &DwTrioviz_GetProfile(DwTrioviz_GetCurrentProfileId());
	float fCorrectedMultiplier;
	if(pCurProfile->m_3DMode.m_3DMode == EDwTrioviz3DMode_ColorFilter)
	{
		fCorrectedMultiplier = 1.0f + (fDwMultiplier * 0.2f);

	}else
	{
		fCorrectedMultiplier = 2.0f + (fDwMultiplier);
	}

	pCurProfile->m_Global.m_Multiplier = fCorrectedMultiplier;
}

void DwTriovizImpl_PrepareCapturing(UCanvas* CanvasObject, UBOOL DwUseCinematicMode)
{
	if (DwTriovizImpl_IsCinematicMode() != DwUseCinematicMode)
	{
		DwTriovizImpl_ToggleCinematicMode(DwUseCinematicMode);
	}		

#if _WINDOWS //Frame dunping
	if (DwTriovizImpl_IsTriovizCapturingDepth()
		&& CanvasObject && CanvasObject->SceneView && CanvasObject->SceneView->PostProcessSettings)
	{
		FVector4 CreateInvDeviceZToWorldZTransform(FMatrix const & ProjectionMatrix, UBOOL InvertZ);
		DwTriovizImpl_SetInvDeviceZToWorldZTransform(CreateInvDeviceZToWorldZTransform(CanvasObject->SceneView->ProjectionMatrix, TRUE));
	}

	if (DwUseCinematicMode && DwTriovizImpl_IsTriovizCapturingDepth()
		&& CanvasObject && CanvasObject->SceneView && CanvasObject->SceneView->PostProcessSettings)
	{
		DwTriovizImpl_SetFocalDistanceCinematicMode(CanvasObject->SceneView->PostProcessSettings->DOF_FocusDistance);
	}
#endif
}

#if _WINDOWS
//Frame dumping helpers

#define DW_MAX_Z_DUMPING	3500.0f	//Max Z relevant value
									//Might want to tweak that per game
									//Far plane is usually too far to get a nice range of values

/**
* Helper for storing IEEE 32 bit float components
*/
struct FFloatIEEE
{
	union
	{
		struct
		{
			DWORD	Mantissa : 23,
					Exponent : 8,
					Sign : 1;
		} Components;

		FLOAT	Float;
	};
};

/**
* Helper for storing 16 bit float components
*/
struct FD3DFloat16
{
	union
	{
		struct
		{
			WORD	Mantissa : 10,
					Exponent : 5,
					Sign : 1;
		} Components;

		WORD	Encoded;
	};

	/**
	* @return full 32 bit float from the 16 bit value
	*/
	operator FLOAT()
	{
		FFloatIEEE	Result;

		Result.Components.Sign = Components.Sign;
		Result.Components.Exponent = Components.Exponent - 15 + 127; // Stored exponents are biased by half their range.
		Result.Components.Mantissa = Min<DWORD>(appFloor((FLOAT)Components.Mantissa / 1024.0f * 8388608.0f),(1 << 23) - 1);

		return Result.Float;
	}
};

FLOAT DwConvertFromDeviceZ(FLOAT DeviceZ, FLOAT OneDivDepthAdd, FLOAT DepthAddDivDepthMul)
{
	return Clamp(1.f / (DeviceZ * OneDivDepthAdd - DepthAddDivDepthMul), 0.0f, DW_MAX_Z_DUMPING);	
}

void DwTriovizImpl_PrepareAlphaDepthWithFocal(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData,BYTE FocalDistance)
{
	UINT const Width = (MaxX - MinX + 1);
	UINT const Height = (MaxY - MinY + 1);
	UINT const MarginSize = 8;
	UINT const DownsampledWidth = Width / 2;
	UINT const DownsampledHeight = Height / 2;

	TArray<BYTE> DepthImage;
	DepthImage.Add(DownsampledWidth * DownsampledHeight);

	//Copy the alpha
	for( UINT Y = MinY; Y <= MaxY; Y+=2 )
	{
		FColor* SrcPtr = (FColor*)(BYTE*)&OutData(Y * Width * 4);

		for( UINT X = MinX; X <= MaxX; X+=2 )
		{
			DepthImage(X / 2 + Y / 2 * DownsampledWidth) = SrcPtr->A;
			SrcPtr++;
			SrcPtr++;
		}
	}

	//Copy back the depth
	for( UINT Y = MinY; Y <= MaxY; Y++ )
	{
		FColor* DstPtr = (FColor*)(BYTE*)&OutData(Y * Width * 4);

		for( UINT X = MinX; X <= MaxX; X++ )
		{
			//Copy the depth
			if (X < DownsampledWidth && Y < DownsampledHeight)
			{
				DstPtr->A = DepthImage(X + Y * DownsampledWidth);
			}
			//Copy the margin
			else if(X < DownsampledWidth + MarginSize && Y < DownsampledHeight + MarginSize)
			{				
				DstPtr->A = DepthImage(Clamp<UINT>(X, 0, Width / 2 - 1) + Clamp<UINT>(Y, 0, Height / 2 - 1) * DownsampledWidth);
			}
			//Copy the focal distance
			else
			{
				DstPtr->A = FocalDistance;
			}
			DstPtr++;
		}
	}
}

void DwTriovizImpl_CaptureDepth(UINT MinX,UINT MinY,UINT MaxX,UINT MaxY,TArray<BYTE>& OutData)
{
	extern FLOAT g_DwFocalCinematicMode;
	extern FVector4 g_DwInvDeviceZToWorldZTransform;
	if (DwTriovizImpl_IsTriovizCapturingDepth())
	{
		UINT SizeX = MaxX - MinX + 1;
		UINT SizeY = MaxY - MinY + 1;

		//Grab the Z from SceneColor as 16bits values to get more precision
		//and normalize it to get a nice range

		IDirect3DSurface9 * SceneColorDx9 = GetPlatformSurface(GSceneRenderTargets.GetSceneColorSurface());	
		D3DSURFACE_DESC SurfaceDescDepth;
		VERIFYD3D9RESULT(SceneColorDx9->GetDesc(&SurfaceDescDepth));
		TRefCountPtr<IDirect3DTexture9> TextureDepth2D;
		VERIFYD3D9RESULT(GLegacyDirect3DDevice9->CreateTexture(
			SurfaceDescDepth.Width,
			SurfaceDescDepth.Height,
			1,
			0,
			SurfaceDescDepth.Format,
			D3DPOOL_SYSTEMMEM,
			TextureDepth2D.GetInitReference(),
			NULL
			));
		// get its surface
		TRefCountPtr<IDirect3DSurface9> DestSurfaceDepth;
		VERIFYD3D9RESULT(TextureDepth2D->GetSurfaceLevel(0,DestSurfaceDepth.GetInitReference()));
		GLegacyDirect3DDevice9->GetRenderTargetData(SceneColorDx9,DestSurfaceDepth.GetReference());

		D3DLOCKED_RECT	LockedRectDepth;
		VERIFYD3D9RESULT(DestSurfaceDepth->LockRect(&LockedRectDepth,0,D3DLOCK_READONLY));

		FPlane	MinValue(FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX),
			MaxValue(-FLT_MAX,-FLT_MAX,-FLT_MAX,-FLT_MAX);

		FLOAT const OneDivDepthAdd = g_DwInvDeviceZToWorldZTransform.Z;
		FLOAT const DepthAddDivDepthMul = g_DwInvDeviceZToWorldZTransform.W;

		check(sizeof(FD3DFloat16)==sizeof(WORD));

		if (SurfaceDescDepth.Format == D3DFMT_A32B32G32R32F)
		{
			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FLOAT*	SrcPtr = (FLOAT*)((BYTE*)LockedRectDepth.pBits + (Y - MinY) * LockedRectDepth.Pitch);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					MinValue.X = Min<FLOAT>(SrcPtr[0],MinValue.X);
					MinValue.Y = Min<FLOAT>(SrcPtr[1],MinValue.Y);
					MinValue.Z = Min<FLOAT>(SrcPtr[2],MinValue.Z);
					MinValue.W = Min<FLOAT>(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul),MinValue.W);
					MaxValue.X = Max<FLOAT>(SrcPtr[0],MaxValue.X);
					MaxValue.Y = Max<FLOAT>(SrcPtr[1],MaxValue.Y);
					MaxValue.Z = Max<FLOAT>(SrcPtr[2],MaxValue.Z);
					MaxValue.W = Max<FLOAT>(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul),MaxValue.W);
					SrcPtr += 4;
				}
			}
		}
		else
		{
			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FD3DFloat16*	SrcPtr = (FD3DFloat16*)((BYTE*)LockedRectDepth.pBits + (Y - MinY) * LockedRectDepth.Pitch);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					MinValue.X = Min<FLOAT>(SrcPtr[0],MinValue.X);
					MinValue.Y = Min<FLOAT>(SrcPtr[1],MinValue.Y);
					MinValue.Z = Min<FLOAT>(SrcPtr[2],MinValue.Z);
					MinValue.W = Min<FLOAT>(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul),MinValue.W);
					MaxValue.X = Max<FLOAT>(SrcPtr[0],MaxValue.X);
					MaxValue.Y = Max<FLOAT>(SrcPtr[1],MaxValue.Y);
					MaxValue.Z = Max<FLOAT>(SrcPtr[2],MaxValue.Z);
					MaxValue.W = Max<FLOAT>(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul),MaxValue.W);
					SrcPtr += 4;
				}
			}
		}

		if (SurfaceDescDepth.Format == D3DFMT_A32B32G32R32F)
		{
			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FLOAT* SrcPtr = (FLOAT*)((BYTE*)LockedRectDepth.pBits + (Y - MinY) * LockedRectDepth.Pitch);
				FColor* DestPtr = (FColor*)(BYTE*)&OutData(Y * SizeX * 4);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					FColor NormalizedColor(
						FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul) - MinValue.W) / (MaxValue.W - MinValue.W)
						));
					DestPtr->A = NormalizedColor.A;
					DestPtr++, SrcPtr += 4;
				}
			}		
		}
		else
		{
			for( UINT Y = MinY; Y <= MaxY; Y++ )
			{
				FD3DFloat16* SrcPtr = (FD3DFloat16*)((BYTE*)LockedRectDepth.pBits + (Y - MinY) * LockedRectDepth.Pitch);
				FColor* DestPtr = (FColor*)(BYTE*)&OutData(Y * SizeX * 4);

				for( UINT X = MinX; X <= MaxX; X++ )
				{
					FColor NormalizedColor(
						FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(DwConvertFromDeviceZ(SrcPtr[3], OneDivDepthAdd, DepthAddDivDepthMul) - MinValue.W) / (MaxValue.W - MinValue.W)
						));
					DestPtr->A = NormalizedColor.A;
					DestPtr++, SrcPtr += 4;
				}
			}	
		}

		DestSurfaceDepth->UnlockRect();

		if (DwTriovizImpl_IsCinematicMode())
		{
			//Store the focal distance in the depth info
			//Stretch the depth buffer, add a margin and draw a quad with the focal distance
			FColor NormalizedColor(
				FLinearColor(
				0.0f, 0.0f,0.0f,
				(Clamp(g_DwFocalCinematicMode, 0.0f, DW_MAX_Z_DUMPING) - MinValue.W) / (MaxValue.W - MinValue.W)
				)); 
			BYTE FocalDistance = NormalizedColor.A;
			DwTriovizImpl_PrepareAlphaDepthWithFocal(MinX,MinY,MaxX,MaxY,OutData, FocalDistance);
		}
	}
}

void DwTriovizImpl_SavePNG(TCHAR * File, INT Width, INT Height, FColor* Data)
{
	if (GLegacyDirect3DDevice9)
	{
		TRefCountPtr<IDirect3DTexture9> TextureDepth2D;
		VERIFYD3D9RESULT(GLegacyDirect3DDevice9->CreateTexture(
			Width,
			Height,
			1,
			0,
			D3DFMT_A8R8G8B8,
			D3DPOOL_MANAGED,
			TextureDepth2D.GetInitReference(),
			NULL
			));

		D3DLOCKED_RECT	LockedRectDepth;
		VERIFYD3D9RESULT(TextureDepth2D->LockRect(0, &LockedRectDepth,0,D3DLOCK_NOSYSLOCK));

		for( INT Y = 0; Y < Height; Y++ )
		{		
			FColor* DestPtr = (FColor*)((BYTE*)LockedRectDepth.pBits + (Y) * LockedRectDepth.Pitch);
			FColor* SrcPtr = (FColor*)(BYTE*)&Data[Y * Width];

			memcpy(DestPtr, SrcPtr, LockedRectDepth.Pitch);
		}

		TextureDepth2D->UnlockRect(0);

		TCHAR PngFile[MAX_SPRINTF]=TEXT("");
		appStrcpy(PngFile, File);
		UINT const PngFileLen = appStrlen(PngFile);
		PngFile[PngFileLen - 3] = 'p', PngFile[PngFileLen - 2] = 'n', PngFile[PngFileLen - 1] = 'g';

		D3DXSaveTextureToFile(PngFile, D3DXIFF_PNG, TextureDepth2D, NULL);
	}
}
#endif



void DwTriovizImpl_HDR_To_LDR(FDwSceneViewCache * pView)
{
	if(!pView->bUseLDRSceneColor)
	{
		// Set the view family's render target/viewport.
		//RHISetRenderTarget(ViewFamily.RenderTarget->GetRenderTargetSurface(),FSurfaceRHIRef());	
		GSceneRenderTargets.BeginRenderingSceneColorLDR( RTUsage_FullOverwrite );

		// turn off culling and blending
		RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
		RHISetBlendState(TStaticBlendState<>::GetRHI());

		// turn off depth reads/writes
		RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

		TShaderMapRef<FGammaCorrectionVertexShader> VertexShader(GetGlobalShaderMap());
		TShaderMapRef<FGammaCorrectionPixelShader> PixelShader(GetGlobalShaderMap());

		static FGlobalBoundShaderState DwTriovizPostProcessBoundShaderState;
		SetGlobalBoundShaderState( DwTriovizPostProcessBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

		FLOAT InvDisplayGamma = 1.0f / pView->DisplayGamma;

		SetPixelShaderValue(
			PixelShader->GetPixelShader(),
			PixelShader->InverseGammaParameter,
			InvDisplayGamma
			);
		SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->ColorScaleParameter,pView->ColorScale);
		SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->OverlayColorParameter,pView->OverlayColor);

		SetTextureParameter(
			PixelShader->GetPixelShader(),
			PixelShader->SceneTextureParameter,
			TStaticSamplerState<>::GetRHI(),
			GSceneRenderTargets.GetSceneColorTexture()
			);

		// Draw a quad mapping scene color to the view's render target
		DrawDenormalizedQuad(
			pView->X,pView->Y,
			pView->SizeX,pView->SizeY,
			pView->RenderTargetX,pView->RenderTargetY,
			pView->RenderTargetSizeX,pView->RenderTargetSizeY,
			GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
			GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY()
			);

		GSceneRenderTargets.FinishRenderingSceneColorLDR();

		pView->bUseLDRSceneColor = !pView->bUseLDRSceneColor;
	}
}



#if DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280 || DWTRIOVIZSDK_PS3_FRAMEPACKING_640
void DwTriovizImpl_DrawSceneColor(void)
{
	// turn off culling and blending
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	// turn off depth reads/writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	TShaderMapRef<FGammaCorrectionVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FGammaCorrectionPixelShader> PixelShader(GetGlobalShaderMap());

	static FGlobalBoundShaderState DwTriovizDrawScenePostProcessBoundShaderState;
	SetGlobalBoundShaderState( DwTriovizDrawScenePostProcessBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	FLOAT InvDisplayGamma = 1.0f;

	SetPixelShaderValue(
		PixelShader->GetPixelShader(),
		PixelShader->InverseGammaParameter,
		InvDisplayGamma
		);
	SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->ColorScaleParameter,FVector4(1.0, 1.0, 1.0, 1.0f));
	SetPixelShaderValue(PixelShader->GetPixelShader(),PixelShader->OverlayColorParameter,FVector4(0.0, 0.0, 0.0, 0.0f));

	SetTextureParameter(
		PixelShader->GetPixelShader(),
		PixelShader->SceneTextureParameter,
		TStaticSamplerState<>::GetRHI(),
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_640
		GSceneRenderTargets.GetPS3TemporaryBackbufferTexture()
#else
		GSceneRenderTargets.GetSceneColorLDRTexture()
#endif
		);

	// Draw a quad mapping scene color to the view's render target
	DrawDenormalizedQuad(
		0,0,
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_640
		GSceneRenderTargets.GetPS3TemporaryBackbufferSizeX(),GSceneRenderTargets.GetPS3TemporaryBackbufferSizeY(),
#else
		GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
#endif
		0,0,
		GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_640
		GSceneRenderTargets.GetPS3TemporaryBackbufferSizeX(),GSceneRenderTargets.GetPS3TemporaryBackbufferSizeY(),
#else
		GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY(),
#endif
		GSceneRenderTargets.GetBufferSizeX(),GSceneRenderTargets.GetBufferSizeY()
		);
}
#endif



// EPICSTART // moved code from engine into this file

#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280
UINT DwTriovizImpl_GetPS3DepthBufferHeight(UINT BufferSizeY)
{
	//make sure the size allocated to the depth buffer is 6 rows longer than the render target in order for PS3's HDMI 1.4 support to work.
	if(g_Dw720p3DAvailable)
	{
		return BufferSizeY + 6;
	}
}
#endif

UBOOL DwTriovizImpl_Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if( ParseCommand(&Cmd,TEXT("TRIOVIZMENU")) )
	{
		FlushRenderingCommands();
		DwTriovizImpl_ToggleMenu();
		return TRUE;	
	}
#if DWTRIOVIZSDK_PS3_FRAMEPACKING_1280 || DWTRIOVIZSDK_XBOX_FRAMEPACKING_1280
	else if( ParseCommand(&Cmd,TEXT("TriovizSwitch3DTV")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Switch3DRenderCommand, { DwTriovizImpl_SwitchFramePacking3D(); } );
		return TRUE;	
	}
	else if( ParseCommand(&Cmd,TEXT("TriovizStrong3D")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Strong3DRenderCommand, { DwTriovizImpl_Strong3D(); } );
		return TRUE;	
	}
	else if( ParseCommand(&Cmd,TEXT("TriovizSubtle3D")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Subtle3DRenderCommand, { DwTriovizImpl_Subtle3D(); } );
		return TRUE;	
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("TRIOVIZUISWITCH3D")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Switch3DForUIRenderCommand, { DwTriovizImpl_Switch3DForUI(); } );
		return TRUE;	
	}
	else if( ParseCommand(&Cmd,TEXT("TRIOVIZUIENABLE3D")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Enable3DForUIRenderCommand, { DwTriovizImpl_Enable3DForUI(true); } );
		return TRUE;	
	}
	else if( ParseCommand(&Cmd,TEXT("TRIOVIZUIDISABLE3D")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_Disable3DForUIRenderCommand, { DwTriovizImpl_Disable3DForUI(); } );
		return TRUE;	
	}
	else if( ParseCommand(&Cmd,TEXT("TRIOVIZSET3DINTENSITY")) )
	{
		FString DwTriovizIntensity(ParseToken(Cmd, 0));
		float fDwTriovizIntensity   = ( DwTriovizIntensity.Len()   > 0 ) ? appAtof(*DwTriovizIntensity)   : 2.50;
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DwTriovizImpl_Set3DIntesityRenderCommand, FLOAT, Intensity, fDwTriovizIntensity, {DwTriovizImpl_Set3DIntesity(Intensity);});
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TRIOVIZSET3DMODE")) )
	{
		FString DwTriovizMode(ParseToken(Cmd, 0));
		if(DwTriovizMode.Len() > 0)
		{
			ETrioviz3DMode eDwTrioviz3DMode   = (ETrioviz3DMode)(appAtoi(*DwTriovizMode) % (int)ETrioviz3DMode_max);
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(DwTriovizImpl_Set3DModeRenderCommand, INT, Mode, eDwTrioviz3DMode, {DwTriovizImpl_Set3DMode(Mode);} );
		}
		return TRUE;
	}
	else if( ParseCommand(&Cmd,TEXT("TriovizSwitchCaptureDepth")) )
	{
		FlushRenderingCommands();
		ENQUEUE_UNIQUE_RENDER_COMMAND( DwTriovizImpl_CaptureDepthRenderCommand, { DwTriovizImpl_SwitchCaptureDepth(); } );
		return TRUE;	
	}

	return FALSE;
}

// EPICEND

#endif	// DWTRIOVIZSDK



