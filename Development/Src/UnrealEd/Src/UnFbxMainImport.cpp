/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
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
	Main implementation of CFbxImporter : import FBX data to Unreal
=============================================================================*/

#include "UnrealEd.h"
#include "FFeedbackContextEditor.h"

#if WITH_FBX

#include "Factories.h"
#include "Engine.h"

#include "SkelImport.h"
#include "EnginePrefabClasses.h"
#include "EngineAnimClasses.h"
#include "UnFbxImporter.h"

namespace UnFbx
{

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter::CFbxImporter()
	: bFirstMesh(TRUE)
{
	// Create the SdkManager
	FbxSdkManager = fbx::FbxManager::Create();
	
	// create an IOSettings object
	fbx::FbxIOSettings * ios = fbx::FbxIOSettings::Create(FbxSdkManager, IOSROOT );
	FbxSdkManager->SetIOSettings(ios);

	// Create the geometry converter
	FbxGeometryConverter = new fbx::FbxGeometryConverter(FbxSdkManager);
	FbxScene = NULL;
	
	ImportOptions = new FBXImportOptions();
	ImportOptions->bConvertSceneUnit = TRUE;

	CurPhase = NOTSTARTED;

	FbxCamera = NULL;
}
	
//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter::~CFbxImporter()
{
	CleanUp();
}

const FLOAT CFbxImporter::SCALE_TOLERANCE = 0.000001;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
CFbxImporter* CFbxImporter::GetInstance()
{
	static CFbxImporter* ImporterInstance = NULL;
	
	if (ImporterInstance == NULL)
	{
		ImporterInstance = new CFbxImporter();
	}
	return ImporterInstance;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CFbxImporter::CleanUp()
{
	ReleaseScene();
	
	if (FbxSdkManager)
	{
		FbxSdkManager->Destroy();
	}
	FbxSdkManager = NULL;
	delete FbxGeometryConverter;
	FbxGeometryConverter = NULL;
	delete ImportOptions;
	ImportOptions = NULL;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CFbxImporter::ReleaseScene()
{
	if (Importer)
	{
		Importer->Destroy();
		Importer = NULL;
	}
	
	if (FbxScene)
	{
		FbxScene->Destroy();
		FbxScene = NULL;
	}
	
	// reset
	CollisionModels.Clear();
	SkelMeshToMorphMap.Reset();
	SkelMeshToMorphMap.Shrink();
	CurPhase = NOTSTARTED;
	bFirstMesh = TRUE;
}

FBXImportOptions* UnFbx::CFbxImporter::GetImportOptions()
{
	return ImportOptions;
}

INT CFbxImporter::DetectDeformer(const FFilename& InFilename)
{
	INT Result = 0;
	FString Filename = InFilename;
	
	if (OpenFile(Filename, TRUE))
	{
		fbx::FbxStatistics Statistics;
		Importer->GetStatistics(&Statistics);
		INT ItemIndex;
		fbx::FbxString ItemName;
		INT ItemCount;
		for ( ItemIndex = 0; ItemIndex < Statistics.GetNbItems(); ItemIndex++ )
		{
			Statistics.GetItemPair(ItemIndex, ItemName, ItemCount);
			if ( ItemName == "Deformer" && ItemCount > 0 )
			{
				Result = 1;
				break;
			}
		}
		Importer->Destroy();
		Importer = NULL;
		CurPhase = NOTSTARTED;
	}
	else
	{
		Result = -1;
	}
	
	return Result; 
}

UBOOL CFbxImporter::GetSceneInfo(FString Filename, FbxSceneInfo& SceneInfo)
{
	UBOOL Result = TRUE;
	FFeedbackContextEditor FbxImportWarn;
	FbxImportWarn.BeginSlowTask( TEXT("Parse FBX file to get scene info"), TRUE );
	
	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(Filename, FALSE))
		{
			Result = FALSE;
			break;
		}
		FbxImportWarn.UpdateProgress( 40, 100 );
	case FILEOPENED:
		if (!ImportFile(Filename))
		{
			Result = FALSE;
			break;
		}
		FbxImportWarn.UpdateProgress( 90, 100 );
	case IMPORTED:
	
	default:
		break;
	}
	
	if (Result)
	{
		fbx::FbxTime GlobalStart(fbx::FbxTime(FBXSDK_TC_INFINITY));
		fbx::FbxTime GlobalEnd(fbx::FbxTime(FBXSDK_TC_MINFINITY));
		
		TArray<fbx::FbxNode*> LinkNodes;
		
		SceneInfo.TotalMaterialNum = FbxScene->GetMaterialCount();
		SceneInfo.TotalTextureNum = FbxScene->GetTextureCount();
		SceneInfo.TotalGeometryNum = 0;
		SceneInfo.NonSkinnedMeshNum = 0;
		SceneInfo.SkinnedMeshNum = 0;
		for ( INT GeometryIndex = 0; GeometryIndex < FbxScene->GetGeometryCount(); GeometryIndex++ )
		{
			fbx::FbxGeometry * Geometry = FbxScene->GetGeometry(GeometryIndex);
			
			if (Geometry->GetAttributeType() == fbx::FbxNodeAttribute::eMesh)
			{
				fbx::FbxNode* GeoNode = Geometry->GetNode();
				UBOOL bIsLinkNode = FALSE;

				// check if this geometry node is used as link
				for ( INT i = 0; i < LinkNodes.Num(); i++ )
				{
					if ( GeoNode == LinkNodes(i))
					{
						bIsLinkNode = TRUE;
						break;
					}
				}
				// if the geometry node is used as link, ignore it
				if (bIsLinkNode)
				{
					continue;
				}

				SceneInfo.TotalGeometryNum++;
				
				fbx::FbxMesh* Mesh = (fbx::FbxMesh*)Geometry;
				SceneInfo.MeshInfo.Add();
				FbxMeshInfo& MeshInfo = SceneInfo.MeshInfo.Last();
				MeshInfo.Name = MakeName(GeoNode->GetName());
				MeshInfo.bTriangulated = Mesh->IsTriangleMesh();
				MeshInfo.MaterialNum = GeoNode->GetMaterialCount();
				MeshInfo.FaceNum = Mesh->GetPolygonCount();
				MeshInfo.VertexNum = Mesh->GetControlPointsCount();
				
				// LOD info
				MeshInfo.LODGroup = NULL;
				fbx::FbxNode* ParentNode = GeoNode->GetParent();
				if ( ParentNode->GetNodeAttribute() && ParentNode->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup)
				{
					fbx::FbxNodeAttribute* LODGroup = ParentNode->GetNodeAttribute();
					MeshInfo.LODGroup = MakeName(ParentNode->GetName());
					for (INT LODIndex = 0; LODIndex < ParentNode->GetChildCount(); LODIndex++)
					{
						if ( GeoNode == ParentNode->GetChild(LODIndex))
						{
							MeshInfo.LODLevel = LODIndex;
							break;
						}
					}
				}
				
				// skeletal mesh
				if (Mesh->GetDeformerCount(fbx::FbxDeformer::eSkin) > 0)
				{
					SceneInfo.SkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = TRUE;
					MeshInfo.MorphNum = Mesh->GetShapeCount();
					// skeleton root
					fbx::FbxSkin* Skin = (fbx::FbxSkin*)Mesh->GetDeformer(0, fbx::FbxDeformer::eSkin);
					fbx::FbxNode* Link = Skin->GetCluster(0)->GetLink();
					while (Link->GetParent() && Link->GetParent()->GetSkeleton())
					{
						Link = Link->GetParent();
					}
					MeshInfo.SkeletonRoot = MakeName(Link->GetName());
					MeshInfo.SkeletonElemNum = Link->GetChildCount(true);
					
					fbx::FbxTimeSpan AnimInterval(fbx::FbxTime(FBXSDK_TC_INFINITY), fbx::FbxTime(FBXSDK_TC_MINFINITY));
					Link->GetAnimationInterval(AnimInterval);
					const fbx::FbxTime Start = AnimInterval.GetStart();
					const fbx::FbxTime End = AnimInterval.GetStop();
					if ( Start < GlobalStart )
					{
						GlobalStart = Start;
					}
					if ( End > GlobalEnd )
					{
						 GlobalEnd = End;
					}
				}
				else
				{
					SceneInfo.NonSkinnedMeshNum++;
					MeshInfo.bIsSkelMesh = FALSE;
					MeshInfo.SkeletonRoot = NULL;
				}
			}
		}
		
		// TODO: display multiple anim stack
		SceneInfo.TakeName = NULL;
		for( INT AnimStackIndex = 0; AnimStackIndex < FbxScene->GetSrcObjectCount<fbx::FbxAnimStack>(); AnimStackIndex++ )
		{
			fbx::FbxAnimStack* CurAnimStack = fbx::FbxCast<fbx::FbxAnimStack>(FbxScene->GetSrcObject<fbx::FbxAnimStack>(0));
			// TODO: skip empty anim stack
			const char* AnimStackName = CurAnimStack->GetName();
			SceneInfo.TakeName = new char[strlen(AnimStackName) + 1];
			strcpy_s(SceneInfo.TakeName, strlen(AnimStackName) + 1, AnimStackName);
		}
		SceneInfo.FrameRate = fbx::FbxTime::GetFrameRate(FbxScene->GetGlobalSettings().GetTimeMode());
		
		if ( GlobalEnd > GlobalStart )
		{
			SceneInfo.TotalTime = (GlobalEnd.GetMilliSeconds() - GlobalStart.GetMilliSeconds())/1000.f * SceneInfo.FrameRate;
		}
		else
		{
			SceneInfo.TotalTime = 0;
		}
	}
	
	FbxImportWarn.EndSlowTask();
	return Result;
}


UBOOL CFbxImporter::OpenFile(FString Filename, UBOOL bParseStatistics)
{
	UBOOL Result = TRUE;
	
	if (CurPhase != NOTSTARTED)
	{
		// something go wrong
		return FALSE;
	}

	INT SDKMajor,  SDKMinor,  SDKRevision;

	// Create an importer.
	Importer = fbx::FbxImporter::Create(FbxSdkManager,"");

	// Get the version number of the FBX files generated by the
	// version of FBX SDK that you are using.
	fbx::FbxManager::GetFileFormatVersion(SDKMajor, SDKMinor, SDKRevision);

	// Initialize the importer by providing a filename.
	if (bParseStatistics)
	{
		Importer->ParseForStatistics(true);
	}
	
	const UBOOL bImportStatus = Importer->Initialize(TCHAR_TO_ANSI(*Filename));

	if( !bImportStatus )  // Problem with the file to be imported
	{
		warnf(NAME_Error,TEXT("Call to FbxImporter::Initialize() failed."));
		warnf(TEXT("Error returned: %s"), ANSI_TO_TCHAR(Importer->GetStatus().GetErrorString()));

		if (Importer->GetStatus().GetCode() == fbx::FbxStatus::eInvalidFileVersion)
		{
			warnf(TEXT("FBX version number for this FBX SDK is %d.%d.%d"),
				SDKMajor, SDKMinor, SDKRevision);
		}

		return FALSE;
	}

	CurPhase = FILEOPENED;
	// Destroy the importer
	//Importer->Destroy();

	return Result;
}

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(FbxSdkManager->GetIOSettings()))
#endif

