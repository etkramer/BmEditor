/*=============================================================================
	OpenGLShaders.cpp: OpenGL shader RHI implementation.
	Copyright 1998-2010 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if _WINDOWS
#include <mmintrin.h>
#elif PLATFORM_MACOSX
#include <xmmintrin.h>
#endif

UBOOL GProgramCacheLoaded = FALSE;
TMap<DWORD,FVertexShaderRHIRef> GLoadedVertexShaders;
TMap<DWORD,FPixelShaderRHIRef> GLoadedPixelShaders;

FVertexShaderRHIRef FOpenGLDynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	DWORD CodeCrc = appMemCrc(&Code(0), Code.Num());
	FVertexShaderRHIRef VertexShaderRHI = GLoadedVertexShaders.FindRef(CodeCrc);
	if (VertexShaderRHI)
	{
		return VertexShaderRHI;
	}

	FOpenGLVertexShader *Shader = new FOpenGLVertexShader(Code);
	Shader->Compile();
	GLoadedVertexShaders.Set(Shader->CodeCrc, Shader);
	GProgramCacheLoaded = FALSE;
	return Shader;
}

FPixelShaderRHIRef FOpenGLDynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	DWORD CodeCrc = appMemCrc(&Code(0), Code.Num());
	FPixelShaderRHIRef PixelShaderRHI = GLoadedPixelShaders.FindRef(CodeCrc);
	if (PixelShaderRHI)
	{
		return PixelShaderRHI;
	}

	FOpenGLPixelShader *Shader = new FOpenGLPixelShader(Code);
	Shader->Compile();
	GLoadedPixelShaders.Set(Shader->CodeCrc, Shader);
	GProgramCacheLoaded = FALSE;
	return Shader;
}

/**
* Key used to map a set of unique vertex/pixel shader combinations to
* a program resource
*/
class FGLSLProgramKey
{
public:

	FGLSLProgramKey(
		FVertexShaderRHIParamRef InVertexShader,
		FPixelShaderRHIParamRef InPixelShader,
		DWORD* InStreamStrides
		)
		:	VertexShader(InVertexShader)
		,	PixelShader(InPixelShader)
	{
		for( UINT Idx=0; Idx < MaxVertexElementCount; ++Idx )
		{
			StreamStrides[Idx] = (BYTE)InStreamStrides[Idx];
		}
	}

	/**
	* Equality is based on render and depth stencil targets 
	* @param Other - instance to compare against
	* @return TRUE if equal
	*/
	friend UBOOL operator ==(const FGLSLProgramKey& A,const FGLSLProgramKey& B)
	{
		return	A.VertexShader == B.VertexShader && A.PixelShader == B.PixelShader && !appMemcmp(A.StreamStrides, B.StreamStrides, sizeof(A.StreamStrides));
	}

	/**
	* Get the hash for this type. 
	* @param Key - struct to hash
	* @return DWORD hash based on type
	*/
	friend DWORD GetTypeHash(const FGLSLProgramKey &Key)
	{
		return GetTypeHash(Key.VertexShader) ^ GetTypeHash(Key.PixelShader) ^ appMemCrc(Key.StreamStrides,sizeof(Key.StreamStrides));
	}

	FVertexShaderRHIRef VertexShader;
	FPixelShaderRHIRef PixelShader;
	/** assuming strides are always smaller than 8-bits */
	BYTE StreamStrides[MaxVertexElementCount];
};

typedef TMap<FGLSLProgramKey,GLuint> FGLSLProgramCache;

/** Lazily initialized program cache singleton. */
static FGLSLProgramCache& GetGLSLProgramCache()
{
	static FGLSLProgramCache GLSLProgramCache;
	return GLSLProgramCache;
}

static GLuint GetGLSLProgram(FVertexShaderRHIParamRef VertexShaderRHI, FPixelShaderRHIParamRef PixelShaderRHI, DWORD* StreamStrides);

const INT SingleProgramSizeInCache = 2 * sizeof(DWORD) + MaxVertexElementCount;

