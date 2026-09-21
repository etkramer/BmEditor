/*
* Copyright 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Main implementation of FbxExporter : export FBX data from Unreal
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX

#include "UnFbxExporter.h"

namespace UnFbx
{

FbxExporter::FbxExporter()
{
	// Create the SdkManager
	FbxSdkManager = fbx::FbxManager::Create();

	// create an IOSettings object
	fbx::FbxIOSettings * ios = fbx::FbxIOSettings::Create(FbxSdkManager, IOSROOT );
	FbxSdkManager->SetIOSettings(ios);

	FbxCamera = NULL;
}

FbxExporter::~FbxExporter()
{
	if (FbxSdkManager)
	{
		FbxSdkManager->Destroy();
		FbxSdkManager = NULL;
	}
}

FbxExporter* FbxExporter::GetInstance()
{
	static FbxExporter* ExporterInstance = NULL;

	if (ExporterInstance == NULL)
	{
		ExporterInstance = new FbxExporter();
	}
	return ExporterInstance;
}

void FbxExporter::CreateDocument()
{
	FbxScene = fbx::FbxScene::Create(FbxSdkManager,"");
	
	// create scene info
	fbx::FbxDocumentInfo* SceneInfo = fbx::FbxDocumentInfo::Create(FbxSdkManager,"SceneInfo");
	SceneInfo->mTitle = "Unreal Matinee Sequence";
	SceneInfo->mSubject = "Export Unreal Matinee";
	SceneInfo->mComment = "no particular comments required.";

	FbxScene->SetSceneInfo(SceneInfo);
	
	//FbxScene->GetGlobalSettings().SetOriginalUpAxis(fbx::FbxAxisSystem::Max);
	fbx::FbxAxisSystem::EFrontVector FrontVector = (fbx::FbxAxisSystem::EFrontVector)-fbx::FbxAxisSystem::eParityOdd;
	const fbx::FbxAxisSystem UnrealZUp(fbx::FbxAxisSystem::eZAxis, FrontVector, fbx::FbxAxisSystem::eRightHanded);
	FbxScene->GetGlobalSettings().SetAxisSystem(UnrealZUp);
	FbxScene->GetGlobalSettings().SetOriginalUpAxis(UnrealZUp);
	// Maya use cm by default
	FbxScene->GetGlobalSettings().SetSystemUnit(fbx::FbxSystemUnit::cm);
	//FbxScene->GetGlobalSettings().SetOriginalSystemUnit( fbx::FbxSystemUnit::m );
	
	// setup anim stack
	AnimStack = fbx::FbxAnimStack::Create(FbxScene, "Unreal Matinee Take");
	//fbx::FbxSet<fbx::FbxTime>(AnimStack->LocalStart, fbx::FbxTime(FBXSDK_TC_SECOND));
	AnimStack->Description.Set("Animation Take for Unreal Matinee.");

	// this take contains one base layer. In fact having at least one layer is mandatory.
	fbx::FbxAnimLayer* AnimLayer = fbx::FbxAnimLayer::Create(FbxScene, "Base Layer");
	AnimStack->AddMember(AnimLayer);
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(FbxSdkManager->GetIOSettings()))
#endif

void FbxExporter::WriteToFile(const TCHAR* Filename)
{
	INT Major, Minor, Revision;
	UBOOL Status = TRUE;

	INT FileFormat = -1;
	bool bEmbedMedia = false;

	// Create an exporter.
	fbx::FbxExporter* FbxExporter = fbx::FbxExporter::Create(FbxSdkManager, "");

	// set file format
	if( FileFormat < 0 || FileFormat >= FbxSdkManager->GetIOPluginRegistry()->GetWriterFormatCount() )
	{
		// Write in fall back format if pEmbedMedia is true
		FileFormat = FbxSdkManager->GetIOPluginRegistry()->GetNativeWriterFormat();
	}

	// Set the export states. By default, the export states are always set to 
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below 
	// shows how to change these states.

	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL,        true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE,         true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED,        bEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE,           true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO,            true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION,       true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

	// Initialize the exporter by providing a filename.
	if( !FbxExporter->Initialize(TCHAR_TO_ANSI(Filename), FileFormat, FbxSdkManager->GetIOSettings()) )
	{
		warnf(NAME_Log, TEXT("Call to FbxExporter::Initialize() failed.\n"));
		warnf(NAME_Log, TEXT("Error returned: %s\n\n"), ANSI_TO_TCHAR(FbxExporter->GetStatus().GetErrorString()));
		return;
	}

	fbx::FbxManager::GetFileFormatVersion(Major, Minor, Revision);
	warnf(NAME_Log, TEXT("FBX version number for this version of the FBX SDK is %d.%d.%d\n\n"), Major, Minor, Revision);

	// Export the scene.
	Status = FbxExporter->Export(FbxScene); 

	// Destroy the exporter.
	FbxExporter->Destroy();
	
	CloseDocument();
	
	return;
}

/**
 * Release the FBX scene, releasing its memory.
 */
void FbxExporter::CloseDocument()
{
	FbxActors.Reset();
	FbxMaterials.Reset();
	FbxNodeNameToIndexMap.Reset();
	
	if (FbxScene)
	{
		FbxScene->Destroy();
		FbxScene = NULL;
	}
}

void FbxExporter::CreateAnimatableUserProperty(fbx::FbxNode* Node, FLOAT Value, const char* Name, const char* Label)
{
	// Add one user property for recording the animation
	fbx::FbxProperty IntensityProp = fbx::FbxProperty::Create(Node, fbx::FbxFloatDT, Name, Label);
	IntensityProp.Set(Value);
	IntensityProp.ModifyFlag(fbx::FbxPropertyAttr::eUserDefined, true);
	IntensityProp.ModifyFlag(fbx::FbxPropertyAttr::eAnimatable, true);
}

/**
 * Exports the basic scene information to the FBX document.
 */
void FbxExporter::ExportLevelMesh(ULevel* Level, USeqAct_Interp* MatineeSequence )
{
	if (Level == NULL) return;

	// Exports the level's scene geometry
	// the vertex number of Model must be more than 2 (at least a triangle panel)
	if (Level->Model != NULL && Level->Model->VertexBuffer.Vertices.Num() > 2 && Level->Model->MaterialIndexBuffers.Num() > 0)
	{
		// create a fbx::FbxNode
		fbx::FbxNode* Node = fbx::FbxNode::Create(FbxScene,"LevelMesh");

		// set the shading mode to view texture
		Node->SetShadingMode(fbx::FbxNode::eTextureShading);
		Node->LclScaling.Set(fbx::FbxVector4(1.0, 1.0, 1.0));
		
		FbxScene->GetRootNode()->AddChild(Node);

		// Export the mesh for the world
		ExportModel(Level->Model, Node, "Level Mesh");
	}

	// Export all the recognized global actors.
	// Right now, this only includes lights.
	INT ActorCount = GWorld->CurrentLevel->Actors.Num();
	for (INT ActorIndex = 0; ActorIndex < ActorCount; ++ActorIndex)
	{
		AActor* Actor = GWorld->CurrentLevel->Actors(ActorIndex);
		if (Actor != NULL)
		{
			if (Actor->IsA(ALight::StaticClass()))
			{
				ExportLight((ALight*) Actor, MatineeSequence );
			}
			else if (Actor->IsA(AStaticMeshActor::StaticClass()))
			{
				ExportStaticMesh( Actor, ((AStaticMeshActor*) Actor)->StaticMeshComponent, MatineeSequence );
			}
			else if (Actor->IsA(ADynamicSMActor::StaticClass()))
			{
				ExportStaticMesh( Actor, ((ADynamicSMActor*) Actor)->StaticMeshComponent, MatineeSequence );
			}
			else if (Actor->IsA(ABrush::StaticClass()))
			{
				// All brushes should be included within the world geometry exported above.
				ExportBrush((ABrush*) Actor, FALSE);
			}
			else if (Actor->IsA(ATerrain::StaticClass()))
			{
				// ExportTerrain?((ATerrain*) Actor);
			}
			else if (Actor->IsA(AEmitter::StaticClass()))
			{
				ExportActor( Actor, MatineeSequence ); // Just export the placement of the particle emitter.
			}
		}
	}
}

