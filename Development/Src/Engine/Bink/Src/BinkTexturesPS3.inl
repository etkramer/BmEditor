#if USE_BINK_CODEC


struct FBinkShader
{
	CGprogram	Program;
	void *		Microcode;
	UINT		MicrocodeSize;
	UINT		PixelShaderOffset;

	FBinkShader()
	{
		Init();
	}
	~FBinkShader()
	{
		Destroy();
	}

	void Init()
	{
		Program = 0;
		Microcode = NULL;
		MicrocodeSize = 0;
		PixelShaderOffset = 0;
	}

	UBOOL IsValid()
	{
		return (Microcode != NULL);
	}

	void CreateShaderFromFile( const TCHAR *Filename )
	{
		FArchive *Ar = GFileManager->CreateFileReader( Filename );
		checkf( Ar && Ar->TotalSize() > 0, TEXT("Can't open %s"), Filename );
		INT Size = Ar->TotalSize();
		Program = (CGprogram) appMalloc( Size );
		Ar->Serialize( Program, Size );
		cellGcmCgInitProgram( Program );
		delete Ar;

		// get pointer to the microcode
		cellGcmCgGetUCode(Program, &Microcode, &MicrocodeSize);
	}

	void CreateVertexShaderFromFile( const TCHAR *Filename )
	{
		CreateShaderFromFile( Filename );
	}

	void CreatePixelShaderFromFile( const TCHAR *Filename )
	{
		CreateShaderFromFile( Filename );

		// align the micrcocode memory to 64 bytes
		void* TempMicrocode = Microcode;
		Microcode = GPS3Gcm->GetLocalAllocator()->Malloc(MicrocodeSize, AT_PixelShader, 64);
		check( Microcode );
		cellGcmAddressToOffset(Microcode, &PixelShaderOffset);

		// copy microcode up to memory
		appMemcpy(Microcode, TempMicrocode, MicrocodeSize);
	}

	void Destroy()
	{
		if ( IsValid() )
		{
			if ( PixelShaderOffset )
			{
				GPS3Gcm->GetLocalAllocator()->Free(Microcode);
			}
			appFree( Program );
		}

		Init();
	}
};


struct FBinkVertex
{
	FVector2D Position;
	FVector2D TextureCoordinate;
};

//
// pointers to our local vertex and pixel shader
//

static FBinkShader VertexShader;
static FBinkShader YCrCbToRGB;
static FBinkShader YCrCbAToRGBA;
static FBinkVertex * GVertexBuffer = 0;
static UINT GVertexBufferOffset = 0;

static CGparameter AdjustParam = 0;

static CGparameter YplaneParam = 0;
static CGparameter cRplaneParam = 0;
static CGparameter cBplaneParam = 0;
static CGparameter AplaneParam = 0;
static CGparameter AlphaParam = 0;
static CGparameter YplaneNPAParam = 0;
static CGparameter cRplaneNPAParam = 0;
static CGparameter cBplaneNPAParam = 0;
static CGparameter AlphaNPAParam = 0;

//############################################################################
//##                                                                        ##
//## Free the shaders that we use.                                          ##
//##                                                                        ##
//############################################################################

void FreeBinkStuff()
{
	GPS3Gcm->BlockUntilGPUIdle();

	VertexShader.Destroy();
	YCrCbToRGB.Destroy();
	YCrCbAToRGBA.Destroy();
	if (GVertexBuffer)
	{
		GPS3Gcm->GetHostAllocator()->Free( GVertexBuffer );
	}
	GVertexBuffer = NULL;
	GVertexBufferOffset = 0;

	delete GFullScreenMovie;
	GFullScreenMovie = NULL;
}

void Free_Bink_shaders( void )
{
	FreeBinkStuff();
}

//############################################################################
//##                                                                        ##
//## Create the three shaders that we use.                                  ##
//##                                                                        ##
//############################################################################