void LoadProgramCache()
{
	if (GProgramCacheLoaded)
	{
		return;
	}

	FString CacheFileName = FString::Printf(TEXT("%s\\OpenGLProgramCache.bin"), *appGameDir());
	FArchive* Reader = GFileManager->CreateFileReader( *CacheFileName );
	if (Reader)
	{
		INT ReaderTotalSize	= Reader->TotalSize();
		BYTE *Buffer = (BYTE *)appMalloc(ReaderTotalSize);
		Reader->Serialize(Buffer, ReaderTotalSize);

		INT NumPrograms = ReaderTotalSize / SingleProgramSizeInCache;
		for (INT Index = 0; Index < NumPrograms; Index++)
		{
			DWORD VertexShaderCrc = *((DWORD *)(Buffer + Index * SingleProgramSizeInCache));
			DWORD PixelShaderCrc = *((DWORD *)(Buffer + Index * SingleProgramSizeInCache + sizeof(DWORD)));
			DWORD StreamStrides[MaxVertexElementCount];
			BYTE *StreamStridesBytes = Buffer + Index * SingleProgramSizeInCache + sizeof(DWORD) * 2;
			for (UINT StreamIndex = 0; StreamIndex < MaxVertexElementCount; StreamIndex++)
			{
				StreamStrides[StreamIndex] = StreamStridesBytes[StreamIndex];
			}

			FVertexShaderRHIRef VertexShaderRHI = GLoadedVertexShaders.FindRef(VertexShaderCrc);
			FPixelShaderRHIRef PixelShaderRHI = GLoadedPixelShaders.FindRef(PixelShaderCrc);
			if (VertexShaderRHI && PixelShaderRHI)
			{
				GetGLSLProgram(VertexShaderRHI, PixelShaderRHI, (DWORD*)StreamStrides);
			}
		}

		appFree(Buffer);
		delete Reader;
	}

	GProgramCacheLoaded = TRUE;
}

static void AddUniqueProgramToBuffer(TArray<BYTE> &Buffer, DWORD InVertexShaderCrc, DWORD InPixelShaderCrc, BYTE *InStreamStrides)
{
	UBOOL bFound = FALSE;
	INT NumPrograms = Buffer.Num() / SingleProgramSizeInCache;
	for (INT Index = 0; Index < NumPrograms; Index++)
	{
		DWORD VertexShaderCrc = *((DWORD *)(&Buffer(0) + Index * SingleProgramSizeInCache));
		DWORD PixelShaderCrc = *((DWORD *)(&Buffer(0) + Index * SingleProgramSizeInCache + sizeof(DWORD)));
		BYTE *StreamStrides = &Buffer(0) + Index * SingleProgramSizeInCache + sizeof(DWORD) * 2;

		if (VertexShaderCrc == InVertexShaderCrc && PixelShaderCrc == InPixelShaderCrc)
		{
			UBOOL bEqual = TRUE;
			for (UINT StreamIndex = 0; StreamIndex < MaxVertexElementCount; StreamIndex++)
			{
				if (StreamStrides[StreamIndex] != InStreamStrides[StreamIndex])
				{
					bEqual = FALSE;
					break;
				}
			}

			if (bEqual)
			{
				bFound = TRUE;
				break;
			}
		}
	}

	if (!bFound)
	{
		INT Offset = Buffer.Add(2 * sizeof(DWORD) + MaxVertexElementCount);
		appMemcpy(&Buffer(Offset), &InVertexShaderCrc, sizeof(DWORD));
		appMemcpy(&Buffer(Offset + sizeof(DWORD)), &InPixelShaderCrc, sizeof(DWORD));
		appMemcpy(&Buffer(Offset + 2 * sizeof(DWORD)), InStreamStrides, MaxVertexElementCount);
	}
}