UBOOL CFbxImporter::ImportFile(FString Filename)
{
	UBOOL Result = TRUE;
	
	UBOOL bStatus;
	
	FFilename FilePath(Filename);
	FileBasePath = FilePath.GetPath();

	// Create the Scene
	FbxScene = fbx::FbxScene::Create(FbxSdkManager,"");
	warnf(TEXT("Loading FBX Scene from %s"), *Filename);

	INT FileMajor, FileMinor, FileRevision;

	IOS_REF.SetBoolProp(IMP_FBX_MATERIAL,		true);
	IOS_REF.SetBoolProp(IMP_FBX_TEXTURE,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_LINK,			true);
	IOS_REF.SetBoolProp(IMP_FBX_SHAPE,		   true);
	IOS_REF.SetBoolProp(IMP_FBX_GOBO,			true);
	IOS_REF.SetBoolProp(IMP_FBX_ANIMATION,	   true);
	IOS_REF.SetBoolProp(IMP_SKINS,			   true);
	IOS_REF.SetBoolProp(IMP_DEFORMATION,		 true);
	IOS_REF.SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
	IOS_REF.SetBoolProp(IMP_TAKE,				true);

	// Import the scene.
	bStatus = Importer->Import(FbxScene);

	// Get the version number of the FBX file format.
	Importer->GetFileVersion(FileMajor, FileMinor, FileRevision);

	if(bStatus == FALSE &&	 // The import file may have a password
		Importer->GetStatus().GetCode() == fbx::FbxStatus::ePasswordError)
	{
		warnf(NAME_Error,TEXT("FBX file requires a password - unsupported."));
	}
	// output result
	if(bStatus)
	{
		warnf(TEXT("FBX Scene Loaded Succesfully"));
		CurPhase = IMPORTED;
	}
	else
	{
		ErrorMessage = ANSI_TO_TCHAR(Importer->GetStatus().GetErrorString());
		warnf(NAME_Error,TEXT("FBX Scene Loading Failed : %s"),*ErrorMessage);
		CleanUp();
		Result = FALSE;
		CurPhase = NOTSTARTED;
	}
	
	Importer->Destroy();
	Importer = NULL;
	
	return Result;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
UBOOL CFbxImporter::ImportFromFile(const TCHAR* Filename)
{
	UBOOL Result = TRUE;
	fbx::FbxAxisSystem::EFrontVector FrontVector = (fbx::FbxAxisSystem::EFrontVector)-fbx::FbxAxisSystem::eParityOdd;
	const fbx::FbxAxisSystem UnrealZUp(fbx::FbxAxisSystem::eZAxis, FrontVector, fbx::FbxAxisSystem::eRightHanded);
	
	switch (CurPhase)
	{
	case NOTSTARTED:
		if (!OpenFile(FString(Filename), FALSE))
		{
			Result = FALSE;
			break;
		}
	case FILEOPENED:
		if (!ImportFile(FString(Filename)))
		{
			Result = FALSE;
			CurPhase = NOTSTARTED;
			break;
		}
	case IMPORTED:
		// convert axis to Z-up
		fbx::FbxRootNodeUtility::RemoveAllFbxRoots( FbxScene );
		UnrealZUp.ConvertScene( FbxScene );

		if ( ImportOptions->bConvertSceneUnit )
		{
			const fbx::FbxSystemUnit SceneUnit = FbxScene->GetGlobalSettings().GetSystemUnit();
			if ( SceneUnit != fbx::FbxSystemUnit::cm )
			{
				warnf(NAME_Log, TEXT("Converting FBX scene from %s to cm"), ANSI_TO_TCHAR(SceneUnit.GetScaleFactorAsString().Buffer()));
				fbx::FbxSystemUnit::cm.ConvertScene( FbxScene );
			}
		}


		// convert name to unreal-supported format
		// actually, crashes...
		//fbx::FbxSceneRenamer renamer(FbxScene);
		//renamer.ResolveNameClashing(false,false,true,true,true,fbx::FbxString(),"TestBen",false,false);
		
	default:
		break;
	}
	
	return Result;
}

char* CFbxImporter::MakeName(const char* Name)
{
	int SpecialChars[] = {'.', ',', '/', '`', '%'};
	char* TmpName = new char[strlen(Name)+1];
	int len = strlen(Name);
	strcpy_s(TmpName, strlen(Name) + 1, Name);

	for ( INT i = 0; i < 5; i++ )
	{
		char* CharPtr = TmpName;
		while ( (CharPtr = strchr(CharPtr,SpecialChars[i])) != NULL )
		{
			CharPtr[0] = '_';
		}
	}

	// Remove namespaces
	char* NewName;
	NewName = strchr (TmpName, ':');
	  
	// there may be multiple namespace, so find the last ':'
	while (NewName && strchr(NewName + 1, ':'))
	{
		NewName = strchr(NewName + 1, ':');
	}

	if (NewName)
	{
		return NewName + 1;
	}

	return TmpName;
}

FName CFbxImporter::MakeNameForMesh(FString InName, fbx::FbxObject* FbxObject)
{
	FName OutputName;

	// "Name" field can't be empty
	if (ImportOptions->bUsedAsFullName && InName != FString("None"))
	{
		OutputName = *InName;
	}
	else
	{
		char Name[512];
		int SpecialChars[] = {'.', ',', '/', '`', '%'};
		sprintf_s(Name,512,"%s",FbxObject->GetName());

		for ( INT i = 0; i < 5; i++ )
		{
			char* CharPtr = Name;
			while ( (CharPtr = strchr(CharPtr,SpecialChars[i])) != NULL )
			{
				CharPtr[0] = '_';
			}
		}

		// for mesh, replace ':' with '_' because Unreal doesn't support ':' in mesh name
		char* NewName = NULL;
		NewName = strchr (Name, ':');

		if (NewName)
		{
			char* Tmp;
			Tmp = NewName;
			while (Tmp)
			{

				// Always remove namespaces
				NewName = Tmp + 1;
				
				// there may be multiple namespace, so find the last ':'
				Tmp = strchr(NewName + 1, ':');
			}
		}
		else
		{
			NewName = Name;
		}

		if ( InName == FString("None"))
		{
			OutputName = FName( *FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(NewName )) );
		}
		else
		{
			OutputName = FName( *FString::Printf(TEXT("%s_%s"), *InName,ANSI_TO_TCHAR(NewName)) );
		}
	}
	
	return OutputName;
}

