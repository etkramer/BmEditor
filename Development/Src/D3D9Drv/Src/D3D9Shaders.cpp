/*=============================================================================
	D3D9Shaders.cpp: D3D shader RHI implementation.
	Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

FVertexShaderRHIRef FD3D9DynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<FD3D9VertexShader> VertexShader;
	VERIFYD3D9RESULT(Direct3DDevice->CreateVertexShader((DWORD*)&Code(0),(IDirect3DVertexShader9**)VertexShader.GetInitReference()));
	return VertexShader.GetReference();
}

FPixelShaderRHIRef FD3D9DynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<FD3D9PixelShader> PixelShader = NULL;
	VERIFYD3D9RESULT(Direct3DDevice->CreatePixelShader((DWORD*)&Code(0),(IDirect3DPixelShader9**)PixelShader.GetInitReference()));

#if BATMAN
	{
		// Reflect sampler usage via the D3DX constant table. We need the actual sampler register index
		// per name (D3DXGetShaderSamplers only gives names, in alphabetical order). Paired with
		// BmShaderInit in ShaderManager.cpp — log order gives correlation.
		LPD3DXCONSTANTTABLE ConstantTable = NULL;
		HRESULT hr = D3DXGetShaderConstantTable((CONST DWORD*)&Code(0), &ConstantTable);
		if (SUCCEEDED(hr) && ConstantTable)
		{
			D3DXCONSTANTTABLE_DESC TableDesc;
			ConstantTable->GetDesc(&TableDesc);
			FString RegList;
			INT SamplerCount = 0;
			for (UINT i = 0; i < TableDesc.Constants; ++i)
			{
				D3DXHANDLE Handle = ConstantTable->GetConstant(NULL, i);
				if (!Handle) continue;
				D3DXCONSTANT_DESC Desc;
				UINT Count = 1;
				if (FAILED(ConstantTable->GetConstantDesc(Handle, &Desc, &Count))) continue;
				if (Desc.RegisterSet != D3DXRS_SAMPLER) continue;
				if (SamplerCount > 0) RegList += TEXT(",");
				RegList += FString::Printf(TEXT("s%u=%s"), Desc.RegisterIndex, ANSI_TO_TCHAR(Desc.Name ? Desc.Name : "<null>"));
				++SamplerCount;
			}
			warnf(NAME_Warning, TEXT("BmShaderSamplers count=%d regs=[%s]"), SamplerCount, *RegList);
			ConstantTable->Release();
		}
		else
		{
			warnf(NAME_Warning, TEXT("BmShaderSamplers [GetConstantTable failed hr=0x%08x]"), hr);
		}

		// Dump pixel shader output semantics — shows which oC# registers (MRT slots) the shader writes to.
		{
			D3DXSEMANTIC OutSemantics[16];
			UINT NumSemantics = 16;
			HRESULT hr2 = D3DXGetShaderOutputSemantics((CONST DWORD*)&Code(0), OutSemantics, &NumSemantics);
			if (SUCCEEDED(hr2))
			{
				FString OutList;
				for (UINT i = 0; i < NumSemantics; ++i)
				{
					if (i > 0) OutList += TEXT(",");
					OutList += FString::Printf(TEXT("usage=%u idx=%u"), OutSemantics[i].Usage, OutSemantics[i].UsageIndex);
				}
				warnf(NAME_Warning, TEXT("BmShaderOutputs count=%u [%s]"), NumSemantics, *OutList);
			}
			else
			{
				warnf(NAME_Warning, TEXT("BmShaderOutputs [GetOutputSemantics failed hr=0x%08x]"), hr2);
			}
		}
	}
#endif

	return PixelShader.GetReference();
}

FD3D9BoundShaderState::FD3D9BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI
	):
	CacheLink(InVertexDeclarationRHI,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,this)
{
	DYNAMIC_CAST_D3D9RESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_D3D9RESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_D3D9RESOURCE(PixelShader,InPixelShader);

	VertexDeclaration = InVertexDeclaration;
	check(IsValidRef(VertexDeclaration));
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
}

/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param PixelShader - existing pixel shader
*/
FBoundShaderStateRHIRef FD3D9DynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI
	)
{
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
		return new FD3D9BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI);
	}
}