S32 Create_Bink_shaders( void )
{
	VertexShader.CreateVertexShaderFromFile( TEXT("..\\Binaries\\PS3\\Bink\\BinkPS3_VertexShader.bin") );
	YCrCbToRGB.CreatePixelShaderFromFile( TEXT("..\\Binaries\\PS3\\Bink\\BinkPS3_YCrCbToRGB.bin") );
	YCrCbAToRGBA.CreatePixelShaderFromFile( TEXT("..\\Binaries\\PS3\\Bink\\BinkPS3_YCrCbAToRGBA.bin") );

	AdjustParam = cellGcmCgGetNamedParameter( VertexShader.Program, "Adjust" );

	YplaneNPAParam = cellGcmCgGetNamedParameter( YCrCbToRGB.Program, "YplaneNPA" );
	cRplaneNPAParam = cellGcmCgGetNamedParameter( YCrCbToRGB.Program, "cRplaneNPA" );
	cBplaneNPAParam = cellGcmCgGetNamedParameter( YCrCbToRGB.Program, "cBplaneNPA" );
	AlphaNPAParam = cellGcmCgGetNamedParameter( YCrCbToRGB.Program, "AlphaNPA" );

	YplaneParam = cellGcmCgGetNamedParameter( YCrCbAToRGBA.Program, "Yplane" );
	cRplaneParam = cellGcmCgGetNamedParameter( YCrCbAToRGBA.Program, "cRplane" );
	cBplaneParam = cellGcmCgGetNamedParameter( YCrCbAToRGBA.Program, "cBplane" );
	AplaneParam = cellGcmCgGetNamedParameter( YCrCbAToRGBA.Program, "Aplane" );
	AlphaParam = cellGcmCgGetNamedParameter( YCrCbAToRGBA.Program, "Alpha" );

	GVertexBuffer = (FBinkVertex*) GPS3Gcm->GetHostAllocator()->Malloc( 4 * sizeof(FBinkVertex), AT_VertexBuffer, 128 );
	cellGcmAddressToOffset(GVertexBuffer, &GVertexBufferOffset);
	check( GVertexBuffer );
	GVertexBuffer[0].Position = FVector2D(0,0);
	GVertexBuffer[1].Position = FVector2D(1,0);
	GVertexBuffer[2].Position = FVector2D(1,1);
	GVertexBuffer[3].Position = FVector2D(0,1);

	return( 1 );
}


//############################################################################
//##                                                                        ##
//## Create a texture while allocating the memory ourselves.                ##
//##                                                                        ##
//############################################################################

static S32 make_texture( U32 width, U32 height, CellGcmTexture * out_texture, DWORD *out_size, void ** out_ptr, U32 * out_pitch )
{
	// allocate the texture in host memory
	void *sTextureData = GPS3Gcm->GetHostAllocator(HostMem_Movies)->Malloc(width * height, AT_Texture, 128);
	if ( sTextureData == NULL )
		return 0;

	out_texture->format = CELL_GCM_TEXTURE_B8 | CELL_GCM_TEXTURE_LN;
	out_texture->mipmap = 1;
	out_texture->dimension = CELL_GCM_TEXTURE_DIMENSION_2;
	out_texture->cubemap = CELL_GCM_FALSE;
	out_texture->remap = CELL_GCM_REMAP_MODE(CELL_GCM_TEXTURE_REMAP_ORDER_XYXY,
										 CELL_GCM_TEXTURE_REMAP_FROM_A,
										 CELL_GCM_TEXTURE_REMAP_FROM_R,
										 CELL_GCM_TEXTURE_REMAP_FROM_G,
										 CELL_GCM_TEXTURE_REMAP_FROM_B,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP,
										 CELL_GCM_TEXTURE_REMAP_REMAP);
	out_texture->width = width;
	out_texture->height = height;
	out_texture->depth = 1;
	out_texture->location = CELL_GCM_LOCATION_MAIN;
	out_texture->pitch = width;
	cellGcmAddressToOffset(sTextureData, &out_texture->offset);

	*out_pitch = width;
	*out_ptr = sTextureData;
	*out_size = width * height;

	return( 1 );
}


//############################################################################
//##                                                                        ##
//## Free the textures that we allocated.                                   ##
//##                                                                        ##
//############################################################################