/**
 * Exports the light-specific information for a UE3 light actor.
 */
void FbxExporter::ExportLight( ALight* Actor, USeqAct_Interp* MatineeSequence )
{
	if (FbxScene == NULL || Actor == NULL || Actor->LightComponent == NULL) return;

	// Export the basic actor information.
	fbx::FbxNode* FbxActor = ExportActor( Actor, MatineeSequence ); // this is the pivot node
	// The real fbx light node
	fbx::FbxNode* FbxLightNode = FbxActor->GetParent();

	ULightComponent* BaseLight = Actor->LightComponent;

	FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

	// Export the basic light information
	fbx::FbxLight* Light = fbx::FbxLight::Create(FbxScene, TCHAR_TO_ANSI(*FbxNodeName));
	Light->Intensity.Set(BaseLight->Brightness * 100);
	Light->Color.Set(Converter.ConvertToFbxColor(BaseLight->LightColor));
	
	// Add one user property for recording the Brightness animation
	CreateAnimatableUserProperty(FbxLightNode, BaseLight->Brightness, "UE_Intensity", "UE_Matinee_Light_Intensity");
	
	// Look for the higher-level light types and determine the lighting method
	if (BaseLight->IsA(UPointLightComponent::StaticClass()))
	{
		UPointLightComponent* PointLight = (UPointLightComponent*) BaseLight;
		if (BaseLight->IsA(USpotLightComponent::StaticClass()))
		{
			USpotLightComponent* SpotLight = (USpotLightComponent*) BaseLight;
			Light->LightType.Set(fbx::FbxLight::eSpot);

			// Export the spot light parameters.
			if (!appIsNearlyZero(SpotLight->InnerConeAngle))
			{
				Light->InnerAngle.Set(SpotLight->InnerConeAngle);
			}
			else // Maya requires a non-zero inner cone angle
			{
				Light->InnerAngle.Set(0.01f);
			}
			Light->OuterAngle.Set(SpotLight->OuterConeAngle);
		}
		else
		{
			Light->LightType.Set(fbx::FbxLight::ePoint);
		}
		
		// Export the point light parameters.
		Light->EnableFarAttenuation.Set(true);
		Light->FarAttenuationEnd.Set(PointLight->Radius);
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->Radius, "UE_Radius", "UE_Matinee_Light_Radius");
		
		// Add one user property for recording the FalloffExponent animation
		CreateAnimatableUserProperty(FbxLightNode, PointLight->FalloffExponent, "UE_FalloffExponent", "UE_Matinee_Light_FalloffExponent");
	}
	else if (BaseLight->IsA(UDirectionalLightComponent::StaticClass()))
	{
		// The directional light has no interesting properties.
		Light->LightType.Set(fbx::FbxLight::eDirectional);
	}
	
	FbxActor->SetNodeAttribute(Light);
}

void FbxExporter::ExportCamera( ACameraActor* Actor, USeqAct_Interp* MatineeSequence )
{
	if (FbxScene == NULL || Actor == NULL) return;

	// Export the basic actor information.
	fbx::FbxNode* FbxActor = ExportActor( Actor, MatineeSequence ); // this is the pivot node
	// The real fbx camera node
	fbx::FbxNode* FbxCameraNode = FbxActor->GetParent();

	FString FbxNodeName = GetActorNodeName(Actor, NULL);

	// Create a properly-named FBX camera structure and instantiate it in the FBX scene graph
	fbx::FbxCamera* Camera = fbx::FbxCamera::Create(FbxScene, TCHAR_TO_ANSI(*FbxNodeName));

	// Export the view area information
	Camera->ProjectionType.Set(fbx::FbxCamera::ePerspective);
	Camera->SetAspect(fbx::FbxCamera::eFixedRatio, Actor->AspectRatio, 1.0f);
	Camera->FilmAspectRatio.Set(Actor->AspectRatio);
	Camera->SetApertureWidth(Actor->AspectRatio * 0.612f); // 0.612f is a magic number from Maya that represents the ApertureHeight
	Camera->SetApertureMode(fbx::FbxCamera::eFocalLength);
	Camera->FocalLength.Set(Camera->ComputeFocalLength(Actor->FOVAngle));
	
	// Add one user property for recording the AspectRatio animation
	CreateAnimatableUserProperty(FbxCameraNode, Actor->AspectRatio, "UE_AspectRatio", "UE_Matinee_Camera_AspectRatio");

	// Push the near/far clip planes away, as the UE3 engine uses larger values than the default.
	Camera->SetNearPlane(10.0f);
	Camera->SetFarPlane(100000.0f);

	// Export the post-processing information: only one of depth-of-field or motion blur can be supported in 3dsMax.
	// And Maya only supports some simple depth-of-field effect.
	FPostProcessSettings* PostProcess = &Actor->CamOverridePostProcess;
	if (PostProcess->bEnableDOF)
	{
		// Export the depth-of-field information
		Camera->UseDepthOfField.Set(TRUE);
		
		// 'focal depth' <- 'focus distance'.
		// BM: AK's PostProcessSettings has no focus type/position/inner radius/kernel size
		Camera->FocusSource.Set(fbx::FbxCamera::eFocusSpecificDistance);
		Camera->FocusDistance.Set(PostProcess->DOF_FocusDistance);

		// Add one user property for recording the UE_DOF_FocusDistance animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->DOF_FocusDistance, "UE_DOF_FocusDistance", "UE_Matinee_Camear_DOF_FocusDistance");

		// Add one user property for recording the DOF_ApertureStop animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->DOF_ApertureStop, "UE_DOF_ApertureStop", "UE_Matinee_Camear_DOF_ApertureStop");
	}
	else if (PostProcess->bEnableMotionBlur)
	{
		Camera->UseMotionBlur.Set(TRUE);
		Camera->MotionBlurIntensity.Set(PostProcess->MotionBlur_Amount);
		// Add one user property for recording the MotionBlur_Amount animation
		CreateAnimatableUserProperty(FbxCameraNode, PostProcess->MotionBlur_Amount, "UE_MotionBlur_Amount", "UE_Matinee_Camear_MotionBlur_Amount");
	}
	
	FbxActor->SetNodeAttribute(Camera);

	FbxCamera = Camera;
}

/**
 * Exports the mesh and the actor information for a UE3 brush actor.
 */