fbx::FbxAMatrix CFbxImporter::ComputeTotalMatrix(fbx::FbxNode* Node)
{
	fbx::FbxAMatrix Geometry;
	fbx::FbxVector4 Translation, Rotation, Scaling;
	Translation = Node->GetGeometricTranslation(fbx::FbxNode::eSourcePivot);
	Rotation = Node->GetGeometricRotation(fbx::FbxNode::eSourcePivot);
	Scaling = Node->GetGeometricScaling(fbx::FbxNode::eSourcePivot);
	Geometry.SetT(Translation);
	Geometry.SetR(Rotation);
	Geometry.SetS(Scaling);

	//For Single Matrix situation, obtain transfrom matrix from eDestinationPivot, which include pivot offsets and pre/post rotations.
	fbx::FbxAMatrix& GlobalTransform = FbxScene->GetAnimationEvaluator()->GetNodeGlobalTransform(Node);
	
	fbx::FbxAMatrix TotalMatrix;
	TotalMatrix = GlobalTransform * Geometry;

	return TotalMatrix;
}

/**
* Recursively get skeletal mesh count
*
* @param Node Root node to find skeletal meshes
* @return INT skeletal mesh count
*/
INT GetFbxSkeletalMeshCount(fbx::FbxNode* Node)
{
	INT SkeletalMeshCount = 0;
	if (Node->GetMesh() && (Node->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin)>0))
	{
		SkeletalMeshCount = 1;
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		SkeletalMeshCount += GetFbxSkeletalMeshCount(Node->GetChild(ChildIndex));
	}

	return SkeletalMeshCount;
}