void Free_Bink_textures( FBinkTextureSet * set_textures )
{
	GPS3Gcm->BlockUntilGPUIdle();

	// free the texture memory and then the textures directly
	BINKFRAMETEXTURES *bt = set_textures->textures;
	for( INT i = 0 ; i < set_textures->bink_buffers.TotalFrames; i++ )
	{
		bt->fence = 0;
		if ( bt->Ytexture.offset )
		{
			void *Address = NULL;
			cellGcmIoOffsetToAddress( bt->Ytexture.offset, &Address );			
			GPS3Gcm->GetHostAllocator(HostMem_Movies)->Free( Address );
			bt->Ytexture.offset = 0;
		}
		if ( bt->cRtexture.offset )
		{
			void *Address = NULL;
			cellGcmIoOffsetToAddress( bt->cRtexture.offset, &Address );
			GPS3Gcm->GetHostAllocator(HostMem_Movies)->Free( Address );
			bt->cRtexture.offset = 0;
		}
		if ( bt->cBtexture.offset )
		{
			void *Address = NULL;
			cellGcmIoOffsetToAddress( bt->cBtexture.offset, &Address );
			GPS3Gcm->GetHostAllocator(HostMem_Movies)->Free( Address );
			bt->cBtexture.offset = 0;
		}
		if ( bt->Atexture.offset )
		{
			void *Address = NULL;
			cellGcmIoOffsetToAddress( bt->Atexture.offset, &Address );
			GPS3Gcm->GetHostAllocator(HostMem_Movies)->Free( Address );
			bt->Atexture.offset = 0;
		}
		++bt;
	}
}

//############################################################################
//##                                                                        ##
//## Create 2 sets of textures for Bink to decompress into...               ##
//##                                                                        ##
//############################################################################

S32 Create_Bink_textures( FBinkTextureSet * set_textures )
{
	//
	// Create our textures
	//

	BINKFRAMEBUFFERS &bb = set_textures->bink_buffers;
	for( INT i = 0; i < bb.TotalFrames; i++ )
	{
		BINKFRAMETEXTURES &bt = set_textures->textures[ i ];
		bt.Ytexture.offset = 0;
		bt.cBtexture.offset = 0;
		bt.cRtexture.offset = 0;
		bt.Atexture.offset = 0;

		if ( bb.Frames[ i ].YPlane.Allocate )
		{
			// create Y plane
			if ( !make_texture(bb.YABufferWidth, bb.YABufferHeight, &bt.Ytexture, &bt.Ysize,
				&bb.Frames[ i ].YPlane.Buffer, &bb.Frames[ i ].YPlane.BufferPitch) )
				goto fail;
		}

		if ( bb.Frames[ i ].cRPlane.Allocate )
		{
			// create cR plane
			if ( !make_texture(bb.cRcBBufferWidth, bb.cRcBBufferHeight, &bt.cRtexture, &bt.cRsize,
				&bb.Frames[ i ].cRPlane.Buffer, &bb.Frames[ i ].cRPlane.BufferPitch) )
				goto fail;
		}

		if ( bb.Frames[ i ].cBPlane.Allocate )
		{
			// create cB plane
			if ( !make_texture(bb.cRcBBufferWidth, bb.cRcBBufferHeight, &bt.cBtexture, &bt.cBsize,
				&bb.Frames[ i ].cBPlane.Buffer, &bb.Frames[ i ].cBPlane.BufferPitch) )
				goto fail;
		}

		if ( bb.Frames[ i ].APlane.Allocate )
		{
			// create alpha plane
			if ( !make_texture(bb.YABufferWidth, bb.YABufferHeight, &bt.Atexture, &bt.Asize,
				&bb.Frames[ i ].APlane.Buffer, &bb.Frames[ i ].APlane.BufferPitch) )
				goto fail;
		}  
	}

	return( 1 );
  
fail:

	Free_Bink_textures( set_textures );
	return( 0 );
}


//############################################################################
//##                                                                        ##
//## Draw our textures onto the screen with our vertex and pixel shaders.   ##
//##                                                                        ##
//############################################################################