void FbxExporter::ExportBrush(ABrush* Actor, UBOOL bConvertToStaticBesh)
{
	if (FbxScene == NULL || Actor == NULL || Actor->BrushComponent == NULL) return;

	if (!bConvertToStaticBesh)
	{
		// Retrieve the information structures, verifying the integrity of the data.
		UModel* Model = Actor->BrushComponent->Brush;

		if (Model == NULL || Model->VertexBuffer.Vertices.Num() < 3 || Model->MaterialIndexBuffers.Num() == 0) return;

		// Create the FBX actor, the FBX geometry and instantiate it.
		fbx::FbxNode* FbxActor = ExportActor( Actor, NULL );
		FbxScene->GetRootNode()->AddChild(FbxActor);

		// Export the mesh information
		ExportModel(Model, FbxActor, TCHAR_TO_ANSI(*Actor->GetName()));
	}
	else
	{
		// Convert the brush to a static mesh then export that static mesh
		TArray<FStaticMeshTriangle>	Triangles;
		TArray<FStaticMeshElement>	Materials;

		GetBrushTriangles( Triangles, Materials, Actor, Actor->Brush );

		// Transform the static meshes triangles into worldspace.
		const FMatrix LocalToWorld = Actor->LocalToWorld();
		FVector Pivot(0,0,0);

		for( INT x = 0 ; x < Triangles.Num() ; ++x )
		{
			FStaticMeshTriangle* Tri = &Triangles(x);
			for( INT y = 0 ; y < 3 ; ++y )
			{
				Tri->Vertices[y] = Actor->LocalToWorld().TransformFVector( Tri->Vertices[y] );
				Tri->Vertices[y] -= Pivot - Actor->PrePivot;
			}
		}

		if( Triangles.Num() )
		{
			UStaticMesh* NewMesh = CreateStaticMesh(Triangles,Materials,UObject::GetTransientPackage(),Actor->GetPureName());

			ExportStaticMesh(NewMesh);
		}
	}
}

void FbxExporter::ExportModel(UModel* Model, fbx::FbxNode* Node, const char* Name)
{
	//INT VertexCount = Model->VertexBuffer.Vertices.Num();
	INT MaterialCount = Model->MaterialIndexBuffers.Num();

	const FLOAT BiasedHalfWorldExtent = HALF_WORLD_MAX * 0.95f;

	// Create the mesh and three data sources for the vertex positions, normals and texture coordinates.
	fbx::FbxMesh* Mesh = fbx::FbxMesh::Create(FbxScene, Name);
	
	// Create control points.
	UINT VertCount(Model->VertexBuffer.Vertices.Num());
	Mesh->InitControlPoints(VertCount);
	fbx::FbxVector4* ControlPoints = Mesh->GetControlPoints();
	
	// Set the normals on Layer 0.
	fbx::FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}
	
	// We want to have one normal for each vertex (or control point),
	// so we set the mapping mode to eByControlPoint.
	fbx::FbxLayerElementNormal* LayerElementNormal= fbx::FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(fbx::FbxLayerElement::eByControlPoint);

	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(fbx::FbxLayerElement::eDirect);
	
	// Create UV for Diffuse channel.
	fbx::FbxLayerElementUV* UVDiffuseLayer = fbx::FbxLayerElementUV::Create(Mesh, "DiffuseUV");
	UVDiffuseLayer->SetMappingMode(fbx::FbxLayerElement::eByControlPoint);
	UVDiffuseLayer->SetReferenceMode(fbx::FbxLayerElement::eDirect);
	Layer->SetUVs(UVDiffuseLayer, fbx::FbxLayerElement::eTextureDiffuse);
	
	for (UINT VertexIdx = 0; VertexIdx < VertCount; ++VertexIdx)
	{
		FModelVertex& Vertex = Model->VertexBuffer.Vertices(VertexIdx);
		FVector Normal = (FVector) Vertex.TangentZ;

		// If the vertex is outside of the world extent, snap it to the origin.  The faces associated with
		// these vertices will be removed before exporting.  We leave the snapped vertex in the buffer so
		// we won't have to deal with reindexing everything.
		FVector FinalVertexPos = Vertex.Position;
		if( Abs( Vertex.Position.X ) > BiasedHalfWorldExtent ||
			Abs( Vertex.Position.Y ) > BiasedHalfWorldExtent ||
			Abs( Vertex.Position.Z ) > BiasedHalfWorldExtent )
		{
			FinalVertexPos = FVector( 0.0f, 0.0f, 0.0f );
		}

		ControlPoints[VertexIdx] = fbx::FbxVector4(FinalVertexPos.X, -FinalVertexPos.Y, FinalVertexPos.Z);
		fbx::FbxVector4 FbxNormal = fbx::FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		fbx::FbxAMatrix NodeMatrix;
		fbx::FbxVector4 Trans = Node->LclTranslation.Get();
		NodeMatrix.SetT(fbx::FbxVector4(Trans[0], Trans[1], Trans[2]));
		fbx::FbxVector4 Rot = Node->LclRotation.Get();
		NodeMatrix.SetR(fbx::FbxVector4(Rot[0], Rot[1], Rot[2]));
		NodeMatrix.SetS(Node->LclScaling.Get());
		FbxNormal = NodeMatrix.MultT(FbxNormal);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
		
		// update the index array of the UVs that map the texture to the face
		UVDiffuseLayer->GetDirectArray().Add(fbx::FbxVector2(Vertex.TexCoord.X, -Vertex.TexCoord.Y));
	}
	
	Layer->SetNormals(LayerElementNormal);
	Layer->SetUVs(UVDiffuseLayer);
	
	fbx::FbxLayerElementMaterial* MatLayer = fbx::FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(fbx::FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(fbx::FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);
	
	// Create the materials and the per-material tesselation structures.
	TMap<UMaterialInterface*,TScopedPointer<FRawIndexBuffer16or32> >::TIterator MaterialIterator(Model->MaterialIndexBuffers);
	for (; MaterialIterator; ++MaterialIterator)
	{
		UMaterialInterface* MaterialInterface = MaterialIterator.Key();
		FRawIndexBuffer16or32& IndexBuffer = *MaterialIterator.Value();
		INT IndexCount = IndexBuffer.Indices.Num();
		if (IndexCount < 3) continue;
		
		// Are NULL materials okay?
		INT MaterialIndex = -1;
		fbx::FbxSurfaceMaterial* FbxMaterial;
		if (MaterialInterface != NULL && MaterialInterface->GetMaterial(MSP_BASE) != NULL)
		{
			FbxMaterial = ExportMaterial(MaterialInterface->GetMaterial(MSP_BASE));
		}
		else
		{
			// Set default material
			FbxMaterial = CreateDefaultMaterial();
		}
		MaterialIndex = Node->AddMaterial(FbxMaterial);

		// Create the Fbx polygons set.

		// Retrieve and fill in the index buffer.
		const INT TriangleCount = IndexCount / 3;
		for( INT TriangleIdx = 0; TriangleIdx < TriangleCount; ++TriangleIdx )
		{
			UBOOL bSkipTriangle = FALSE;

			for( INT IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
			{
				// Skip triangles that belong to BSP geometry close to the world extent, since its probably
				// the automatically-added-brush for new levels.  The vertices will be left in the buffer (unreferenced)
				FVector VertexPos = Model->VertexBuffer.Vertices( IndexBuffer.Indices( TriangleIdx * 3 + IndexIdx ) ).Position;
				if( Abs( VertexPos.X ) > BiasedHalfWorldExtent ||
					Abs( VertexPos.Y ) > BiasedHalfWorldExtent ||
					Abs( VertexPos.Z ) > BiasedHalfWorldExtent )
				{
					bSkipTriangle = TRUE;
					break;
				}
			}

			if( !bSkipTriangle )
			{
				// all faces of the cube have the same texture
				Mesh->BeginPolygon(MaterialIndex);
				for( INT IndexIdx = 0; IndexIdx < 3; ++IndexIdx )
				{
					// Control point index
					Mesh->AddPolygon(IndexBuffer.Indices( TriangleIdx * 3 + IndexIdx ));

				}
				Mesh->EndPolygon ();
			}
		}
	}
	
	Node->SetNodeAttribute(Mesh);
}

void FbxExporter::ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, USeqAct_Interp* MatineeSequence )
{
	if (FbxScene == NULL || Actor == NULL || StaticMeshComponent == NULL) return;

	// Retrieve the static mesh rendering information at the correct LOD level.
	UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;
	if (StaticMesh == NULL || StaticMesh->LODModels.Num() == 0) return;
	INT LODIndex = 0;
	if (LODIndex >= StaticMesh->LODModels.Num())
	{
		LODIndex = StaticMesh->LODModels.Num() - 1;
	}
	FStaticMeshRenderData& RenderMesh = StaticMesh->LODModels(LODIndex);

	FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

	FColorVertexBuffer* ColorBuffer = NULL;
	
	if (LODIndex < StaticMeshComponent->LODData.Num())
	{
		ColorBuffer = StaticMeshComponent->LODData(LODIndex).OverrideVertexColors;
	}

	fbx::FbxNode* FbxActor = ExportActor( Actor, MatineeSequence );
	ExportStaticMeshToFbx(RenderMesh, *FbxNodeName, FbxActor, -1, ColorBuffer);
}