/**
* Get mesh count (including static mesh and skeletal mesh, except collision models) and find collision models
*
* @param Node Root node to find meshes
* @param FbxImporter
* @return INT mesh count
*/
INT CFbxImporter::GetFbxMeshCount(fbx::FbxNode* Node, UnFbx::CFbxImporter* FbxImporter)
{
	INT MeshCount = 0;
	if (Node->GetMesh())
	{
		if (!FbxImporter->FillCollisionModelList(Node))
		{
			MeshCount = 1;
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		MeshCount += GetFbxMeshCount(Node->GetChild(ChildIndex), FbxImporter);
	}

	return MeshCount;
}

/**
* Get all Fbx mesh objects
*
* @param Node Root node to find meshes
* @param outMeshArray return Fbx meshes
*/
void CFbxImporter::FillFbxMeshArray(fbx::FbxNode* Node, TArray<fbx::FbxNode*>& outMeshArray, UnFbx::CFbxImporter* FbxImporter)
{
	if (Node->GetMesh())
	{
		if (!FbxImporter->FillCollisionModelList(Node))
		{ 
			outMeshArray.AddItem(Node);
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxMeshArray(Node->GetChild(ChildIndex), outMeshArray, FbxImporter);
	}
}

/**
* Get all Fbx skeletal mesh objects
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes
*/
void FillFbxSkelMeshArray(fbx::FbxNode* Node, TArray<fbx::FbxNode*>& outSkelMeshArray)
{
	if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin) > 0 )
	{
		outSkelMeshArray.AddItem(Node);
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		FillFbxSkelMeshArray(Node->GetChild(ChildIndex), outSkelMeshArray);
	}
}

