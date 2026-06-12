/*=============================================================================
	D3D11Shaders.cpp: D3D shader RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"
#include "D3Dcompiler.h"

#if BATMAN
static const DWORD BM_DXBC_MAGIC = 0x43425844;

static INT BMDecompressShaderLZ4(const BYTE* Source, BYTE* Dest, INT DestSize)
{
	const BYTE* SourceStart = Source;
	BYTE* DestEnd = Dest + DestSize;

	for (;;)
	{
		BYTE Token = *Source++;
		INT LiteralLength = Token >> 4;
		if (LiteralLength == 15)
		{
			BYTE LengthByte;
			do
			{
				LengthByte = *Source++;
				LiteralLength += LengthByte;
			}
			while (LengthByte == 255);
		}

		if (Dest + LiteralLength > DestEnd)
		{
			return -1;
		}
		appMemcpy(Dest, Source, LiteralLength);
		Source += LiteralLength;
		Dest += LiteralLength;

		if (Dest >= DestEnd)
		{
			return Source - SourceStart;
		}

		INT Offset = Source[0] | (Source[1] << 8);
		Source += 2;
		BYTE* Match = Dest - Offset;

		INT MatchLength = Token & 15;
		if (MatchLength == 15)
		{
			BYTE LengthByte;
			do
			{
				LengthByte = *Source++;
				MatchLength += LengthByte;
			}
			while (LengthByte == 255);
		}
		MatchLength += 4;

		if (Dest + MatchLength > DestEnd)
		{
			return -1;
		}
		while (MatchLength-- > 0)
		{
			*Dest++ = *Match++;
		}
	}
}
#endif

static HRESULT TryDecompressShaderByteCode(const TArray<BYTE>& Code, TRefCountPtr<ID3DBlob>& DecompressedShader, TArray<BYTE>& ExpandedShader, const void*& pByteCode, SIZE_T& CodeSize)
{
#if BATMAN
	if (Code.Num() >= 4)
	{
		DWORD ExpandedSize = *(DWORD*)&Code(0);
		if (ExpandedSize == BM_DXBC_MAGIC)
		{
			pByteCode = &Code(0);
			CodeSize = Code.Num();
			return S_OK;
		}

		const BYTE* Payload = &Code(0) + 4;
		INT PayloadSize = Code.Num() - 4;

		if (ExpandedSize == 0)
		{
			pByteCode = Payload;
			CodeSize = PayloadSize;
			return S_OK;
		}

		if (PayloadSize >= 4)
		{
			if (*(DWORD*)Payload == 0x00B00B1E)
			{
				ExpandedShader.Empty((INT)ExpandedSize);
				ExpandedShader.Add((INT)ExpandedSize);
				INT DecompressedBytes = BMDecompressShaderLZ4(Payload + 4, &ExpandedShader(0), (INT)ExpandedSize);
				if (DecompressedBytes > 0)
				{
					pByteCode = &ExpandedShader(0);
					CodeSize = ExpandedSize;
					return S_OK;
				}
				ExpandedShader.Empty();
			}
			else if (*(WORD*)Payload == 0x9C78)
			{
				ExpandedShader.Empty((INT)ExpandedSize);
				ExpandedShader.Add((INT)ExpandedSize);
				if (appUncompressMemory(COMPRESS_ZLIB, &ExpandedShader(0), (INT)ExpandedSize, Payload, PayloadSize))
				{
					pByteCode = &ExpandedShader(0);
					CodeSize = ExpandedSize;
					return S_OK;
				}
				ExpandedShader.Empty();
			}
		}
	}
#endif
	UINT ShaderIndex = 0;
	UINT TotalShaders = 0;
	HRESULT DecompressResult = D3DDecompressShaders(&Code(0), Code.Num(), 1, 0, &ShaderIndex, 0, DecompressedShader.GetInitReference(), &TotalShaders);

	if (FAILED(DecompressResult))
	{
		pByteCode = &Code(0);
		CodeSize = Code.Num();
	}
	else
	{
		pByteCode = DecompressedShader->GetBufferPointer();
		CodeSize = DecompressedShader->GetBufferSize();
	}
	return DecompressResult;
}

FVertexShaderRHIRef FD3D11DynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TArray<BYTE> VertexShaderCode;
	const TArray<BYTE>* StoredCode = &Code;
	if (pByteCode != &Code(0) || CodeSize != (SIZE_T)Code.Num())
	{
		VertexShaderCode.Empty((INT)CodeSize);
		VertexShaderCode.Add((INT)CodeSize);
		appMemcpy(&VertexShaderCode(0), pByteCode, CodeSize);
		StoredCode = &VertexShaderCode;
	}
	TRefCountPtr<ID3D11VertexShader> VertexShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateVertexShader(pByteCode,CodeSize,NULL,VertexShader.GetInitReference()));
	return new FD3D11VertexShader(VertexShader,*StoredCode);
}

FPixelShaderRHIRef FD3D11DynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TRefCountPtr<FD3D11PixelShader> PixelShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreatePixelShader(pByteCode,CodeSize,NULL,(ID3D11PixelShader**)PixelShader.GetInitReference()));
	return PixelShader.GetReference();
}

FHullShaderRHIRef FD3D11DynamicRHI::CreateHullShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TRefCountPtr<FD3D11HullShader> HullShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateHullShader(pByteCode,CodeSize,NULL,(ID3D11HullShader**)HullShader.GetInitReference()));
	return HullShader.GetReference();
}

FDomainShaderRHIRef FD3D11DynamicRHI::CreateDomainShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TRefCountPtr<FD3D11DomainShader> DomainShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateDomainShader(pByteCode,CodeSize,NULL,(ID3D11DomainShader**)DomainShader.GetInitReference()));
	return DomainShader.GetReference();
}

FGeometryShaderRHIRef FD3D11DynamicRHI::CreateGeometryShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TRefCountPtr<FD3D11GeometryShader> Shader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateGeometryShader(pByteCode,CodeSize,NULL,(ID3D11GeometryShader**)Shader.GetInitReference()));
	return Shader.GetReference();
}

FComputeShaderRHIRef FD3D11DynamicRHI::CreateComputeShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	const void* pByteCode = NULL;
	SIZE_T CodeSize = 0;
	TRefCountPtr<ID3DBlob> DecompressedShader;
	TArray<BYTE> ExpandedShader;
	TryDecompressShaderByteCode(Code, DecompressedShader, ExpandedShader, pByteCode, CodeSize);
	TRefCountPtr<FD3D11ComputeShader> Shader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateComputeShader(pByteCode,CodeSize,NULL,(ID3D11ComputeShader**)Shader.GetInitReference()));
	return Shader.GetReference();
}

void FD3D11DynamicRHI::SetMultipleViewports(UINT Count, FViewPortBounds* Data) 
{ 
	check(Count > 0);
	check(Data);

	// structures are chosen to be directly mappable
	D3D11_VIEWPORT* D3DData = (D3D11_VIEWPORT*)Data;

	Direct3DDeviceIMContext->RSSetViewports(Count, D3DData);
}

FD3D11BoundShaderState::FD3D11BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	ID3D11Device* Direct3DDevice
	):
	CacheLink(InVertexDeclarationRHI,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	DYNAMIC_CAST_D3D11RESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_D3D11RESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_D3D11RESOURCE(PixelShader,InPixelShader);
	DYNAMIC_CAST_D3D11RESOURCE(HullShader,InHullShader);
	DYNAMIC_CAST_D3D11RESOURCE(DomainShader,InDomainShader);
	DYNAMIC_CAST_D3D11RESOURCE(GeometryShader,InGeometryShader);

	// Create an input layout for this combination of vertex declaration and vertex shader.
	VERIFYD3D11RESULT(Direct3DDevice->CreateInputLayout(
		&InVertexDeclaration->VertexElements(0),
		InVertexDeclaration->VertexElements.Num(),
		&InVertexShader->Code(0),
		InVertexShader->Code.Num(),
		InputLayout.GetInitReference()
		));

	VertexShader = InVertexShader->Resource;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param PixelShader - existing pixel shader
*/
FBoundShaderStateRHIRef FD3D11DynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI
	)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && Direct3DDeviceIMContext,(TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
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
		return new FD3D11BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI,NULL,NULL,NULL,Direct3DDevice);
	}
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param HullShader - existing hull shader
* @param DomainShader - existing domain shader
* @param PixelShader - existing pixel shader
* @param GeometryShader - existing geometry shader
*/
FBoundShaderStateRHIRef FD3D11DynamicRHI::CreateBoundShaderStateD3D11(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && Direct3DDeviceIMContext,(TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		StreamStrides,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FD3D11BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI,Direct3DDevice);
	}
}