void FbxExporter::ExportStaticMesh( UStaticMesh* StaticMesh )
{
	if (FbxScene == NULL || StaticMesh == NULL) return;
	
	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshRenderData& RenderMesh = StaticMesh->LODModels(0);
	fbx::FbxNode* MeshNode = fbx::FbxNode::Create(FbxScene, TCHAR_TO_ANSI(*MeshName));
	FbxScene->GetRootNode()->AddChild(MeshNode);
	ExportStaticMeshToFbx(RenderMesh, *MeshName, MeshNode);
}

void FbxExporter::ExportStaticMeshLightMap( UStaticMesh* StaticMesh, INT LODIndex, INT UVChannel )
{
	if (FbxScene == NULL || StaticMesh == NULL) return;

	FString MeshName;
	StaticMesh->GetName(MeshName);
	FStaticMeshRenderData& RenderMesh = StaticMesh->LODModels(LODIndex);
	fbx::FbxNode* MeshNode = fbx::FbxNode::Create(FbxScene, TCHAR_TO_ANSI(*MeshName));
	FbxScene->GetRootNode()->AddChild(MeshNode);
	ExportStaticMeshToFbx(RenderMesh, *MeshName, MeshNode, UVChannel);
}

fbx::FbxSurfaceMaterial* FbxExporter::CreateDefaultMaterial()
{
	fbx::FbxSurfaceMaterial* FbxMaterial = FbxScene->GetMaterial("Fbx Default Material");
	
	if (!FbxMaterial)
	{
		FbxMaterial = fbx::FbxSurfaceLambert::Create(FbxScene, "Fbx Default Material");
		((fbx::FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(fbx::FbxDouble3(0.72, 0.72, 0.72));
	}
	
	return FbxMaterial;
}

fbx::FbxDouble3 SetMaterialComponent(FColorMaterialInput& MatInput)
{
	FColor FinalColor;
	
	if (MatInput.Expression)
	{
		if (Cast<UMaterialExpressionConstant>(MatInput.Expression))
		{
			UMaterialExpressionConstant* Expr = Cast<UMaterialExpressionConstant>(MatInput.Expression);
			FinalColor = FColor(Expr->R);
		}
		else if (Cast<UMaterialExpressionVectorParameter>(MatInput.Expression))
		{
			UMaterialExpressionVectorParameter* Expr = Cast<UMaterialExpressionVectorParameter>(MatInput.Expression);
			FinalColor = Expr->DefaultValue;
		}
		else if (Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant3Vector* Expr = Cast<UMaterialExpressionConstant3Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = Expr->B;
		}
		else if (Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant4Vector* Expr = Cast<UMaterialExpressionConstant4Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = Expr->B;
			//FinalColor.A = Expr->A;
		}
		else if (Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression))
		{
			UMaterialExpressionConstant2Vector* Expr = Cast<UMaterialExpressionConstant2Vector>(MatInput.Expression);
			FinalColor.R = Expr->R;
			FinalColor.G = Expr->G;
			FinalColor.B = 0;
		}
		else
		{
			FinalColor.R = MatInput.Constant.R / 128.0;
			FinalColor.G = MatInput.Constant.G / 128.0;
			FinalColor.B = MatInput.Constant.B / 128.0;
		}
	}
	else
	{
		FinalColor.R = MatInput.Constant.R / 128.0;
		FinalColor.G = MatInput.Constant.G / 128.0;
		FinalColor.B = MatInput.Constant.B / 128.0;
	}
	
	return fbx::FbxDouble3(FinalColor.R, FinalColor.G, FinalColor.B);
}

/**
* Exports the profile_COMMON information for a UE3 material.
*/
fbx::FbxSurfaceMaterial* FbxExporter::ExportMaterial(UMaterial* Material)
{
	if (FbxScene == NULL || Material == NULL) return NULL;
	
	// Verify that this material has not already been exported:
	if (FbxMaterials.Find(Material))
	{
		return *FbxMaterials.Find(Material);
	}

	// Create the Fbx material
	fbx::FbxSurfaceMaterial* FbxMaterial = NULL;
	
	// Set the lighting model
	if (Material->LightingModel == MLM_Phong || Material->LightingModel == MLM_Custom || Material->LightingModel == MLM_SHPRT)
	{
		FbxMaterial = fbx::FbxSurfacePhong::Create(FbxScene, TCHAR_TO_ANSI(*Material->GetName()));
		((fbx::FbxSurfacePhong*)FbxMaterial)->Specular.Set(SetMaterialComponent(Material->SpecularColor));
		((fbx::FbxSurfacePhong*)FbxMaterial)->Shininess.Set(Material->SpecularPower.Constant);
	}
	else if (Material->LightingModel == MLM_NonDirectional)
	{
		FbxMaterial = fbx::FbxSurfaceLambert::Create(FbxScene, TCHAR_TO_ANSI(*Material->GetName()));
	}
	else // if (Material->LightingModel == MLM_Unlit)
	{
		FbxMaterial = fbx::FbxSurfaceLambert::Create(FbxScene, TCHAR_TO_ANSI(*Material->GetName()));
	}
	
	((fbx::FbxSurfaceLambert*)FbxMaterial)->Emissive.Set(SetMaterialComponent(Material->EmissiveColor));
	((fbx::FbxSurfaceLambert*)FbxMaterial)->Diffuse.Set(SetMaterialComponent(Material->DiffuseColor));
	((fbx::FbxSurfaceLambert*)FbxMaterial)->TransparencyFactor.Set(Material->Opacity.Constant);

	// Fill in the profile_COMMON effect with the UE3 material information.
	// TODO: Look for textures/constants in the Material expressions...
	
	FbxMaterials.Set(Material, FbxMaterial);
	
	return FbxMaterial;
}


/**
 * Exports the given Matinee sequence information into a FBX document.
 */
void FbxExporter::ExportMatinee(USeqAct_Interp* MatineeSequence)
{
	if (MatineeSequence == NULL || FbxScene == NULL) return;

	// If the Matinee editor is not open, we need to initialize the sequence.
	UBOOL InitializeMatinee = MatineeSequence->InterpData == NULL;
	if (InitializeMatinee)
	{
		MatineeSequence->InitInterp();
	}

	// Iterate over the Matinee data groups and export the known tracks
	INT GroupCount = MatineeSequence->GroupInst.Num();
	for (INT GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		UInterpGroupInst* Group = MatineeSequence->GroupInst(GroupIndex);
		AActor* Actor = Group->GetGroupActor();
		if (Group->Group == NULL || Actor == NULL) continue;

		// Look for the class-type of the actor.
		if (Actor->IsA(ACameraActor::StaticClass()))
		{
			ExportCamera( (ACameraActor*) Actor, MatineeSequence );
		}

		fbx::FbxNode* FbxActor = ExportActor( Actor, MatineeSequence );

		// Look for the tracks that we currently support
		INT TrackCount = Min(Group->TrackInst.Num(), Group->Group->InterpTracks.Num());
		for (INT TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
		{
			UInterpTrackInst* TrackInst = Group->TrackInst(TrackIndex);
			UInterpTrack* Track = Group->Group->InterpTracks(TrackIndex);
			if (TrackInst->IsA(UInterpTrackInstMove::StaticClass()) && Track->IsA(UInterpTrackMove::StaticClass()))
			{
				UInterpTrackInstMove* MoveTrackInst = (UInterpTrackInstMove*) TrackInst;
				UInterpTrackMove* MoveTrack = (UInterpTrackMove*) Track;
				ExportMatineeTrackMove(FbxActor, MoveTrackInst, MoveTrack, MatineeSequence->InterpData->InterpLength);
			}
			else if (TrackInst->IsA(UInterpTrackInstFloatProp::StaticClass()) && Track->IsA(UInterpTrackFloatProp::StaticClass()))
			{
				UInterpTrackInstFloatProp* PropertyTrackInst = (UInterpTrackInstFloatProp*) TrackInst;
				UInterpTrackFloatProp* PropertyTrack = (UInterpTrackFloatProp*) Track;
				ExportMatineeTrackFloatProp(FbxActor, PropertyTrack);
			}
		}
	}

	if (InitializeMatinee)
	{
		MatineeSequence->TermInterp();
	}

	FbxCamera = NULL;
}


/**
 * Exports a scene node with the placement indicated by a given UE3 actor.
 * This scene node will always have two transformations: one translation vector and one Euler rotation.
 */
fbx::FbxNode* FbxExporter::ExportActor(AActor* Actor, USeqAct_Interp* MatineeSequence )
{
	// Verify that this actor isn't already exported, create a structure for it
	// and buffer it.
	fbx::FbxNode* ActorNode = FindActor(Actor);
	if (ActorNode == NULL)
	{
		FString FbxNodeName = GetActorNodeName(Actor, MatineeSequence);

		// See if a node with this name was already found
		// if so add and increment the number on the end of it
		// this seems to be what Collada does internally
		INT *NodeIndex = FbxNodeNameToIndexMap.Find( FbxNodeName );
		if( NodeIndex )
		{
			FbxNodeName = FString::Printf( TEXT("%s%d"), *FbxNodeName, *NodeIndex );
			++(*NodeIndex);
		}
		else
		{
			FbxNodeNameToIndexMap.Set( FbxNodeName, 1 );	
		}

		ActorNode = fbx::FbxNode::Create(FbxScene, TCHAR_TO_ANSI(*FbxNodeName));
		FbxScene->GetRootNode()->AddChild(ActorNode);

		FbxActors.Set(Actor, ActorNode);

		// Set the default position of the actor on the transforms
		// The UE3 transformation is different from FBX's Z-up: invert the Y-axis for translations and the Y/Z angle values in rotations.
		ActorNode->LclTranslation.Set(Converter.ConvertToFbxPos(Actor->Location));
		ActorNode->LclRotation.Set(Converter.ConvertToFbxRot(Actor->Rotation.Euler()));
		ActorNode->LclScaling.Set(Converter.ConvertToFbxScale(Actor->DrawScale * Actor->DrawScale3D));
	
		// For cameras and lights: always add a Y-pivot rotation to get the correct coordinate system.
		if (Actor->IsA(ACameraActor::StaticClass()) || Actor->IsA(ALight::StaticClass()))
		{
			FString FbxPivotNodeName = GetActorNodeName(Actor, NULL);

			if (FbxPivotNodeName == FbxNodeName)
			{
				FbxPivotNodeName += ANSI_TO_TCHAR("_pivot");
			}

			fbx::FbxNode* PivotNode = fbx::FbxNode::Create(FbxScene, TCHAR_TO_ANSI(*FbxPivotNodeName));
			PivotNode->LclRotation.Set(fbx::FbxVector4(90, 0, -90));

			if (Actor->IsA(ACameraActor::StaticClass()))
			{
				PivotNode->SetPostRotation(fbx::FbxNode::eSourcePivot, fbx::FbxVector4(0, -90, 0));
			}
			else if (Actor->IsA(ALight::StaticClass()))
			{
				PivotNode->SetPostRotation(fbx::FbxNode::eSourcePivot, fbx::FbxVector4(-90, 0, 0));
			}
			ActorNode->AddChild(PivotNode);

			ActorNode = PivotNode;
		}
	}

	return ActorNode;
}

/**
 * Exports the Matinee movement track into the FBX animation library.
 */
void FbxExporter::ExportMatineeTrackMove(fbx::FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, FLOAT InterpLength)
{
	if (FbxActor == NULL || MoveTrack == NULL) return;
	
	// For the Y and Z angular rotations, we need to invert the relative animation frames,
	// While keeping the standard angles constant.

	if (MoveTrack != NULL)
	{
		fbx::FbxAnimLayer* BaseLayer = (fbx::FbxAnimLayer*)AnimStack->GetMember<fbx::FbxAnimLayer>(0);
		fbx::FbxAnimCurve* Curve;

		UBOOL bPosCurve = TRUE;
		if( MoveTrack->SubTracks.Num() == 0 )
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, FALSE, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = FALSE;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportAnimatedVector(Curve, "X", MoveTrack, MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportAnimatedVector(Curve, "Y", MoveTrack, MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportAnimatedVector(Curve, "Z", MoveTrack, MoveTrackInst, bPosCurve, 2, TRUE, InterpLength);
		}
		else
		{
			// Translation;
			FbxActor->LclTranslation.GetCurveNode(BaseLayer, true);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(0)), MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(1)), MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclTranslation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(2)), MoveTrackInst, bPosCurve, 2, FALSE, InterpLength);

			// Rotation
			FbxActor->LclRotation.GetCurveNode(BaseLayer, true);
			bPosCurve = FALSE;

			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			ExportMoveSubTrack(Curve, "X", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(3)), MoveTrackInst, bPosCurve, 0, FALSE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			ExportMoveSubTrack(Curve, "Y", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(4)), MoveTrackInst, bPosCurve, 1, TRUE, InterpLength);
			Curve = FbxActor->LclRotation.GetCurve(BaseLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);
			ExportMoveSubTrack(Curve, "Z", CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks(5)), MoveTrackInst, bPosCurve, 2, TRUE, InterpLength);
		}
	}
}