void CFbxImporter::RecursiveFixSkeleton(fbx::FbxNode* Node)
{
	fbx::FbxNodeAttribute* Attr = Node->GetNodeAttribute();
	fbx::FbxString n = Node->GetName();
	const char* name = n.Buffer();
	if (Attr && (Attr->GetAttributeType() == fbx::FbxNodeAttribute::eMesh || Attr->GetAttributeType() == fbx::FbxNodeAttribute::eNull))
	{
		//replace with skeleton
		fbx::FbxSkeleton* FbxSkeleton = fbx::FbxSkeleton::Create(FbxSdkManager,"");
		Node->SetNodeAttribute(FbxSkeleton);
		FbxSkeleton->SetSkeletonType(fbx::FbxSkeleton::eLimbNode);
	}

	for (INT i = 0; i < Node->GetChildCount(); i++)
	{
		RecursiveFixSkeleton(Node->GetChild(i));
	}
}

fbx::FbxNode* CFbxImporter::GetRootSkeleton(fbx::FbxNode* Link)
{
	fbx::FbxNode* RootBone = Link;
	// get FBX skeleton root
	while (RootBone->GetParent() && RootBone->GetParent()->GetSkeleton())
	{
		RootBone = RootBone->GetParent();
	}

	// get Unreal skeleton root
	// mesh and dummy are used as bone if they are in the skeleton hierarchy
	while (RootBone->GetParent())
	{
		fbx::FbxNodeAttribute* Attr = RootBone->GetParent()->GetNodeAttribute();
		if (Attr && 
			(Attr->GetAttributeType() == fbx::FbxNodeAttribute::eMesh || Attr->GetAttributeType() == fbx::FbxNodeAttribute::eNull) &&
			RootBone->GetParent() != FbxScene->GetRootNode())
		{
			// in some case, skeletal mesh can be ancestor of bones
			// this avoids this situation
			if (Attr->GetAttributeType() == fbx::FbxNodeAttribute::eMesh )
			{
				fbx::FbxMesh* FbxMesh = (fbx::FbxMesh*)Attr;
				if (FbxMesh->GetDeformerCount(fbx::FbxDeformer::eSkin) > 0)
				{
					break;
				}
			}

			RootBone = RootBone->GetParent();
		}
		else
		{
			break;
		}
	}

	return RootBone;
}