GLuint GetGLSLProgram(FVertexShaderRHIParamRef VertexShaderRHI, FPixelShaderRHIParamRef PixelShaderRHI, DWORD* StreamStrides)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,VertexShader);
	DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,PixelShader);

	GLuint Program = GetGLSLProgramCache().FindRef(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, StreamStrides));
	if (!Program)
	{
		Program = glCreateProgram();

		glAttachShader(Program, VertexShader->Resource);
		glAttachShader(Program, PixelShader->Resource);

		debugf(TEXT("Creating program %d from %d and %d"), Program, VertexShader->Resource, PixelShader->Resource);

		glBindAttribLocation(Program, GLAttr_Position, "_Un_AttrPosition0");
		glBindAttribLocation(Program, GLAttr_Tangent, "_Un_AttrTangent");
		glBindAttribLocation(Program, GLAttr_Color, "_Un_AttrColor0");
		glBindAttribLocation(Program, GLAttr_Binormal, "_Un_AttrColor1");
		glBindAttribLocation(Program, GLAttr_Binormal, "_Un_AttrBinormal0");
		glBindAttribLocation(Program, GLAttr_Normal, "_Un_AttrNormal0");
		glBindAttribLocation(Program, GLAttr_Weights, "_Un_AttrBlendWeight0");
		glBindAttribLocation(Program, GLAttr_Bones, "_Un_AttrBlendIndices0");
		glBindAttribLocation(Program, GLAttr_TexCoord0, "_Un_AttrTexCoord0");
		glBindAttribLocation(Program, GLAttr_TexCoord1, "_Un_AttrTexCoord1");
		glBindAttribLocation(Program, GLAttr_TexCoord2, "_Un_AttrTexCoord2");
		glBindAttribLocation(Program, GLAttr_TexCoord3, "_Un_AttrTexCoord3");
		glBindAttribLocation(Program, GLAttr_TexCoord4, "_Un_AttrTexCoord4");
		glBindAttribLocation(Program, GLAttr_TexCoord5, "_Un_AttrTexCoord5");
		glBindAttribLocation(Program, GLAttr_TexCoord6, "_Un_AttrTexCoord6");
		glBindAttribLocation(Program, GLAttr_TexCoord7, "_Un_AttrTexCoord7");

		glLinkProgram(Program);
		glValidateProgram(Program);

		GLint LinkStatus;
		glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
		if (LinkStatus != GL_TRUE)
		{
			GLint LogLength;
			glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
			if (LogLength > 1)
			{
				ANSICHAR *LinkLog = (ANSICHAR *)appMalloc(LogLength);
				glGetProgramInfoLog(Program, LogLength, NULL, LinkLog);
				appErrorf(TEXT("Failed to link GLSL program. Link log:\n%s"), ANSI_TO_TCHAR(LinkLog));
				appFree(LinkLog);
			}
			else
			{
				appErrorf(TEXT("Failed to link GLSL program. No link log."));
			}

			glDeleteProgram(Program);
			Program = 0;
		}
		else
		{
			GetGLSLProgramCache().Set(FGLSLProgramKey(VertexShaderRHI, PixelShaderRHI, StreamStrides), Program);
		}
	}

	return Program;
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param PixelShader - existing pixel shader
*/
FBoundShaderStateRHIRef FOpenGLDynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI
	)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateBoundShaderStateTime);

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		NULL,
		StreamStrides,
		VertexShaderRHI,
		PixelShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FOpenGLBoundShaderState(this,VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI);
	}
}

template <ERHIResourceTypes ResourceTypeEnum, GLenum Type>
UBOOL TOpenGLShader<ResourceTypeEnum, Type>::Compile()
{
	if (Resource)
	{
		return TRUE;
	}

	Resource = glCreateShader(Type);
	GLchar *CodePtr = (GLchar *)&Code(0);

#if !OPENGL_USE_BINDABLE_UNIFORMS
	while (ANSICHAR *Bindable = strstr(CodePtr, "bindable uniform"))
	{
		Bindable[0] = '/';
		Bindable[1] = '*';
		Bindable[7] = '*';
		Bindable[8] = '/';
	}
#endif

	GLint CodeLength = strlen(CodePtr);
	glShaderSource(Resource, 1, (const GLchar **)&CodePtr, &CodeLength);
	glCompileShader(Resource);

	GLint CompileStatus;
	glGetShaderiv(Resource, GL_COMPILE_STATUS, &CompileStatus);
	if (CompileStatus != GL_TRUE)
	{
		GLint LogLength;
		glGetShaderiv(Resource, GL_INFO_LOG_LENGTH, &LogLength);
		if (LogLength > 1)
		{
			ANSICHAR *CompileLog = (ANSICHAR *)appMalloc(LogLength);
			glGetShaderInfoLog(Resource, LogLength, NULL, CompileLog);
			appErrorf(TEXT("Failed to compile shader. Compile log:\n%s"), ANSI_TO_TCHAR(CompileLog));
			appFree(CompileLog);
		}
		else
		{
			appErrorf(TEXT("Failed to compile shader. No compile log."));
		}

		glDeleteShader(Resource);
		Resource = 0;

		return FALSE;
	}
	else
	{
		Code.Empty();

		return TRUE;
	}
}

