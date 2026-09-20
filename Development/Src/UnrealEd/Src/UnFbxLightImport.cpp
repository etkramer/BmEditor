/*=============================================================================
	Light actor creation from FBX data.
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX

#include "Factories.h"
#include "Engine.h"
#include "EngineMaterialClasses.h"

#include "UnFbxImporter.h"

using namespace UnFbx;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
ALight* CFbxImporter::CreateLight(fbx::FbxLight* FbxLight)
{
	ALight* UnrealLight = NULL;
	FString ActorName = ANSI_TO_TCHAR(MakeName(FbxLight->GetName()));

	// create the light actor
	switch (FbxLight->LightType.Get())
	{
	case fbx::FbxLight::ePoint:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(APointLight::StaticClass(),*ActorName));
		break;
	case fbx::FbxLight::eDirectional:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(ADirectionalLight::StaticClass(),*ActorName));
		break;
	case fbx::FbxLight::eSpot:
		UnrealLight = Cast<ALight>(GWorld->SpawnActor(ASpotLight::StaticClass(),*ActorName));
		break;
	}

	if (UnrealLight)
	{
		FillLightComponent(FbxLight,UnrealLight->LightComponent);	
	}
	
	return UnrealLight;
}

UBOOL CFbxImporter::FillLightComponent(fbx::FbxLight* FbxLight, ULightComponent* UnrealLightComponent)
{
	fbx::FbxDouble3 Color = FbxLight->Color.Get();
	FColor UnrealColor( BYTE(255.0*Color[0]), BYTE(255.0*Color[1]), BYTE(255.0*Color[2]) );
	UnrealLightComponent->LightColor = UnrealColor;

	fbx::FbxDouble Intensity = FbxLight->Intensity.Get();
	UnrealLightComponent->Brightness = (FLOAT)Intensity/100.f;

	UnrealLightComponent->CastShadows = FbxLight->CastShadows.Get();

	switch (FbxLight->LightType.Get())
	{
	// point light properties
	case fbx::FbxLight::ePoint:
		{
			UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(UnrealLightComponent);
			if (PointLightComponent)
			{
				fbx::FbxDouble DecayStart = FbxLight->DecayStart.Get();
				PointLightComponent->Radius = Converter.ConvertDist(DecayStart);

				fbx::FbxLight::EDecayType Decay = FbxLight->DecayType.Get();
				if (Decay == fbx::FbxLight::eNone)
				{
					PointLightComponent->Radius = FBXSDK_FLOAT_MAX;
				}
			}
			else
			{
				warnf(NAME_Error,TEXT("FBX Light type 'Point' does not match unreal light component"));
			}
		}
		break;
	// spot light properties
	case fbx::FbxLight::eSpot:
		{
			USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(UnrealLightComponent);
			if (SpotLightComponent)
			{
				fbx::FbxDouble DecayStart = FbxLight->DecayStart.Get();
				SpotLightComponent->Radius = Converter.ConvertDist(DecayStart);
				fbx::FbxLight::EDecayType Decay = FbxLight->DecayType.Get();
				if (Decay == fbx::FbxLight::eNone)
				{
					SpotLightComponent->Radius = FBXSDK_FLOAT_MAX;
				}
				SpotLightComponent->InnerConeAngle = FbxLight->InnerAngle.Get();
				SpotLightComponent->OuterConeAngle = FbxLight->OuterAngle.Get();
			}
			else
			{
				warnf(NAME_Error,TEXT("FBX Light type 'Spot' does not match unreal light component"));
			}
		}
		break;
	// directional light properties 
	case fbx::FbxLight::eDirectional:
		{
			// nothing specific
		}
		break;
	}

	return TRUE;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
ACameraActor* CFbxImporter::CreateCamera(fbx::FbxCamera* FbxCamera)
{
	ACameraActor* UnrealCamera = NULL;
	FString ActorName = ANSI_TO_TCHAR(MakeName(FbxCamera->GetName()));
	UnrealCamera = Cast<ACameraActor>(GWorld->SpawnActor(ACameraActor::StaticClass(),*ActorName));
	if (UnrealCamera)
	{
		UnrealCamera->FOVAngle = FbxCamera->FieldOfView.Get();
	}
	return UnrealCamera;
}

#endif // WITH_FBX