/**
* Get all Fbx skeletal mesh objects which are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
* @param SkeletonArray
* @param ExpandLOD flag of expanding LOD to get each mesh
*/
void CFbxImporter::RecursiveFindFbxSkelMesh(fbx::FbxNode* Node, TArray< TArray<fbx::FbxNode*>* >& outSkelMeshArray, TArray<fbx::FbxNode*>& SkeletonArray, UBOOL ExpandLOD)
{
	fbx::FbxNode* SkelMeshNode = NULL;
	fbx::FbxNode* NodeToAdd = Node;

	if (Node->GetMesh() && Node->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin) > 0 )
	{
		SkelMeshNode = Node;
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		SkelMeshNode = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (!(SkelMeshNode->GetMesh() && SkelMeshNode->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin) > 0))
		{
			SkelMeshNode = NULL;
		}
		else if (ExpandLOD)
		{
			// if ExpandLOD is TRUE, only add the first LODGroup level node
			NodeToAdd = SkelMeshNode;
		}
		// else NodeToAdd = Node;
	}

	if (SkelMeshNode)
	{
		// find root skeleton
		fbx::FbxSkin* Deformer = (fbx::FbxSkin*)SkelMeshNode->GetMesh()->GetDeformer(0);
		fbx::FbxNode* Link = Deformer->GetCluster(0)->GetLink();
		Link = GetRootSkeleton(Link);

		INT i;
		for (i = 0; i < SkeletonArray.Num(); i++)
		{
			if ( Link == SkeletonArray(i) )
			{
				// append to existed outSkelMeshArray element
				TArray<fbx::FbxNode*>* TempArray = outSkelMeshArray(i);
				TempArray->AddItem(NodeToAdd);
				break;
			}
		}

		// if there is no outSkelMeshArray element that is bind to this skeleton
		// create new element for outSkelMeshArray
		if ( i == SkeletonArray.Num() )
		{
			TArray<fbx::FbxNode*>* TempArray = new TArray<fbx::FbxNode*>();
			TempArray->AddItem(NodeToAdd);
			outSkelMeshArray.AddItem(TempArray);
			SkeletonArray.AddItem(Link);
		}
	}
	else
	{
		INT ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindFbxSkelMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

void CFbxImporter::RecursiveFindRigidMesh(fbx::FbxNode* Node, TArray< TArray<fbx::FbxNode*>* >& outSkelMeshArray, TArray<fbx::FbxNode*>& SkeletonArray, UBOOL ExpandLOD)
{
	UBOOL RigidNodeFound = FALSE;
	fbx::FbxNode* RigidMeshNode = NULL;

	if (Node->GetMesh())
	{
		// ignore skeletal mesh
		if (Node->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin) == 0 )
		{
			for (INT MatIndex = 0; !RigidNodeFound && MatIndex < Node->GetMaterialCount(); MatIndex++)
			{
				fbx::FbxSurfaceMaterial* Mat = Node->GetMaterial(MatIndex);
				INT TexIndex;
				for (TexIndex = 0; TexIndex < fbx::FbxLayerElement::sTypeTextureCount; TexIndex++)
				{
					fbx::FbxProperty FbxProperty = Mat->FindProperty(fbx::FbxLayerElement::sTextureChannelNames[TexIndex]);
					if( FbxProperty.IsValid() && FbxProperty.GetSrcObjectCount<fbx::FbxTexture>())
					{
						// texture found
						RigidMeshNode = Node;
						RigidNodeFound = TRUE;
						break;
					}
				}
			}
		}
	}
	else if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup)
	{
		// for LODgroup, add the LODgroup to OutSkelMeshArray according to the skeleton that the first child bind to
		fbx::FbxNode* FirstLOD = Node->GetChild(0);
		// check if the first child is skeletal mesh
		if (FirstLOD->GetMesh())
		{
			if (FirstLOD->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin) == 0 )
			{
				for (INT MatIndex = 0; !RigidNodeFound && MatIndex < RigidMeshNode->GetMaterialCount(); MatIndex++)
				{
					fbx::FbxSurfaceMaterial* Mat = FirstLOD->GetMaterial(MatIndex);
					INT TexIndex;
					for (TexIndex = 0; TexIndex < fbx::FbxLayerElement::sTypeTextureCount; TexIndex++)
					{
						fbx::FbxProperty FbxProperty = Mat->FindProperty(fbx::FbxLayerElement::sTextureChannelNames[TexIndex]);
						if( FbxProperty.IsValid() && FbxProperty.GetSrcObjectCount<fbx::FbxTexture>())
						{
							// texture found
							RigidNodeFound = TRUE;
							break;
						}
					}
				}
			}
		}

		if (RigidNodeFound)
		{
			if (ExpandLOD)
			{
				RigidMeshNode = FirstLOD;
			}
			else
			{
				RigidMeshNode = Node;
			}

		}
	}

	if (RigidMeshNode)
	{
		// find root skeleton
		fbx::FbxNode* Link = GetRootSkeleton(RigidMeshNode);

		INT i;
		for (i = 0; i < SkeletonArray.Num(); i++)
		{
			if ( Link == SkeletonArray(i))
			{
				// append to existed outSkelMeshArray element
				TArray<fbx::FbxNode*>* TempArray = outSkelMeshArray(i);
				TempArray->AddItem(RigidMeshNode);
				break;
			}
		}

		// if there is no outSkelMeshArray element that is bind to this skeleton
		// create new element for outSkelMeshArray
		if ( i == SkeletonArray.Num() )
		{
			TArray<fbx::FbxNode*>* TempArray = new TArray<fbx::FbxNode*>();
			TempArray->AddItem(RigidMeshNode);
			outSkelMeshArray.AddItem(TempArray);
			SkeletonArray.AddItem(Link);
		}
	}

	// for LODGroup, we will not deep in.
	if (!(Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup))
	{
		INT ChildIndex;
		for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
		{
			RecursiveFindRigidMesh(Node->GetChild(ChildIndex), outSkelMeshArray, SkeletonArray, ExpandLOD);
		}
	}
}