/**
 * Exports the Matinee float property track into the FBX animation library.
 */
void FbxExporter::ExportMatineeTrackFloatProp(fbx::FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack)
{
	if (FbxActor == NULL || PropTrack == NULL) return;
	
	fbx::FbxNodeAttribute* FbxNodeAttr = NULL;
	// camera and light is appended on the fbx pivot node
	if( FbxActor->GetChild(0) )
	{
		FbxNodeAttr = ((fbx::FbxNode*)FbxActor->GetChild(0))->GetNodeAttribute();

		if (FbxNodeAttr == NULL) return;
	}
	
	fbx::FbxProperty FbxProperty;
	FString PropertyName = PropTrack->PropertyName.GetNameString();
	UBOOL IsFoV = FALSE;
	// most properties are created as user property, only FOV of camera in FBX supports animation
	if (PropertyName == "Brightness")
	{
		FbxProperty = FbxActor->FindProperty("UE_Intensity", false);
	}
	else if (PropertyName == "FalloffExponent")
	{
		FbxProperty = FbxActor->FindProperty("UE_FalloffExponent", false);
	}
	else if (PropertyName == "Radius")
	{
		FbxProperty = FbxActor->FindProperty("UE_Radius", false);
	}
	else if (PropertyName == "FOVAngle" && FbxNodeAttr )
	{
		FbxProperty = ((fbx::FbxCamera*)FbxNodeAttr)->FocalLength;
		IsFoV = TRUE;
	}
	else if (PropertyName == "AspectRatio")
	{
		FbxProperty = FbxActor->FindProperty("UE_AspectRatio", false);
	}
	else if (PropertyName == "DOF_FocusDistance")
	{
		FbxProperty = FbxActor->FindProperty("UE_DOF_FocusDistance", false);
	}
	else if (PropertyName == "DOF_ApertureStop")
	{
		FbxProperty = FbxActor->FindProperty("UE_DOF_ApertureStop", false);
	}
	else if (PropertyName == "MotionBlur_Amount")
	{
		FbxProperty = FbxActor->FindProperty("UE_MotionBlur_Amount", false);
	}

	if (FbxProperty != NULL)
	{
		ExportAnimatedFloat(&FbxProperty, &PropTrack->FloatTrack, IsFoV);
	}
}

