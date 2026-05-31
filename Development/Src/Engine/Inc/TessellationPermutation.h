/*=============================================================================
	TessellationPermutation.h: BM2 tessellation permutation wrappers.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_TESSELLATIONPERMUTATION
#define _INC_TESSELLATIONPERMUTATION

#if BATMAN

/**
 * BM2 wraps every material V/H/D shader in a TessellationPermutation template
 * parameterized by an index 0..12. The index encodes a (TessellationPolicy, bA, bB)
 * tuple, and each wrapper specialization registers as a distinct FShaderType so
 * the FName matches BM2's cached shader names exactly.
 *
 * Permutation 0 (TP_NoTessellation,FALSE,FALSE) is the only one BM2 PC actually
 * cached, so it's the only one we register here. Adding more is a matter of
 * extending IMPLEMENT_TESSELLATION_SHADER_TYPE and the IMPLEMENT_*_TYPE callers.
 */
enum ETessellationPolicy
{
	TP_NoTessellation,
	TP_FlatTessellation,
	TP_PNTriangles,
	TP_PhongTessellation,
};

void ModifyTessellationPermutationCompilationEnvironment(INT PermutationIndex, EShaderFrequency ShaderFrequency, FShaderCompilerEnvironment& OutEnvironment);

/** Wraps a vertex shader. */
template<typename WrappedShaderType, INT PermutationIndex>
class TVertexShaderTessellationPermutation : public WrappedShaderType
{
	DECLARE_SHADER_TYPE(TVertexShaderTessellationPermutation, MeshMaterial);
public:
	TVertexShaderTessellationPermutation() {}
	TVertexShaderTessellationPermutation(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		WrappedShaderType(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		// We only cache the no-tessellation permutation; matches BM2 PC's RefShaderCache contents.
		return PermutationIndex == 0 && WrappedShaderType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ModifyTessellationPermutationCompilationEnvironment(PermutationIndex, SF_Vertex, OutEnvironment);
		WrappedShaderType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

/** Wraps a hull shader. */
template<typename WrappedShaderType, INT PermutationIndex>
class THullShaderTessellationPermutation : public WrappedShaderType
{
	DECLARE_SHADER_TYPE(THullShaderTessellationPermutation, MeshMaterial);
public:
	THullShaderTessellationPermutation() {}
	THullShaderTessellationPermutation(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		WrappedShaderType(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return PermutationIndex == 0 && WrappedShaderType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ModifyTessellationPermutationCompilationEnvironment(PermutationIndex, SF_Hull, OutEnvironment);
		WrappedShaderType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

/** Wraps a domain shader. */
template<typename WrappedShaderType, INT PermutationIndex>
class TDomainShaderTessellationPermutation : public WrappedShaderType
{
	DECLARE_SHADER_TYPE(TDomainShaderTessellationPermutation, MeshMaterial);
public:
	TDomainShaderTessellationPermutation() {}
	TDomainShaderTessellationPermutation(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		WrappedShaderType(Initializer)
	{}

	static UBOOL ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return PermutationIndex == 0 && WrappedShaderType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		ModifyTessellationPermutationCompilationEnvironment(PermutationIndex, SF_Domain, OutEnvironment);
		WrappedShaderType::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}
};

#endif // BATMAN

#endif // _INC_TESSELLATIONPERMUTATION