/**
* Get all Fbx skeletal mesh objects in the scene. these meshes are grouped by skeleton they bind to
*
* @param Node Root node to find skeletal meshes
* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
*/
void CFbxImporter::FillFbxSkelMeshArrayInScene(fbx::FbxNode* Node, TArray< TArray<fbx::FbxNode*>* >& outSkelMeshArray, UBOOL ExpandLOD)
{
	TArray<fbx::FbxNode*> SkeletonArray;

	// a) find skeletal meshes
	
	RecursiveFindFbxSkelMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	// for skeletal mesh, we convert the skeleton system to skeleton
	// in less we recognize bone mesh as rigid mesh if they are textured
	for ( INT SkelIndex = 0; SkelIndex < SkeletonArray.Num(); SkelIndex++)
	{
		RecursiveFixSkeleton(SkeletonArray(SkelIndex));
	}

	SkeletonArray.Empty();
	// b) find rigid mesh
	
	// for rigid meshes, we don't convert to bone
	if (ImportOptions->bImportAnimSet && ImportOptions->bImportRigidAnim)
	{
		RecursiveFindRigidMesh(Node, outSkelMeshArray, SkeletonArray, ExpandLOD);
	}
}

fbx::FbxNode* CFbxImporter::FindFBXMeshesByBone(USkeletalMesh* FillInMesh, UBOOL bExpandLOD, TArray<fbx::FbxNode*>& OutFBXMeshNodeArray)
{
	// get the root bone of Unreal skeletal mesh
	FMeshBone& Bone = FillInMesh->RefSkeleton(0);
	FString BoneNameString = Bone.Name.ToString();
	const char *BoneName = TCHAR_TO_ANSI(*BoneNameString);

	// we do not need to check if the skeleton root node is a skeleton
	// because the animation may be a rigid animation
	fbx::FbxNode* SkeletonRoot = NULL;

	// find the FBX skeleton node according to name
	SkeletonRoot = FbxScene->FindNodeByName(TCHAR_TO_ANSI(*BoneNameString));

	// Since bone names are stored as FNames, it's possible that the case of the
	// bone name in the engine doesn't match that of the one in the FBX file and
	// would not be found by FindNodeByName()
	if (!SkeletonRoot)
	{
		for (INT NodeIndex = 0; NodeIndex < FbxScene->GetNodeCount(); NodeIndex++)
		{
			fbx::FbxNode* FbxNode = FbxScene->GetNode(NodeIndex);

			if (!appStricmp(ANSI_TO_TCHAR(FbxNode->GetName()), *BoneNameString))
			{
				SkeletonRoot = FbxNode;
				break;
			}
		}
	}

	// take the namespace into account, match name without namespace
	// when import the skeletal mesh, the namespace in node name may be stripped
	if (!SkeletonRoot)
	{
		for (INT NodeIndex = 0; NodeIndex < FbxScene->GetNodeCount(); NodeIndex++)
		{
			fbx::FbxNode* FbxNode = FbxScene->GetNode(NodeIndex);
			char NodeName[512];
			sprintf_s(NodeName, 512, "%s", FbxNode->GetName());
			char* NameRmNS = strchr (NodeName, ':');

			// there may be multiple namespace, so find the last ':'
			while (NameRmNS && strchr(NameRmNS + 1, ':'))
			{
				NameRmNS = strchr(NameRmNS + 1, ':');
			}

			if (NameRmNS)
			{
				NameRmNS = NameRmNS + 1;
			}
			else
			{
				NameRmNS = NodeName;
			}

			if (!_stricmp(NameRmNS, TCHAR_TO_ANSI(*BoneNameString)))
			{
				// name is matched after strip the namespace
				SkeletonRoot = FbxNode;
				break;
			}
		}
	}

	// return if do not find matched FBX skeleton
	if (!SkeletonRoot)
	{
		return NULL;
	}
	

	// Get Mesh nodes array that bind to the skeleton system
	// 1, get all skeltal meshes in the FBX file
	TArray< TArray<fbx::FbxNode*>* > SkelMeshArray;
	FillFbxSkelMeshArrayInScene(FbxScene->GetRootNode(), SkelMeshArray, FALSE);

	// 2, then get skeletal meshes that bind to this skeleton
	for (INT SkelMeshIndex = 0; SkelMeshIndex < SkelMeshArray.Num(); SkelMeshIndex++)
	{
		fbx::FbxNode* FbxNode = (*SkelMeshArray(SkelMeshIndex))(0);
		fbx::FbxNode* MeshNode;
		if (FbxNode->GetNodeAttribute() && FbxNode->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup)
		{
			MeshNode = FbxNode->GetChild(0);
		}
		else
		{
			MeshNode = FbxNode;
		}

		// 3, get the root bone that the mesh bind to
		fbx::FbxSkin* Deformer = (fbx::FbxSkin*)MeshNode->GetMesh()->GetDeformer(0);
		fbx::FbxNode* Link = Deformer->GetCluster(0)->GetLink();
		Link = GetRootSkeleton(Link);
		// 4, fill in the mesh node
		if (Link == SkeletonRoot)
		{
			// copy meshes
			if (bExpandLOD)
			{
				TArray<fbx::FbxNode*> SkelMeshes = 	*SkelMeshArray(SkelMeshIndex);
				for (INT NodeIndex = 0; NodeIndex < SkelMeshes.Num(); NodeIndex++)
				{
					fbx::FbxNode* Node = SkelMeshes(NodeIndex);
					if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == fbx::FbxNodeAttribute::eLODGroup)
					{
						OutFBXMeshNodeArray.AddItem(Node->GetChild(0));
					}
					else
					{
						OutFBXMeshNodeArray.AddItem(Node);
					}
				}
			}
			else
			{
				OutFBXMeshNodeArray.Append(*SkelMeshArray(SkelMeshIndex));
			}
			break;
		}
	}

	for (INT i = 0; i < SkelMeshArray.Num(); i++)
	{
		delete SkelMeshArray(i);
	}

	return SkeletonRoot;
}