static void Set_Texture( CGprogram PixelShader, CGparameter Parameter, CellGcmTexture *Texture )
{
	DWORD TexUnitIndex = cellGcmCgGetParameterResource(PixelShader, Parameter) - CG_TEXUNIT0;
	cellGcmSetTexture( TexUnitIndex, Texture );
	cellGcmSetTextureControl(TexUnitIndex, CELL_GCM_TRUE, 0, 0, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:0,ANISO:1
	cellGcmSetTextureFilter(TexUnitIndex, 0, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	cellGcmSetTextureAddress(TexUnitIndex, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, CELL_GCM_TEXTURE_ZFUNC_LESS, 0);
}

static void Disable_Texture( CGprogram PixelShader, CGparameter Parameter )
{
	DWORD TexUnitIndex = cellGcmCgGetParameterResource(PixelShader, Parameter) - CG_TEXUNIT0;
	cellGcmSetTextureControl(TexUnitIndex, CELL_GCM_FALSE, 0, 0, CELL_GCM_TEXTURE_MAX_ANISO_1); // MIN:0,MAX:0,ANISO:1
}

void Draw_Bink_textures( FBinkTextureSet * set_textures,
                         U32 width,
                         U32 height,
                         F32 x_offset,
                         F32 y_offset,
                         F32 x_scale,
                         F32 y_scale,
                         F32 alpha_level,
                         UBOOL is_premultiplied_alpha,
						 UBOOL bIsFullscreen)
{
	// Set vertex shader
	extern FPS3RHIVertexShader* GCurrentVertexShader;
	GCurrentVertexShader = NULL;
	cellGcmSetVertexProgram( VertexShader.Program, VertexShader.Microcode );

	// Set vertex attribute
	cellGcmSetVertexDataArray(
		0,						// POSITION
		0,						// Frequency
		sizeof(FBinkVertex),	// Stride
		2,						// Num "components" for this attribute
		CELL_GCM_VERTEX_F,		// Data type of this attribute
		CELL_GCM_LOCATION_MAIN,	// Location of memory
		GVertexBufferOffset);	// Offset of verts into that memory

	//
	// turn off Z buffering, culling, and projection (since we are drawing orthographically)
	//
	cellGcmSetDepthTestEnable( CELL_GCM_FALSE );
	cellGcmSetCullFaceEnable( CELL_GCM_FALSE );

	// Set scale and bias for the rect
	FLOAT Adjust[4];
	Adjust[0] = -1.0f;
	Adjust[1] = +1.0f;
	Adjust[2] = 2.0f;
	Adjust[3] = -2.0f;
	cellGcmSetVertexProgramParameter( AdjustParam, Adjust );

	//
	// are we using an alpha plane? if so, turn on the 4th texture and set our pixel shader
	//
	if ( set_textures->textures[ set_textures->bink_buffers.FrameNum ].Atexture.offset )
	{
		//
		// setup our pixel shader
		//
		Set_Texture( YCrCbAToRGBA.Program, YplaneParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].Ytexture );
		Set_Texture( YCrCbAToRGBA.Program, cRplaneParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].cRtexture );
		Set_Texture( YCrCbAToRGBA.Program, cBplaneParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].cBtexture );
		Set_Texture( YCrCbAToRGBA.Program, AplaneParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].Atexture );

		cellGcmSetFragmentProgramParameter( YCrCbAToRGBA.Program, AlphaParam, &alpha_level, YCrCbAToRGBA.PixelShaderOffset );

		cellGcmSetFragmentProgram( YCrCbAToRGBA.Program, YCrCbAToRGBA.PixelShaderOffset );
		if (bIsFullscreen)
		{
			goto do_alpha;
		}
	}
	else
	{
		//
		// setup our pixel shader
		//
		Set_Texture( YCrCbToRGB.Program, YplaneNPAParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].Ytexture );
		Set_Texture( YCrCbToRGB.Program, cRplaneNPAParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].cRtexture );
		Set_Texture( YCrCbToRGB.Program, cBplaneNPAParam, &set_textures->textures[ set_textures->bink_buffers.FrameNum ].cBtexture );

		cellGcmSetFragmentProgramParameter( YCrCbToRGB.Program, AlphaNPAParam, &alpha_level, YCrCbToRGB.PixelShaderOffset );

		cellGcmSetFragmentProgram( YCrCbToRGB.Program, YCrCbToRGB.PixelShaderOffset );
	}

	
	// for render-to-texture movies, we don't need to blend here, as the texture/material will do the 
	// normal UE3 blending; we just need to pass alpha through
	if (!bIsFullscreen || alpha_level >= 0.999f )
	{
		cellGcmSetBlendEnable( CELL_GCM_FALSE );
	}
	else
	{
do_alpha:
		cellGcmSetBlendEnable( CELL_GCM_TRUE );

		if (is_premultiplied_alpha)
		{
			cellGcmSetBlendFunc( CELL_GCM_ONE, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_ZERO );
		}
		else
		{
			cellGcmSetBlendFunc( CELL_GCM_SRC_ALPHA, CELL_GCM_ONE_MINUS_SRC_ALPHA, CELL_GCM_ONE, CELL_GCM_ZERO );
		}
	}
  
	// Draw the quad
	cellGcmSetDrawArrays( CELL_GCM_PRIMITIVE_QUADS, 0, 4 );

	//
	// set a fence so that we know when we are done
	//
	set_textures->textures[ set_textures->bink_buffers.FrameNum ].fence = GPS3Gcm->InsertFence();

	// Disable textures
	if ( set_textures->textures[ set_textures->bink_buffers.FrameNum ].Atexture.offset )
	{
		Disable_Texture( YCrCbAToRGBA.Program, YplaneParam );
		Disable_Texture( YCrCbAToRGBA.Program, cRplaneParam );
		Disable_Texture( YCrCbAToRGBA.Program, cBplaneParam );
		Disable_Texture( YCrCbAToRGBA.Program, AplaneParam );
	}
	else
	{
		Disable_Texture( YCrCbToRGB.Program, YplaneNPAParam );
		Disable_Texture( YCrCbToRGB.Program, cRplaneNPAParam );
		Disable_Texture( YCrCbToRGB.Program, cBplaneNPAParam );
	}
}


