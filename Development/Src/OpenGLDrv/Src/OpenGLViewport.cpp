/*=============================================================================
	OpenGLViewport.cpp: OpenGL viewport RHI implementation.
	Copyright 1998-2010 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"
#if PLATFORM_MACOSX
#include "MacObjCWrapper.h"
#endif

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FOpenGLDynamicRHI::CreateViewport(void* WindowHandle,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	check( IsInGameThread() );
	DefaultViewport = new FOpenGLViewport(this,WindowHandle,SizeX,SizeY,bIsFullscreen);
	CachedState.RenderTargetWidth = SizeX;
	CachedState.RenderTargetHeight = SizeY;
	CachedState.Viewport.Max.X = CachedState.Scissor.Max.X = SizeX;
	CachedState.Viewport.Max.Y = CachedState.Scissor.Max.Y = SizeY;
	return DefaultViewport.GetReference();
}

void FOpenGLDynamicRHI::ResizeViewport(FViewportRHIParamRef ViewportRHI,UINT SizeX,UINT SizeY,UBOOL bIsFullscreen)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	check( IsInGameThread() );
	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FOpenGLDynamicRHI::Tick( FLOAT DeltaTime )
{
}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FOpenGLDynamicRHI::BeginDrawingViewport(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);

	check(!DrawingViewport);
	DrawingViewport = Viewport;

	// update any resources that needed a deferred update
	FDeferredUpdateResource::UpdateResources();

	// Set the render target and viewport.
	RHISetRenderTarget(Viewport->GetBackBuffer(), FSurfaceRHIRef());
	RHISetViewport(0,0,0.0f,Viewport->GetSizeX(),Viewport->GetSizeY(),1.0f);
}

void FOpenGLDynamicRHI::EndDrawingViewport(FViewportRHIParamRef ViewportRHI,UBOOL bPresent,UBOOL bLockToVsync)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLPresentTime);

	check(DrawingViewport.GetReference() == Viewport);
	DrawingViewport = NULL;

	FOpenGLSurface *BackBuffer = Viewport->GetBackBuffer();

	// Determine start and end of drawing to compensate for CGL's rescaling of the window in fullscreen.
	FLOAT TargetPointStartX = -1.f;
	FLOAT TargetPointStartY = -1.f;
	FLOAT TargetPointEndX = 1.f;
	FLOAT TargetPointEndY = 1.f;
	FLOAT DisplaySizeX = (FLOAT)BackBuffer->SizeX;
	FLOAT DisplaySizeY = (FLOAT)BackBuffer->SizeY;
	GLenum TextureScaling = GL_NEAREST;
	UBOOL bAdjustmentForFullscreen = FALSE;
	if( Viewport->IsFullscreen() )
	{
#if PLATFORM_MACOSX
		MacGetDisplaySize( DisplaySizeX, DisplaySizeY );
#endif

		FLOAT BackBufferSizeX = (FLOAT)BackBuffer->SizeX;
		FLOAT BackBufferSizeY = (FLOAT)BackBuffer->SizeY;

		FLOAT ScaleX = DisplaySizeX / BackBufferSizeX;
		FLOAT ScaleY = DisplaySizeY / BackBufferSizeY;

		if( ScaleX > ScaleY )
		{
			FLOAT Displacement = 1.0 - ( ScaleY * BackBufferSizeX ) / DisplaySizeX;
			TargetPointStartX += Displacement;
			TargetPointEndX -= Displacement;
			TextureScaling = GL_LINEAR;
			bAdjustmentForFullscreen = TRUE;
		}
		else if( ScaleX < ScaleY )
		{
			FLOAT Displacement = 1.0 - ( ScaleX * BackBufferSizeY ) / DisplaySizeY;
			TargetPointStartY += Displacement;
			TargetPointEndY -= Displacement;
			TextureScaling = GL_LINEAR;
			bAdjustmentForFullscreen = TRUE;
		}
		else if( ScaleX != 1.0f )	// no need for position adjustments - aspect ration is kept. But GL_NEAREST might not be enough.
		{
			TextureScaling = GL_LINEAR;
			bAdjustmentForFullscreen = TRUE;
		}
	}

#if OPENGL_USE_BLIT_FOR_BACK_BUFFER
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	CachedSetDrawBuffer(GL_BACK);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, BackBuffer->Resource);
	CachedSetReadBuffer(GL_COLOR_ATTACHMENT0);

	glBlitFramebuffer(
		0, 0, BackBuffer->SizeX, BackBuffer->SizeY,
		0, 0, BackBuffer->SizeX, BackBuffer->SizeY,
		GL_COLOR_BUFFER_BIT,
		GL_NEAREST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	CachedSetDrawBuffer(GL_BACK);
	CachedSetReadBuffer(GL_BACK);
	CachedState.Framebuffer = (GLuint)-1;
#else
	if (CachedState.Program != 0)
	{
		glUseProgram(0);
	}
	if (CachedState.RasterizerState.CullMode != GL_NONE)
	{
		glDisable(GL_CULL_FACE);
	}
	if (CachedState.BlendState.bAlphaBlendEnable)
	{
		glDisable(GL_BLEND);
	}
	if (CachedState.BlendState.bAlphaTestEnable)
	{
		glDisable(GL_ALPHA_TEST);
	}
	if (CachedState.bScissorEnabled)
	{
		glDisable(GL_SCISSOR_TEST);
	}
	if (CachedState.ColorWriteEnabled != CW_RGBA)
	{
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	if (CachedState.DepthState.bZEnable)
	{
		glDisable(GL_DEPTH_TEST);
	}
	if (CachedState.DepthState.bZWriteEnable)
	{
		glDepthMask(GL_FALSE);
	}

	CachedBindArrayBuffer(0);
	CachedBindElementArrayBuffer(0);

	for (GLuint AttribIndex = 0; AttribIndex < NumVertexStreams; AttribIndex++)
	{
		glDisableVertexAttribArray(AttribIndex);
		CachedVertexAttrs[AttribIndex].Enabled = FALSE;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	CachedSetDrawBuffer(GL_BACK);
	CachedSetReadBuffer(GL_BACK);
	CachedState.Framebuffer = (GLuint)-1;
	FIntRect CachedViewport;
	if( bAdjustmentForFullscreen )
	{
		// Clear to black two stripes on the sides of the content, if necessary
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		if( TargetPointStartX > -1.f )
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(0.0f, 0.0f, ( 1.0 + TargetPointStartX ) * DisplaySizeX / 2.0f, DisplaySizeY );
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor( ( 1.0 + TargetPointEndX ) * DisplaySizeX / 2.0f, 0.0f, DisplaySizeX, DisplaySizeY );
			glClear(GL_COLOR_BUFFER_BIT);
			glDisable(GL_SCISSOR_TEST);
			glScissor(CachedState.Viewport.Min.X, CachedState.RenderTargetHeight - CachedState.Viewport.Max.Y, CachedState.Viewport.Max.X-CachedState.Viewport.Min.X, CachedState.Viewport.Max.Y-CachedState.Viewport.Min.Y);
		}
		else if( TargetPointStartY > -1.f )
		{
			glEnable(GL_SCISSOR_TEST);
			glScissor(0.0f, 0.0f, DisplaySizeX, ( 1.0 + TargetPointStartY ) * DisplaySizeY / 2.0f );
			glClear(GL_COLOR_BUFFER_BIT);
			glScissor( 0.0f, ( 1.0 + TargetPointEndY ) * DisplaySizeY / 2.0f, DisplaySizeX, DisplaySizeY );
			glClear(GL_COLOR_BUFFER_BIT);
			glDisable(GL_SCISSOR_TEST);
			glScissor(CachedState.Viewport.Min.X, CachedState.RenderTargetHeight - CachedState.Viewport.Max.Y, CachedState.Viewport.Max.X-CachedState.Viewport.Min.X, CachedState.Viewport.Max.Y-CachedState.Viewport.Min.Y);
		}

		// Adjust to draw on entire viewport
		if (DisplaySizeX != CachedState.RenderTargetWidth || DisplaySizeY != CachedState.RenderTargetHeight)
		{
			CachedViewport = CachedState.Viewport;
			DWORD TempRenderTargetHeight = CachedState.RenderTargetHeight;
			CachedState.RenderTargetHeight = DisplaySizeY;
			SetViewport( 0, 0, CachedState.MinZ, DisplaySizeX, DisplaySizeY, CachedState.MaxZ );
			CachedState.RenderTargetHeight = TempRenderTargetHeight;
		}
	}

	CachedSetActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, BackBuffer->Resource);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TextureScaling);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TextureScaling);

	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(TargetPointStartX, TargetPointEndY);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(TargetPointStartX, TargetPointStartY);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(TargetPointEndX, TargetPointStartY);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2f(TargetPointEndX, TargetPointEndY);
	glEnd();

	// Restore previous render state

	glDisable(GL_TEXTURE_2D);
	if (CachedState.Textures[0].Target != GL_NONE)
	{
		glBindTexture(CachedState.Textures[0].Target, CachedState.Textures[0].Resource);
	}

	if( bAdjustmentForFullscreen && ( (DisplaySizeX != CachedState.RenderTargetWidth || DisplaySizeY != CachedState.RenderTargetHeight) ) )
	{
		SetViewport( CachedViewport.Min.X, CachedViewport.Min.Y, CachedState.MinZ, CachedViewport.Max.X, CachedViewport.Max.Y, CachedState.MinZ );
	}

	if (CachedState.Program != 0)
	{
		glUseProgram(CachedState.Program);
	}
	if (CachedState.RasterizerState.CullMode != GL_NONE)
	{
		glEnable(GL_CULL_FACE);
	}
	if (CachedState.BlendState.bAlphaBlendEnable)
	{
		glEnable(GL_BLEND);
	}
	if (CachedState.BlendState.bAlphaTestEnable)
	{
		glEnable(GL_ALPHA_TEST);
	}
	if (CachedState.bScissorEnabled)
	{
		glEnable(GL_SCISSOR_TEST);
	}
	if (CachedState.DepthState.bZEnable)
	{
		glEnable(GL_DEPTH_TEST);
	}
	if (CachedState.DepthState.bZWriteEnable)
	{
		glDepthMask(GL_TRUE);
	}
	if (CachedState.ColorWriteEnabled != CW_RGBA)
	{
		glColorMask((CachedState.ColorWriteEnabled & CW_RED), (CachedState.ColorWriteEnabled & CW_GREEN), (CachedState.ColorWriteEnabled & CW_BLUE), (CachedState.ColorWriteEnabled & CW_ALPHA));
	}
#endif

	if(bPresent)
	{
		Viewport->SwapBuffers();
	}
}

/**
 * Determine if currently drawing the viewport
 *
 * @return TRUE if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
UBOOL FOpenGLDynamicRHI::IsDrawingViewport()
{
	return DrawingViewport != NULL;
}

void FOpenGLDynamicRHI::BeginScene()
{
}

void FOpenGLDynamicRHI::EndScene()
{
}

FSurfaceRHIRef FOpenGLDynamicRHI::GetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(Viewport,Viewport);

	return Viewport->GetBackBuffer();
}

/*=============================================================================
 *	FOpenGLViewport methods.
 *=============================================================================*/

FOpenGLSurface *FOpenGLViewport::CreateBackBufferSurface(FOpenGLDynamicRHI *RHI)
{
#if OPENGL_USE_BLIT_FOR_BACK_BUFFER
	return RHI->CreateSurface(SizeX, SizeY, PF_A16B16G16R16, 0, 0, GL_TEXTURE_2D);
#else
	FOpenGLTexture2D *Texture = RHI->CreateOpenGLTexture(SizeX, SizeY, FALSE, PF_A16B16G16R16, 1, TexCreate_ResolveTargetable);
	return RHI->CreateSurface(SizeX, SizeY, PF_A16B16G16R16, Texture->Resource, 0xFFFFFFFF, GL_TEXTURE_2D);
#endif
}