void ConvertInterpToFBX(BYTE UnrealInterpMode, fbx::FbxAnimCurveDef::EInterpolationType& Interpolation, fbx::FbxAnimCurveDef::ETangentMode& Tangent)
{
	switch(UnrealInterpMode)
	{
	case CIM_Linear:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationLinear;
		Tangent = fbx::FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveAuto:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationCubic;
		Tangent = fbx::FbxAnimCurveDef::eTangentAuto;
		break;
	case CIM_Constant:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationConstant;
		Tangent = (fbx::FbxAnimCurveDef::ETangentMode)fbx::FbxAnimCurveDef::eConstantStandard;
		break;
	case CIM_CurveUser:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationCubic;
		Tangent = fbx::FbxAnimCurveDef::eTangentUser;
		break;
	case CIM_CurveBreak:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (fbx::FbxAnimCurveDef::ETangentMode) fbx::FbxAnimCurveDef::eTangentBreak;
		break;
	case CIM_CurveAutoClamped:
		Interpolation = fbx::FbxAnimCurveDef::eInterpolationCubic;
		Tangent = (fbx::FbxAnimCurveDef::ETangentMode) (fbx::FbxAnimCurveDef::eTangentAuto | fbx::FbxAnimCurveDef::eTangentGenericClamp);
		break;
	case CIM_Unknown:  // ???
		fbx::FbxAnimCurveDef::EInterpolationType Interpolation = fbx::FbxAnimCurveDef::eInterpolationConstant;
		fbx::FbxAnimCurveDef::ETangentMode Tangent = fbx::FbxAnimCurveDef::eTangentAuto;
		break;
	}
}


// float-float comparison that allows for a certain error in the floating point values
// due to floating-point operations never being exact.
static bool IsEquivalent(FLOAT a, FLOAT b, FLOAT Tolerance = KINDA_SMALL_NUMBER)
{
	return (a - b) > -Tolerance && (a - b) < Tolerance;
}

// Set the default FPS to 30 because the SetupMatinee MEL script sets up Maya this way.
const FLOAT FbxExporter::BakeTransformsFPS = 30;

/**
 * Exports a given interpolation curve into the FBX animation curve.
 */
void FbxExporter::ExportAnimatedVector(fbx::FbxAnimCurve* FbxCurve, const char* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength)
{
	if (FbxScene == NULL) return;
	
	FInterpCurveVector* Curve = bPosCurve ? &MoveTrack->PosTrack : &MoveTrack->EulerTrack;

	if (Curve == NULL || CurveIndex >= 3) return;

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	INT KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the UE3 curve to the FBX curve.
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Curve->Points(KeyIndex).InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			// If we are baking trnasforms, ask the movement track what are transforms are at the given time.
			if( bBakeKeys )
			{
				MoveTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes(KeyIndex), KeyPosition, KeyRotation);
			}
			// Else, this information is already present in the position and rotation tracks stored on the movement track.
			else
			{
				KeyPosition = MoveTrack->PosTrack.Points(KeyIndex).OutVal;
				KeyRotation = FRotator( FQuat::MakeFromEuler(MoveTrack->EulerTrack.Points(KeyIndex).OutVal) );
			}

			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			MoveTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		FLOAT KeyTime = KeyTimes(KeyIndex);
		FLOAT OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		FLOAT FbxKeyValue = bNegative ? -OutValue : OutValue;
		
		// Add a new key to the FBX curve
		fbx::FbxTime Time;
		fbx::FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);
		

		fbx::FbxAnimCurveDef::EInterpolationType Interpolation = fbx::FbxAnimCurveDef::eInterpolationConstant;
		fbx::FbxAnimCurveDef::ETangentMode Tangent = fbx::FbxAnimCurveDef::eTangentAuto;
		
		if( !bBakeKeys )
		{
			ConvertInterpToFBX(Curve->Points(KeyIndex).InterpMode, Interpolation, Tangent);
		}

		if (bBakeKeys || Interpolation != fbx::FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			FInterpCurvePoint<FVector>& Key = Curve->Points(KeyIndex);

			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( (Interpolation == fbx::FbxAnimCurveDef::eInterpolationCubic) )
			{
				FLOAT OutTangentValue = (CurveIndex == 0) ? Key.LeaveTangent.X : (CurveIndex == 1) ? Key.LeaveTangent.Y : Key.LeaveTangent.Z;
				FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				FLOAT OutTangentY = OutTangentValue / 3.0f;
				FLOAT RightTangent =  OutTangentY / OutTangentX ;
				
				FLOAT NextLeftTangent = 0;
				
				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FVector>& NextKey = Curve->Points(KeyIndex + 1);
					FLOAT NextInTangentValue = (CurveIndex == 0) ? NextKey.ArriveTangent.X : (CurveIndex == 1) ? NextKey.ArriveTangent.Y : NextKey.ArriveTangent.Z;
					FLOAT NextInTangentX;
					NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
					FLOAT NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FbxExporter::ExportMoveSubTrack(fbx::FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, UBOOL bPosCurve, INT CurveIndex, UBOOL bNegative, FLOAT InterpLength)
{
	if (FbxScene == NULL || FbxCurve == NULL) return;

	FInterpCurveFloat* Curve = &SubTrack->FloatTrack;
	UInterpTrackMove* ParentTrack = CastChecked<UInterpTrackMove>( SubTrack->GetOuter() );

#define FLT_TOLERANCE 0.000001

	// Determine how many key frames we are exporting. If the user wants to export a key every 
	// frame, calculate this number. Otherwise, use the number of keys the user created. 
	INT KeyCount = bBakeKeys ? (InterpLength * BakeTransformsFPS) + Curve->Points.Num() : Curve->Points.Num();

	// Write out the key times from the UE3 curve to the FBX curve.
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = bBakeKeys ? (KeyIndex * InterpLength) / KeyCount : Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		// First, convert the output value to the correct coordinate system, if we need that.  For movement
		// track keys that are in a local coordinate system (IMF_RelativeToInitial), we need to transform
		// the keys to world space first
		FVector FinalOutVec;
		{
			FVector KeyPosition;
			FRotator KeyRotation;

			ParentTrack->GetKeyTransformAtTime(MoveTrackInst, KeyTimes(KeyIndex), KeyPosition, KeyRotation);
		
			FVector WorldSpacePos;
			FRotator WorldSpaceRotator;
			ParentTrack->ComputeWorldSpaceKeyTransform(
				MoveTrackInst,
				KeyPosition,
				KeyRotation,
				WorldSpacePos,			// Out
				WorldSpaceRotator );	// Out

			if( bPosCurve )
			{
				FinalOutVec = WorldSpacePos;
			}
			else
			{
				FinalOutVec = WorldSpaceRotator.Euler();
			}
		}

		FLOAT KeyTime = KeyTimes(KeyIndex);
		FLOAT OutValue = (CurveIndex == 0) ? FinalOutVec.X : (CurveIndex == 1) ? FinalOutVec.Y : FinalOutVec.Z;
		FLOAT FbxKeyValue = bNegative ? -OutValue : OutValue;

		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// Add a new key to the FBX curve
		fbx::FbxTime Time;
		fbx::FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);

		fbx::FbxAnimCurveDef::EInterpolationType Interpolation = fbx::FbxAnimCurveDef::eInterpolationConstant;
		fbx::FbxAnimCurveDef::ETangentMode Tangent = fbx::FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);

		if (bBakeKeys || Interpolation != fbx::FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves. Avoid this for keys created from baking 
			// transforms since there is no tangent info created for these types of keys. 
			if( (Interpolation == fbx::FbxAnimCurveDef::eInterpolationCubic) )
			{
				FLOAT OutTangentValue = Key.LeaveTangent;
				FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
				if (IsEquivalent(OutTangentX, KeyTime))
				{
					OutTangentX = 0.00333f; // 1/3rd of a millisecond.
				}
				FLOAT OutTangentY = OutTangentValue / 3.0f;
				FLOAT RightTangent =  OutTangentY / OutTangentX ;

				FLOAT NextLeftTangent = 0;

				if (KeyIndex < KeyCount - 1)
				{
					FInterpCurvePoint<FLOAT>& NextKey = Curve->Points(KeyIndex + 1);
					FLOAT NextInTangentValue =  Key.LeaveTangent;
					FLOAT NextInTangentX;
					NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
					FLOAT NextInTangentY = NextInTangentValue / 3.0f;
					NextLeftTangent =  NextInTangentY / NextInTangentX ;
				}

				FbxCurve->KeySet(FbxKeyIndex, Time, (float)FbxKeyValue, Interpolation, Tangent, RightTangent, NextLeftTangent );
			}
		}
	}
	FbxCurve->KeyModifyEnd();
}