//
// simple routine to store the cache of a memory range
//
static void store_cache( void * ptr, U32 size )
{
	for ( U32 i = 0 ; i < size ; i += 128 )
	{
		__dcbst((U8*)ptr + i);
	}
}

// sync the textures out to main memory, so that the GPU can see them (this is fast)
RADDEFFUNC void Sync_Bink_textures( FBinkTextureSet * set_textures )
{
	// store out the cache
	INT i = set_textures->bink_buffers.FrameNum;
	store_cache( set_textures->bink_buffers.Frames[ i ].YPlane.Buffer, set_textures->textures[ i ].Ysize );
	store_cache( set_textures->bink_buffers.Frames[ i ].cRPlane.Buffer, set_textures->textures[ i ].cRsize );
	store_cache( set_textures->bink_buffers.Frames[ i ].cBPlane.Buffer, set_textures->textures[ i ].cBsize );
	store_cache( set_textures->bink_buffers.Frames[ i ].APlane.Buffer, set_textures->textures[ i ].Asize );
}

// make sure the GPU is done with the textures that we are about to write info
RADDEFFUNC void Wait_for_Bink_textures( FBinkTextureSet * set_textures )
{
	S32 next = set_textures->bink_buffers.FrameNum + 1;
	if ( ( next >= set_textures->bink_buffers.TotalFrames ) || ( set_textures->textures[ next ].Ytexture.offset == 0 ) )
		next = 0;

	if ( set_textures->textures[ next ].fence )
	{
		//
		// block until the texture is ready
		//
		GPS3Gcm->BlockOnFence( set_textures->textures[ next ].fence );
	}
}




/*************************************************************
 * Non-resource, PS3-specific Bink support
 ************************************************************/

INT GRenderThreadPriorityDuringStreaming = 1001;

/**
 * Set the rendering thread priority as needed for starting or stopping Bink playback
 */
void appPS3BinkSetRenderingThreadPriority(UBOOL bIsStartingMovie)
{
	// @todo: Is this necessary if using SPU Bink?

	// cache the previous value of the RT priority so we can restore it when the movie ends
	static INT RenderThreadPriorityDuringStreamingPrevious = 0;

	// Jack up the rendering thread priority while rendering movies
	extern FRunnableThread* GRenderingThread;
	if (bIsStartingMovie)
	{
		sys_ppu_thread_get_priority(GRenderingThread->GetThreadID(), &RenderThreadPriorityDuringStreamingPrevious);
		sys_ppu_thread_set_priority(GRenderingThread->GetThreadID(), GRenderThreadPriorityDuringStreaming);
		debugfSuppressed(NAME_DevMovie, TEXT("Starting Bink movie. Setting render thread prio = %d. Prev = %d"), GRenderThreadPriorityDuringStreaming, RenderThreadPriorityDuringStreamingPrevious );
	}
	else
	{
		// Restore the rendering thread priority
		sys_ppu_thread_set_priority(GRenderingThread->GetThreadID(), RenderThreadPriorityDuringStreamingPrevious);
		debugfSuppressed(NAME_DevMovie, TEXT("Ending Bink movie. Restoring render thread prio = %d."), RenderThreadPriorityDuringStreamingPrevious );
	}
}


#endif //USE_BINK_CODEC