FOpenGLBoundShaderState::FOpenGLBoundShaderState(
	class FOpenGLDynamicRHI* InOpenGLRHI,
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI
	)
:	CacheLink(NULL,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,this)
,	Resource(0)
,	OpenGLRHI(InOpenGLRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_OPENGLRESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,InPixelShader);

	VertexDeclaration = InVertexDeclaration;
	check(IsValidRef(VertexDeclaration));
	VertexShader = InVertexShader;
	InVertexShader->Compile();

	if (InPixelShader)
	{
		InPixelShader->Compile();
		PixelShader = InPixelShader;
	}
	else
	{
		// use special null pixel shader when PixelSahder was set to NULL
		FPixelShaderRHIParamRef NullPixelShaderRHI = TShaderMapRef<FNULLPixelShader>(GetGlobalShaderMap())->GetPixelShader();
		DYNAMIC_CAST_OPENGLRESOURCE(PixelShader,NullPixelShader);
		NullPixelShader->Compile();
		PixelShader = NullPixelShader;
	}

	Setup(InStreamStrides);
}

void FOpenGLBoundShaderState::Setup(DWORD* InStreamStrides)
{
	AddRef();

	Resource = GetGLSLProgram(VertexShader, PixelShader, InStreamStrides);

	if (OpenGLRHI->CachedState.Program != Resource)
	{
		glUseProgram(Resource);
		OpenGLRHI->CachedState.Program = Resource;
		CheckOpenGLErrors();
	}

	GLint NumActiveUniforms = 0;
	glGetProgramiv(Resource, GL_ACTIVE_UNIFORMS, &NumActiveUniforms);

	if (NumActiveUniforms > 0)
	{
		GLint MaxUniformNameLength = 0;
		glGetProgramiv(Resource, GL_ACTIVE_UNIFORM_MAX_LENGTH, &MaxUniformNameLength);

		GLchar *UniformName = (GLchar *)appMalloc(MaxUniformNameLength);

		for (GLint UniformIndex = 0; UniformIndex < NumActiveUniforms; UniformIndex++)
		{
			GLint UniformSize = 0;
			GLenum UniformType = GL_NONE;
			glGetActiveUniform(Resource, UniformIndex, MaxUniformNameLength, NULL, &UniformSize, &UniformType, UniformName);
			CheckOpenGLErrors();

			if (UniformType == GL_SAMPLER_1D || UniformType == GL_SAMPLER_2D
				|| UniformType == GL_SAMPLER_3D || UniformType == GL_SAMPLER_1D_SHADOW
				|| UniformType == GL_SAMPLER_2D_SHADOW || UniformType == GL_SAMPLER_CUBE)
			{
				for (INT SamplerIndex = 0; SamplerIndex < 16; SamplerIndex++)
				{
					FString SamplerName = FString::Printf(TEXT("PSampler%d"), SamplerIndex);
					if (SamplerName == ANSI_TO_TCHAR(UniformName))
					{
						GLint Location = glGetUniformLocation(Resource, UniformName);
						glUniform1i(Location, SamplerIndex);
						break;
					}
				}
			}
			else
			{
				SetupUniformArray(UniformName);
			}
		}

		appFree(UniformName);
	}
}