void FbxExporter::ExportAnimatedFloat(fbx::FbxProperty* FbxProperty, FInterpCurveFloat* Curve, UBOOL IsCameraFoV)
{
	if (FbxProperty == NULL || Curve == NULL) return;

	// do not export an empty anim curve
	if (Curve->Points.Num() == 0) return;

	fbx::FbxAnimCurve* FbxCurve = fbx::FbxAnimCurve::Create(FbxScene, "");
	fbx::FbxAnimCurveNode* CurveNode = FbxProperty->GetCurveNode(true);
	if (!CurveNode)
	{
		return;
	}
	CurveNode->SetChannelValue<double>(0U, Curve->Points(0).OutVal);
	CurveNode->ConnectToChannel(FbxCurve, 0U);

	// Write out the key times from the UE3 curve to the FBX curve.
	INT KeyCount = Curve->Points.Num();
	TArray<FLOAT> KeyTimes;
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);

		// The Unreal engine allows you to place more than one key at one time value:
		// displace the extra keys. This assumes that Unreal's keys are always ordered.
		FLOAT KeyTime = Key.InVal;
		if (KeyTimes.Num() && KeyTime < KeyTimes(KeyIndex-1) + FLT_TOLERANCE)
		{
			KeyTime = KeyTimes(KeyIndex-1) + 0.01f; // Add 1 millisecond to the timing of this key.
		}
		KeyTimes.AddItem(KeyTime);
	}

	// Write out the key values from the UE3 curve to the FBX curve.
	FbxCurve->KeyModifyBegin();
	for (INT KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
	{
		FInterpCurvePoint<FLOAT>& Key = Curve->Points(KeyIndex);
		FLOAT KeyTime = KeyTimes(KeyIndex);
		
		// Add a new key to the FBX curve
		fbx::FbxTime Time;
		fbx::FbxAnimCurveKey FbxKey;
		Time.SetSecondDouble((float)KeyTime);
		int FbxKeyIndex = FbxCurve->KeyAdd(Time);
		float OutVal = (IsCameraFoV && FbxCamera)? FbxCamera->ComputeFocalLength(Key.OutVal): (float)Key.OutVal;

		fbx::FbxAnimCurveDef::EInterpolationType Interpolation = fbx::FbxAnimCurveDef::eInterpolationConstant;
		fbx::FbxAnimCurveDef::ETangentMode Tangent = fbx::FbxAnimCurveDef::eTangentAuto;
		ConvertInterpToFBX(Key.InterpMode, Interpolation, Tangent);
		
		if (Interpolation != fbx::FbxAnimCurveDef::eInterpolationCubic)
		{
			FbxCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent);
		}
		else
		{
			// Setup tangents for bezier curves.
			FLOAT OutTangentX = (KeyIndex < KeyCount - 1) ? (KeyTimes(KeyIndex + 1) - KeyTime) / 3.0f : 0.333f;
			FLOAT OutTangentY = Key.LeaveTangent / 3.0f;
			FLOAT RightTangent =  OutTangentY / OutTangentX ;

			FLOAT NextLeftTangent = 0;

			if (KeyIndex < KeyCount - 1)
			{
				FInterpCurvePoint<FLOAT>& NextKey = Curve->Points(KeyIndex + 1);
				FLOAT NextInTangentX;
				NextInTangentX = (KeyTimes(KeyIndex + 1) - KeyTimes(KeyIndex)) / 3.0f;
				FLOAT NextInTangentY = NextKey.ArriveTangent / 3.0f;
				NextLeftTangent =  NextInTangentY / NextInTangentX ;
			}

			FbxCurve->KeySet(FbxKeyIndex, Time, OutVal, Interpolation, Tangent, RightTangent, NextLeftTangent );

		}
	}
	FbxCurve->KeyModifyEnd();
}

/**
 * Finds the given UE3 actor in the already-exported list of structures
 */
fbx::FbxNode* FbxExporter::FindActor(AActor* Actor)
{
	if (FbxActors.Find(Actor))
	{
		return *FbxActors.Find(Actor);
	}
	else
	{
		return NULL;
	}
}

/*
 * Exports the given static rendering mesh into a FBX geometry.
 */