/**
* Get the first Fbx mesh node.
*
* @param Node Root node
* @param bIsSkelMesh if we want a skeletal mesh
* @return fbx::FbxNode* the node containing the first mesh
*/
fbx::FbxNode* GetFirstFbxMesh(fbx::FbxNode* Node, UBOOL bIsSkelMesh)
{
	if (Node->GetMesh())
	{
		if (bIsSkelMesh)
		{
			if (Node->GetMesh()->GetDeformerCount(fbx::FbxDeformer::eSkin)>0)
			{
				return Node;
			}
		}
		else
		{
			return Node;
		}
	}

	INT ChildIndex;
	for (ChildIndex=0; ChildIndex<Node->GetChildCount(); ++ChildIndex)
	{
		fbx::FbxNode* FirstMesh;
		FirstMesh = GetFirstFbxMesh(Node->GetChild(ChildIndex), bIsSkelMesh);

		if (FirstMesh)
		{
			return FirstMesh;
		}
	}

	return NULL;
}

void CFbxImporter::CheckSmoothingInfo(fbx::FbxMesh* FbxMesh)
{
	if (bFirstMesh)
	{
		bFirstMesh = FALSE;	 // don't check again
		
		fbx::FbxLayer* LayerSmoothing = FbxMesh->GetLayer(0, fbx::FbxLayerElement::eSmoothing);
		if (!LayerSmoothing)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_NoSmoothgroupForFBXScene"));
		}
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
fbx::FbxNode* CFbxImporter::RetrieveObjectFromName(const TCHAR* ObjectName, fbx::FbxNode* Root)
{
	fbx::FbxNode* Result = NULL;
	
	if ( FbxScene != NULL )
	{
		if (Root == NULL)
		{
			Root = FbxScene->GetRootNode();
		}

		for (INT ChildIndex=0;ChildIndex<Root->GetChildCount() && !Result;++ChildIndex)
		{
			fbx::FbxNode* Node = Root->GetChild(ChildIndex);
			fbx::FbxMesh* FbxMesh = Node->GetMesh();
			if (FbxMesh && 0 == appStrcmp(ObjectName,ANSI_TO_TCHAR(Node->GetName())))
			{
				Result = Node;
			}
			else
			{
				Result = RetrieveObjectFromName(ObjectName,Node);
			}
		}
	}
	return Result;
}

} // namespace UnFbx

#endif //WITH_FBX