void FOpenGLBoundShaderState::SetupUniformArray(const GLchar *Name)
{
	INT ArrayNum = UniformArray_VLocal;
	const char *SearchFormat = "VConstFloat[%d]";
	INT ElementSize = sizeof(FVector4);

	if (strstr(Name, "VConstFloat"))
	{
	}
	else if (strstr(Name, "VConstGlobal"))
	{
		ArrayNum = UniformArray_VGlobal;
		SearchFormat = "VConstGlobal[%d]";
	}
	else if (strstr(Name, "VConstBones"))
	{
		ArrayNum = UniformArray_VBones;
		SearchFormat = "VConstBones[%d]";
	}
	else if (strstr(Name, "VConstBool"))
	{
		ArrayNum = UniformArray_VBool;
		SearchFormat = "VConstBool[%d]";
		ElementSize = sizeof(UINT);
	}
	else if (strstr(Name, "PConstFloat"))
	{
		ArrayNum = UniformArray_PLocal;
		SearchFormat = "PConstFloat[%d]";
	}
	else if (strstr(Name, "PConstGlobal"))
	{
		ArrayNum = UniformArray_PGlobal;
		SearchFormat = "PConstGlobal[%d]";
	}
	else if (strstr(Name, "PConstBool"))
	{
		ArrayNum = UniformArray_PBool;
		SearchFormat = "PConstBool[%d]";
		ElementSize = sizeof(UINT);
	}
	else
	{
		debugf(TEXT("Unhandled uniform %s in program %d"), ANSI_TO_TCHAR(Name), Resource);
		return;
	}

	FUniformArray &Array = UniformArrays[ArrayNum];
	Array.Location = glGetUniformLocation(Resource, Name);

	if (Array.Location != -1)
	{
		for (INT Index = 0; Index < 256; Index++)
		{
			char ConstText[255];
#if _WINDOWS
			sprintf_s(ConstText, 255, SearchFormat, Index);
#else
			snprintf(ConstText, 255, SearchFormat, Index);
#endif
			GLint ElementLocation = glGetUniformLocation(Resource, ConstText);
			if (ElementLocation == -1)
			{
				break;
			}
			else
			{
				Array.CacheSize += ElementSize;
			}
		}
		Array.Cache = appMalloc(Array.CacheSize);
		appMemzero(Array.Cache, Array.CacheSize);
	}
}

void FOpenGLBoundShaderState::Bind()
{
	if (OpenGLRHI->CachedState.Program != Resource)
	{
		glUseProgram(Resource);
		OpenGLRHI->CachedState.Program = Resource;
		CheckOpenGLErrors();
	}
}

void FOpenGLBoundShaderState::UpdateUniforms(INT ArrayNum, void *Data, UINT Size)
{
	GLint Location = UniformArrays[ArrayNum].Location;
	void *Cache = UniformArrays[ArrayNum].Cache;
	UINT CacheSize = UniformArrays[ArrayNum].CacheSize;
	Size = Min<UINT>(Size, CacheSize);

	if (Location != -1)
	{
		switch (ArrayNum)
		{
		case UniformArray_VLocal:
		case UniformArray_VGlobal:
		case UniformArray_VBones:
		case UniformArray_PLocal:
		case UniformArray_PGlobal:
		{
			UINT Count = Size / sizeof(FVector4);
			FLOAT *DataAddr = (FLOAT *)Data;
			FLOAT *CacheAddr = (FLOAT *)Cache;
			INT PastRegisterChanged = 0;

			for (UINT Index = 0; Index < Count; Index++)
			{
				// _WIN64 has ENABLE_VECTORINTRINSICS disabled at this moment, so we use SSE directly
				if (_mm_movemask_ps(_mm_cmpneq_ps(_mm_loadu_ps(CacheAddr ), _mm_loadu_ps(DataAddr))))
				{
					appMemcpy(CacheAddr, DataAddr, 4 * sizeof(FLOAT));
					PastRegisterChanged++;
				}
				else if (PastRegisterChanged)
				{
					glUniform4fv(Location - PastRegisterChanged, PastRegisterChanged, (const GLfloat *)DataAddr - (4 * PastRegisterChanged));
					PastRegisterChanged = 0;
				}
				DataAddr += 4;
				CacheAddr += 4;
				Location++;
			}

			if (PastRegisterChanged)
			{
				glUniform4fv(Location - PastRegisterChanged, PastRegisterChanged, (const GLfloat *)DataAddr - (4 * PastRegisterChanged));
			}
			break;
		}

		case UniformArray_VBool:
		case UniformArray_PBool:
			if (appMemcmp(Cache, Data, Size) != 0)
			{
				glUniform1iv(Location, Size / sizeof(UINT), (const GLint *)Data);
				appMemcpy(Cache, Data, Size);
			}
			break;
		}
	}
}