fbx::FbxNode* FbxExporter::ExportStaticMeshToFbx(FStaticMeshRenderData& RenderMesh, const TCHAR* MeshName, fbx::FbxNode* FbxActor, INT LightmapUVChannel, FColorVertexBuffer* ColorBuffer)
{
	// Verify the integrity of the static mesh.
	if (RenderMesh.VertexBuffer.GetNumVertices() == 0) return NULL;
	if (RenderMesh.Elements.Num() == 0) return NULL;

	fbx::FbxMesh* Mesh = fbx::FbxMesh::Create(FbxScene, TCHAR_TO_ANSI(MeshName));

	// Create and fill in the vertex position data source.
	// The position vertices are duplicated, for some reason, retrieve only the first half vertices.
	const INT VertexCount = RenderMesh.VertexBuffer.GetNumVertices();
	
	Mesh->InitControlPoints(VertexCount); //TmpControlPoints.Num());
	fbx::FbxVector4* ControlPoints = Mesh->GetControlPoints();
	for (INT PosIndex = 0; PosIndex < VertexCount; ++PosIndex)
	{
		FVector Position = RenderMesh.PositionVertexBuffer.VertexPosition(PosIndex); //TmpControlPoints(PosIndex);
		ControlPoints[PosIndex] = fbx::FbxVector4(Position.X, -Position.Y, Position.Z);
	}
	
	// Set the normals on Layer 0.
	fbx::FbxLayer* Layer = Mesh->GetLayer(0);
	if (Layer == NULL)
	{
		Mesh->CreateLayer();
		Layer = Mesh->GetLayer(0);
	}

	// Create and fill in the per-face-vertex normal data source.
	// We extract the Z-tangent and drop the X/Y-tangents which are also stored in the render mesh.
	fbx::FbxLayerElementNormal* LayerElementNormal= fbx::FbxLayerElementNormal::Create(Mesh, "");

	LayerElementNormal->SetMappingMode(fbx::FbxLayerElement::eByControlPoint);
	// Set the normal values for every control point.
	LayerElementNormal->SetReferenceMode(fbx::FbxLayerElement::eDirect);
	for (INT NormalIndex = 0; NormalIndex < VertexCount; ++NormalIndex)
	{
		FVector Normal = (FVector) (RenderMesh.VertexBuffer.VertexTangentZ(NormalIndex));
		fbx::FbxVector4 FbxNormal = fbx::FbxVector4(Normal.X, -Normal.Y, Normal.Z);
		fbx::FbxAMatrix NodeMatrix;
		fbx::FbxVector4 Trans = FbxActor->LclTranslation.Get();
		NodeMatrix.SetT(fbx::FbxVector4(Trans[0], Trans[1], Trans[2]));
		fbx::FbxVector4 Rot = FbxActor->LclRotation.Get();
		NodeMatrix.SetR(fbx::FbxVector4(Rot[0], Rot[1], Rot[2]));
		NodeMatrix.SetS(FbxActor->LclScaling.Get());
		FbxNormal = NodeMatrix.MultT(FbxNormal);
		FbxNormal.Normalize();
		LayerElementNormal->GetDirectArray().Add(FbxNormal);
	}
	Layer->SetNormals(LayerElementNormal);

	// Create and fill in the per-face-vertex texture coordinate data source(s).
	// Create UV for Diffuse channel.
	INT TexCoordSourceCount = (LightmapUVChannel == -1)? RenderMesh.VertexBuffer.GetNumTexCoords(): LightmapUVChannel + 1;
	INT TexCoordSourceIndex = (LightmapUVChannel == -1)? 0: LightmapUVChannel;
	TCHAR UVChannelName[32];
	for (; TexCoordSourceIndex < TexCoordSourceCount; ++TexCoordSourceIndex)
	{
		fbx::FbxLayer* Layer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		if (Layer == NULL)
		{
			Mesh->CreateLayer();
			Layer = (LightmapUVChannel == -1)? Mesh->GetLayer(TexCoordSourceIndex): Mesh->GetLayer(0);
		}

		if ((LightmapUVChannel >= 0) || ((LightmapUVChannel == -1) && (TexCoordSourceIndex == 1)))
		{
			appSprintf(UVChannelName, TEXT("LightMapUV"));
		}
		else
		{
			appSprintf(UVChannelName, TEXT("DiffuseUV"));
		}			
		
		fbx::FbxLayerElementUV* UVDiffuseLayer = fbx::FbxLayerElementUV::Create(Mesh, TCHAR_TO_ANSI(UVChannelName));
		UVDiffuseLayer->SetMappingMode(fbx::FbxLayerElement::eByControlPoint);
		UVDiffuseLayer->SetReferenceMode(fbx::FbxLayerElement::eDirect);
		
		// Create the texture coordinate data source.
		for (INT TexCoordIndex = 0; TexCoordIndex < VertexCount; ++TexCoordIndex)
		{
			const FVector2D& TexCoord = RenderMesh.VertexBuffer.GetVertexUV(TexCoordIndex, TexCoordSourceIndex);
			UVDiffuseLayer->GetDirectArray().Add(fbx::FbxVector2(TexCoord.X, -TexCoord.Y + 1.0));
		}
		
		Layer->SetUVs(UVDiffuseLayer, fbx::FbxLayerElement::eTextureDiffuse);
	}
	
	fbx::FbxLayerElementMaterial* MatLayer = fbx::FbxLayerElementMaterial::Create(Mesh, "");
	MatLayer->SetMappingMode(fbx::FbxLayerElement::eByPolygon);
	MatLayer->SetReferenceMode(fbx::FbxLayerElement::eIndexToDirect);
	Layer->SetMaterials(MatLayer);
	
	// Create the per-material polygons sets.
	INT PolygonsCount = RenderMesh.Elements.Num();
	for (INT PolygonsIndex = 0; PolygonsIndex < PolygonsCount; ++PolygonsIndex)
	{
		FStaticMeshElement& Polygons = RenderMesh.Elements(PolygonsIndex);

		fbx::FbxSurfaceMaterial* FbxMaterial = Polygons.Material ? ExportMaterial(Polygons.Material->GetMaterial(MSP_BASE)) : NULL;
		if (!FbxMaterial)
		{
			FbxMaterial = CreateDefaultMaterial();
		}
		INT MatIndex = FbxActor->AddMaterial(FbxMaterial);
		
		// Static meshes contain one triangle list per element.
		// [GLAFORTE] Could it occasionally contain triangle strips? How do I know?
		INT TriangleCount = Polygons.NumTriangles;
		
		// Copy over the index buffer into the FBX polygons set.
		for (INT TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			Mesh->BeginPolygon(MatIndex);
			for (INT PointIndex = 0; PointIndex < 3; PointIndex++)
			{
				//Mesh->AddPolygon(ControlPointMap(RenderMesh.IndexBuffer.Indices(Polygons.FirstIndex + IndexIndex*3 + PointIndex)));
				Mesh->AddPolygon(RenderMesh.IndexBuffer.Indices(Polygons.FirstIndex + ((TriangleIndex * 3) + PointIndex)));
			}
			Mesh->EndPolygon();
		}
	}

	// Create and fill in the smoothing data source.
	fbx::FbxLayerElementSmoothing* SmoothingInfo = fbx::FbxLayerElementSmoothing::Create(Mesh, "");
	SmoothingInfo->SetMappingMode(fbx::FbxLayerElement::eByPolygon);
	SmoothingInfo->SetReferenceMode(fbx::FbxLayerElement::eDirect);
	fbx::FbxLayerElementArrayTemplate<int>& SmoothingArray = SmoothingInfo->GetDirectArray();
	Layer->SetSmoothing(SmoothingInfo);

	INT TriangleCount = RenderMesh.RawTriangles.GetElementCount();
	FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*)RenderMesh.RawTriangles.Lock(LOCK_READ_ONLY);
	for( INT TriangleIndex = 0 ; TriangleIndex < TriangleCount ; TriangleIndex++ )
	{
		FStaticMeshTriangle* Triangle = (RawTriangleData++);
		
		SmoothingArray.Add(Triangle->SmoothingMask);
	}
	RenderMesh.RawTriangles.Unlock();

	// Create and fill in the vertex color data source.
	FColorVertexBuffer* ColorBufferToUse = ColorBuffer? ColorBuffer: &RenderMesh.ColorVertexBuffer;
	INT ColorVertexCount = ColorBufferToUse->GetNumVertices();
	
	// Only export vertex colors if they exist
	if (ColorVertexCount > 0)
	{
		fbx::FbxLayerElementVertexColor* VertexColor = fbx::FbxLayerElementVertexColor::Create(Mesh, "");
		VertexColor->SetMappingMode(fbx::FbxLayerElement::eByControlPoint);
		VertexColor->SetReferenceMode(fbx::FbxLayerElement::eDirect);
		fbx::FbxLayerElementArrayTemplate<fbx::FbxColor>& VertexColorArray = VertexColor->GetDirectArray();
		Layer->SetVertexColors(VertexColor);

		for (INT VertIndex = 0; VertIndex < VertexCount; ++VertIndex)
		{
			FLinearColor VertColor(1.0f, 1.0f, 1.0f);
			
			if (VertIndex < ColorVertexCount)
			{
				VertColor = ColorBufferToUse->VertexColor(VertIndex).ReinterpretAsLinear();
			}

			VertexColorArray.Add( fbx::FbxColor(VertColor.R, VertColor.G, VertColor.B, VertColor.A ));
		}
	}

	FbxActor->SetNodeAttribute(Mesh);

	return FbxActor;
}


} // namespace UnFbx

#endif //WITH_FBX
