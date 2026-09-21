/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * Used to affect post process settings in the game and editor.
 */
class PostProcessVolume extends Volume
	native
	placeable
	hidecategories(Advanced,Collision,Volume);


/**  LUT Blender for efficient Color Grading (LUT: color look up table, RGB_new = LUT[RGB_old]) blender. */
struct native LUTBlender
{
	// is emptied at end of each frame, each value stored need to be unique, the value 0 is used for neutral
	var array<Texture> LUTTextures;
	// is emptied at end of each frame
	var array<float> LUTWeights;
	// BM
	var native const transient bool bHasChanged;

	structcpptext
	{
		/** constructor, by default not even the Neutral element is defined */
		FLUTBlender();

		UBOOL IsLUTEmpty() const;

		/** new = lerp(old, Rhs, Weight) (main thread only)
		*
		* @param Texture 0 is a valid entry and is used for neutral
		* @param Weight 0..1
		*/
		void LerpTo(UTexture* Texture, float Weight);

		/** resolve to one LUT (render thread only)*/
		const FTextureRHIRef ResolveLUT(class FViewInfo& View, const struct ColorTransformMaterialProperties& ColorTransform);

		/** Is updated every frame if GColorGrading is set to debug mode, empty if not */
		static UBOOL GetDebugInfo(FString& Out);

		void CopyToRenderThread(FLUTBlender& Dest) const;

		/**
		 * Check if the parameters are different, compared to the previous LUT Blender parameters.
		 */
		void CheckForChanges( const FLUTBlender& PreviousLUTBlender );

		/**
		* Clean the container and adds the neutral LUT.
		* should be called after the render thread copied the data
		*/
		void Reset();

	private:

		/**
		*
		* @param Texture 0 is used for the neutral LUT
		*/
		void SetLUT(UTexture *Texture);

		/**
		* add a LUT to the ones that are blended together
		*
		* @param Texture can be 0 then the call is ignored
		* @param Weight 0..1
		*/
		void PushLUT(UTexture* Texture, float Weight);

		/** @return 0xffffffff if not found */
		UINT FindIndex(UTexture* Tex) const;

		/** @return count */
		UINT GenerateFinalTable(FTexture* OutTextures[], float OutWeights[], UINT MaxCount) const;
	}
};

// BM: declaration order, types and keywords are AK's (retail _Engine.upk PropertyFlags);
// the struct is 524 bytes and every self[0xNNN] of AK's structdefaultproperties lands on a field.
struct native PostProcessSettings
{
	var					vector			levelOffsetCached;

	var	bool			bOverride_InterpolateOverDistance;
	var	bool			bOverride_InterpolateOverDistanceFade;
	var	bool			bOverride_bEnableHighQualityDOF;
	var	bool			bOverride_EnableBloom;
	var	bool			bOverride_EnableDOF;
	var	bool			bOverride_EnableMotionBlur;
	var	bool			bOverride_EnableSceneEffect;
	var	bool			bOverride_AllowAmbientOcclusion;
	var	bool			bOverride_BloomOverload;
	var	bool			bOverride_BloomLowerCut;
	var	bool			bOverride_DOF_ApertureStop;
	var	bool			bOverride_DOF_FocusDistance;
	var	bool			bOverride_DOF_InterpolationDuration;
	var	bool			bOverride_MotionBlur_MaxVelocity;
	var	bool			bOverride_MotionBlur_Amount;
	var	bool			bOverride_MotionBlur_FullMotionBlur;
	var	bool			bOverride_MotionBlur_CameraRotationThreshold;
	var	bool			bOverride_MotionBlur_CameraTranslationThreshold;
	var	bool			bOverride_MotionBlur_InterpolationDuration;
	var	bool			bOverride_Scene_Desaturation;
	var	bool			bOverride_Scene_Colorize;
	var	bool			bOverride_Scene_ImageGrainScale;
	var	bool			bOverride_Scene_HighLights;
	var	bool			bOverride_Scene_MidTones;
	var	bool			bOverride_Scene_Shadows;
	var	bool			bOverride_Scene_InterpolationDuration;
	var	bool			bOverride_Scene_ColorGradingLUT;

	var()				bool			bEnableInterpolateOverDistance<editcondition=bOverride_InterpolateOverDistance>;
	var()	interp		float			InterpolateOverDistanceFade<editcondition=bOverride_InterpolateOverDistanceFade>;

	var	bool			bOverride_MobileColorGrading;

	/** Whether to use bloom effect. */
	var(Bloom)	bool			bEnableBloom<editcondition=bOverride_EnableBloom>;
	/** Whether to use depth of field effect. */
	var(DepthOfField)	bool	bEnableDOF<editcondition=bOverride_EnableDOF>;
	/** Whether to use high quality DOF. */
	var(DepthOfField)	bool	bEnableHighQualityDOF<editcondition=bOverride_bEnableHighQualityDOF>;
	/** Whether to use motion blur effect. */
	var(MotionBlur)	bool		bEnableMotionBlur<editcondition=bOverride_EnableMotionBlur>;
	/** Whether to use the material/ scene effect. */
	var(Scene)	bool			bEnableSceneEffect<editcondition=bOverride_EnableSceneEffect>;
	/** Whether to allow ambient occlusion. */
	var()	bool				bAllowAmbientOcclusion<editcondition=bOverride_AllowAmbientOcclusion>;

	// --- Atmospherics ---
	var	bool			bOverride_EnableAtmosD1;
	var	bool			bAtmosD1;
	var	bool			bOverride_EnableAtmosD1Col;
	var()	Color		AtmosD1_Colour;
	var	bool			bOverride_EnableAtmosD1Den;
	var()	interp float	AtmosD1_Density;
	var	bool			bOverride_EnableAtmosD1Start;
	var()	interp float	AtmosD1_DistanceStart;
	var	bool			bOverride_EnableAtmosD1End;
	var()	interp float	AtmosD1_DistanceEnd;

	var	bool			bOverride_EnableAtmosD2;
	var	bool			bAtmosD2;
	var	bool			bOverride_EnableAtmosD2Col;
	var()	Color		AtmosD2_Colour;
	var	bool			bOverride_EnableAtmosD2Den;
	var()	interp float	AtmosD2_Density;
	var	bool			bOverride_EnableAtmosD2Start;
	var()	interp float	AtmosD2_DistanceStart;
	var	bool			bOverride_EnableAtmosD2End;
	var()	interp float	AtmosD2_DistanceEnd;

	var	bool			bOverride_EnableAtmosH1;
	var	bool			bAtmosH1;
	var	bool			bOverride_EnableAtmosH1Col;
	var()	Color		AtmosH1_Colour;
	var	bool			bOverride_EnableAtmosH1Den;
	var()	interp float	AtmosH1_Density;
	var	bool			bOverride_EnableAtmosH1Size;
	var()	interp float	AtmosH1_GradientSize;
	var	bool			bOverride_EnableAtmosH1Pos;
	var()	interp float	AtmosH1_GradientPosition;

	var	bool			bOverride_EnableAtmosH2;
	var	bool			bAtmosH2;
	var	bool			bOverride_EnableAtmosH2Col;
	var()	Color		AtmosH2_Colour;
	var	bool			bOverride_EnableAtmosH2Den;
	var()	interp float	AtmosH2_Density;
	var	bool			bOverride_EnableAtmosH2Size;
	var()	interp float	AtmosH2_GradientSize;
	var	bool			bOverride_EnableAtmosH2Pos;
	var()	interp float	AtmosH2_GradientPosition;

	var	bool			bOverride_EnableAtmosGlobal_Gradient_Colour;
	var()	Color		AtmosGlobal_Gradient_Colour;
	var	bool			bOverride_EnableAtmosGlobal_Gradient_Direction;
	var()	interp Vector	AtmosGlobal_Gradient_Direction;
	var	bool			bOverride_EnableAtmosGlobal_Gradient_Density;
	var()	interp float	AtmosGlobal_Gradient_Density;
	var	bool			bOverride_EnableAtmosGlobal_Gradient_Cosine;
	var()	interp float	AtmosGlobal_Gradient_Cosine;

	var	bool			bOverride_EnableAtmosHazeWeight;
	var	bool			bOverride_EnableAtmosHazeNear;
	var	bool			bOverride_EnableAtmosHazeFar;
	var()	interp float	AtmosHazeWeight;
	var()	interp float	AtmosHazeNear;
	var()	interp float	AtmosHazeFar;

	var	bool			bOverride_EnableAtmosAmbientD1;
	var	bool			bOverride_EnableAtmosAmbientD2;
	var	bool			bOverride_EnableAtmosAmbientH1;
	var	bool			bOverride_EnableAtmosAmbientH2;
	var()	interp LinearColor	AtmosAmbientD1;
	var()	interp LinearColor	AtmosAmbientD2;
	var()	interp LinearColor	AtmosAmbientH1;
	var()	interp LinearColor	AtmosAmbientH2;

	var	bool			bOverride_EnableAtmosNoiseD1;
	var	bool			bOverride_EnableAtmosNoiseD2;
	var	bool			bOverride_EnableAtmosNoiseH1;
	var	bool			bOverride_EnableAtmosNoiseH2;
	var()	interp float	AtmosNoiseD1;
	var()	interp float	AtmosNoiseD2;
	var()	interp float	AtmosNoiseH1;
	var()	interp float	AtmosNoiseH2;

	var	bool			bOverride_EnableAtmosHeightMapModD1;
	var	bool			bOverride_EnableAtmosHeightMapModD2;
	var	bool			bOverride_EnableAtmosHeightMapModH1;
	var	bool			bOverride_EnableAtmosHeightMapModH2;
	var()	interp Vector	AtmosHeightMapModD1;
	var()	interp Vector	AtmosHeightMapModD2;
	var()	interp Vector	AtmosHeightMapModH1;
	var()	interp Vector	AtmosHeightMapModH2;

	var	bool			bOverride_ExposureAutoBracketing;
	var()	interp float	ExposureAutoBracketing;
	var	bool			bOverride_ExposureBaseOffset;
	var()	interp float	ExposureBaseOffset;

	/** Amount of energy pushed past the bloom threshold. */
	var(Bloom)	interp float	BloomOverload<editcondition=bOverride_BloomOverload>;
	/** Lower cut of the bloom response curve. */
	var(Bloom)	interp float	BloomLowerCut<editcondition=bOverride_BloomLowerCut>;

	/** Physical aperture (f-stop) driving the depth of field. */
	var(RockDepthOfField)	interp float	DOF_ApertureStop<editcondition=bOverride_DOF_ApertureStop>;
	/** Used when FOCUS_Distance is enabled. */
	var(DepthOfField)	interp float	DOF_FocusDistance<editcondition=bOverride_DOF_FocusDistance>;
	/** Duration over which to interpolate values to. */
	var(DepthOfField)	float		DOF_InterpolationDuration<editcondition=bOverride_DOF_InterpolationDuration>;

	/** Maximum blur velocity amount.  This is a clamp on the amount of blur. */
	var(MotionBlur)	interp float	MotionBlur_MaxVelocity<editcondition=bOverride_MotionBlur_MaxVelocity>;
	/** This is a scalar on the blur */
	var(MotionBlur)	interp float	MotionBlur_Amount<editcondition=bOverride_MotionBlur_Amount>;
	/** Whether everything (static/dynamic objects) should motion blur or not. If disabled, only moving objects may blur. */
	var(MotionBlur)	bool			MotionBlur_FullMotionBlur<editcondition=bOverride_MotionBlur_FullMotionBlur>;
	/** Threshold for when to turn off motion blur when the camera rotates swiftly during a single frame (in degrees). */
	var(MotionBlur)	interp float	MotionBlur_CameraRotationThreshold<editcondition=bOverride_MotionBlur_CameraRotationThreshold>;
	/** Threshold for when to turn off motion blur when the camera translates swiftly during a single frame (in world units). */
	var(MotionBlur)	interp float	MotionBlur_CameraTranslationThreshold<editcondition=bOverride_MotionBlur_CameraTranslationThreshold>;
	/** Duration over which to interpolate values to. */
	var(MotionBlur)	float			MotionBlur_InterpolationDuration<editcondition=bOverride_MotionBlur_InterpolationDuration>;

	/** Desaturation amount. */
	var(Scene)	interp float	Scene_Desaturation<editcondition=bOverride_Scene_Desaturation>;
	/** Colorize (color tint after desaturate) */
	var(Scene)	interp vector	Scene_Colorize<editcondition=bOverride_Scene_Colorize>;
	/** Image grain scale, only affects the darks, >=0, 0:none, 1(strong) should be less than 1 */
	var(Scene)	interp float	Scene_ImageGrainScale<editcondition=bOverride_Scene_ImageGrainScale>;
	/** Controlling white point. */
	var(Scene)	interp LinearColor	Scene_HighLights<editcondition=bOverride_Scene_HighLights>;
	/** Controlling gamma curve. */
	var(Scene)	interp LinearColor	Scene_MidTones<editcondition=bOverride_Scene_MidTones>;
	/** Controlling black point. */
	var(Scene)	interp LinearColor	Scene_Shadows<editcondition=bOverride_Scene_Shadows>;
	/** Duration over which to interpolate values to. */
	var(Scene)	float			Scene_InterpolationDuration<editcondition=bOverride_Scene_InterpolationDuration>;

	/** Name of the LUT texture e.g. MyPackage01.LUTNeutral, empty if not used */
	var(Scene)	Texture			ColorGrading_LookupTable<editcondition=bOverride_Scene_ColorGradingLUT>;
	/** Used to blend color grading LUT in a very similar way we blend scalars */
	var	const private transient	LUTBlender ColorGradingLUT;

	var	bool			bOverride_CompositeViewModeBeforeBlur;
	var	bool			bCompositeViewModeBeforeBlur;

	structcpptext
	{
		/* default constructor, for script, values are overwritten by serialization after that */
		FPostProcessSettings()
		{}

		/* second constructor, supposed to be used by C++ */
		FPostProcessSettings(INT A)
		{
			bOverride_InterpolateOverDistance = FALSE;
			bOverride_InterpolateOverDistanceFade = FALSE;
			bOverride_bEnableHighQualityDOF = TRUE;
			bOverride_EnableBloom = TRUE;
			bOverride_EnableDOF = TRUE;
			bOverride_EnableMotionBlur = TRUE;
			bOverride_EnableSceneEffect = TRUE;
			bOverride_AllowAmbientOcclusion = TRUE;

			bOverride_BloomOverload = TRUE;
			bOverride_BloomLowerCut = TRUE;

			bOverride_DOF_ApertureStop = TRUE;
			bOverride_DOF_FocusDistance = TRUE;
			bOverride_DOF_InterpolationDuration = TRUE;

			bOverride_MotionBlur_MaxVelocity = FALSE;
			bOverride_MotionBlur_Amount = FALSE;
			bOverride_MotionBlur_FullMotionBlur = FALSE;
			bOverride_MotionBlur_CameraRotationThreshold = FALSE;
			bOverride_MotionBlur_CameraTranslationThreshold = FALSE;
			bOverride_MotionBlur_InterpolationDuration = FALSE;
			bOverride_Scene_Desaturation = TRUE;
			bOverride_Scene_Colorize = FALSE;
			bOverride_Scene_ImageGrainScale = FALSE;
			bOverride_Scene_HighLights = TRUE;
			bOverride_Scene_MidTones = TRUE;
			bOverride_Scene_Shadows = TRUE;
			bOverride_Scene_InterpolationDuration = TRUE;
			bOverride_Scene_ColorGradingLUT = FALSE;

			bEnableInterpolateOverDistance=FALSE;
			InterpolateOverDistanceFade=2500.0f;

			bOverride_MobileColorGrading = FALSE;

			bEnableBloom=TRUE;
			bEnableDOF=FALSE;
			bEnableHighQualityDOF=FALSE;
			bEnableMotionBlur=TRUE;
			bEnableSceneEffect=TRUE;
			bAllowAmbientOcclusion=TRUE;

			BloomOverload=0.05f;
			BloomLowerCut=0.01f;

			DOF_ApertureStop=22.0f;
			DOF_FocusDistance=220.0f;
			DOF_InterpolationDuration=1;

			MotionBlur_MaxVelocity=1.0f;
			MotionBlur_Amount=0.5f;
			MotionBlur_FullMotionBlur=TRUE;
			MotionBlur_CameraRotationThreshold=45.0f;
			MotionBlur_CameraTranslationThreshold=10000.0f;
			MotionBlur_InterpolationDuration=1;

			Scene_Desaturation=0;
			Scene_Colorize=FVector(1,1,1);
			Scene_ImageGrainScale=0.0f;
			Scene_HighLights=FLinearColor(1,1,1,1);
			Scene_MidTones=FLinearColor(0.5f,0.5f,0.5f,1);
			Scene_Shadows=FLinearColor(0,0,0,1);
			Scene_InterpolationDuration=1;
		}

		/**
		 * Blends the settings on this structure marked as override setting onto the given settings
		 *
		 * @param	ToOverride	The settings that get overridden by the overridable settings on this structure.
		 * @param	Alpha		The opacity of these settings. If Alpha is 1, ToOverride will equal this setting structure.
		 */
		void OverrideSettingsFor( FPostProcessSettings& ToOverride, FLOAT Alpha=1.f ) const;

		/**
		 * Enables the override setting for the given post-process setting.
		 *
		 * @param	PropertyName	The post-process property name to enable.
		 */
		void EnableOverrideSetting( const FName& PropertyName );

		/**
		 * Disables the override setting for the given post-process setting.
		 *
		 * @param	PropertyName	The post-process property name to enable.
		 */
		void DisableOverrideSetting( const FName& PropertyName );

		/**
		 * Sets all override values to false, which prevents overriding of this struct.
		 *
		 * @note	Overrides can be enabled again.
		 */
		void DisableAllOverrides();

		/**
		 * Enables bloom for the post process settings.
		 */
		FORCEINLINE void EnableBloom()
		{
			bOverride_EnableBloom = TRUE;
			bEnableBloom = TRUE;
		}

		/**
		 * Enables DOF for the post process settings.
		 */
		FORCEINLINE void EnableDOF()
		{
			bOverride_EnableDOF = TRUE;
			bEnableDOF = TRUE;
		}

		/**
		 * Enables motion blur for the post process settings.
		 */
		FORCEINLINE void EnableMotionBlur()
		{
			bOverride_EnableMotionBlur = TRUE;
			bEnableMotionBlur = TRUE;
		}

		/**
		 * Enables scene effects for the post process settings.
		 */
		FORCEINLINE void EnableSceneEffect()
		{
			bOverride_EnableSceneEffect = TRUE;
			bEnableSceneEffect = TRUE;
		}

		/**
		 * Disables the override to enable bloom if no overrides are set for bloom settings.
		 */
		void DisableBloomOverrideConditional();

		/**
		 * Disables the override to enable DOF if no overrides are set for DOF settings.
		 */
		void DisableDOFOverrideConditional();

		/**
		 * Disables the override to enable motion blur if no overrides are set for motion blur settings.
		 */
		void DisableMotionBlurOverrideConditional();

		/**
		 * Disables the override to enable scene effect if no overrides are set for scene effect settings.
		 */
		void DisableSceneEffectOverrideConditional();
	}

	/**
	 * Used when a volume is placed in editor but also when the local player is deserialized
	 * (e.g. after seamless map cycle - this caused TTP 162775)
	 */
	structdefaultproperties
	{
		bOverride_InterpolateOverDistance=FALSE
		bOverride_InterpolateOverDistanceFade=FALSE
		bOverride_bEnableHighQualityDOF=TRUE
		bOverride_EnableBloom=TRUE
		bOverride_EnableDOF=TRUE
		bOverride_EnableMotionBlur=TRUE
		bOverride_EnableSceneEffect=TRUE
		bOverride_AllowAmbientOcclusion=TRUE
		bOverride_BloomOverload=TRUE
		bOverride_BloomLowerCut=TRUE
		bOverride_DOF_ApertureStop=TRUE
		bOverride_DOF_FocusDistance=TRUE
		bOverride_DOF_InterpolationDuration=TRUE
		bOverride_MotionBlur_MaxVelocity=FALSE
		bOverride_MotionBlur_Amount=FALSE
		bOverride_MotionBlur_FullMotionBlur=FALSE
		bOverride_MotionBlur_CameraRotationThreshold=FALSE
		bOverride_MotionBlur_CameraTranslationThreshold=FALSE
		bOverride_MotionBlur_InterpolationDuration=FALSE
		bOverride_Scene_Desaturation=TRUE
		bOverride_Scene_Colorize=FALSE
		bOverride_Scene_ImageGrainScale=FALSE
		bOverride_Scene_HighLights=TRUE
		bOverride_Scene_MidTones=TRUE
		bOverride_Scene_Shadows=TRUE
		bOverride_Scene_InterpolationDuration=TRUE
		bOverride_Scene_ColorGradingLUT=FALSE

		bEnableInterpolateOverDistance=FALSE
		InterpolateOverDistanceFade=2500.0

		bOverride_MobileColorGrading=FALSE

		bEnableBloom=TRUE
		bEnableDOF=FALSE
		bEnableHighQualityDOF=FALSE
		bEnableMotionBlur=TRUE
		bEnableSceneEffect=TRUE
		bAllowAmbientOcclusion=TRUE

		bOverride_EnableAtmosD1=TRUE
		bAtmosD1=TRUE
		bOverride_EnableAtmosD1Col=TRUE
		AtmosD1_Colour=(R=227,G=170,B=113,A=255)
		bOverride_EnableAtmosD1Den=TRUE
		AtmosD1_Density=0.0
		bOverride_EnableAtmosD1Start=TRUE
		AtmosD1_DistanceStart=500.0
		bOverride_EnableAtmosD1End=TRUE
		AtmosD1_DistanceEnd=10000.0

		bOverride_EnableAtmosD2=TRUE
		bAtmosD2=TRUE
		bOverride_EnableAtmosD2Col=TRUE
		AtmosD2_Colour=(R=227,G=170,B=113,A=255)
		bOverride_EnableAtmosD2Den=TRUE
		AtmosD2_Density=0.0
		bOverride_EnableAtmosD2Start=TRUE
		AtmosD2_DistanceStart=500.0
		bOverride_EnableAtmosD2End=TRUE
		AtmosD2_DistanceEnd=10000.0

		bOverride_EnableAtmosH1=TRUE
		bAtmosH1=TRUE
		bOverride_EnableAtmosH1Col=TRUE
		AtmosH1_Colour=(R=227,G=170,B=113,A=255)
		bOverride_EnableAtmosH1Den=TRUE
		AtmosH1_Density=0.0
		bOverride_EnableAtmosH1Size=TRUE
		AtmosH1_GradientSize=500.0
		bOverride_EnableAtmosH1Pos=TRUE
		AtmosH1_GradientPosition=1000.0

		bOverride_EnableAtmosH2=TRUE
		bAtmosH2=TRUE
		bOverride_EnableAtmosH2Col=TRUE
		AtmosH2_Colour=(R=227,G=170,B=113,A=255)
		bOverride_EnableAtmosH2Den=TRUE
		AtmosH2_Density=0.0
		bOverride_EnableAtmosH2Size=TRUE
		AtmosH2_GradientSize=500.0
		bOverride_EnableAtmosH2Pos=TRUE
		AtmosH2_GradientPosition=1000.0

		bOverride_EnableAtmosGlobal_Gradient_Colour=TRUE
		AtmosGlobal_Gradient_Colour=(R=255,G=255,B=255,A=255)
		bOverride_EnableAtmosGlobal_Gradient_Direction=TRUE
		AtmosGlobal_Gradient_Direction=(X=1.0,Y=0.0,Z=0.0)
		bOverride_EnableAtmosGlobal_Gradient_Density=TRUE
		AtmosGlobal_Gradient_Density=0.0
		bOverride_EnableAtmosGlobal_Gradient_Cosine=TRUE
		AtmosGlobal_Gradient_Cosine=8.0

		bOverride_EnableAtmosHazeWeight=TRUE
		bOverride_EnableAtmosHazeNear=TRUE
		bOverride_EnableAtmosHazeFar=TRUE
		AtmosHazeWeight=0.0
		AtmosHazeNear=500.0
		AtmosHazeFar=10000.0

		bOverride_EnableAtmosAmbientD1=TRUE
		bOverride_EnableAtmosAmbientD2=TRUE
		bOverride_EnableAtmosAmbientH1=TRUE
		bOverride_EnableAtmosAmbientH2=TRUE
		AtmosAmbientD1=(R=0.0,G=0.0,B=0.0,A=1.0)
		AtmosAmbientD2=(R=0.0,G=0.0,B=0.0,A=1.0)
		AtmosAmbientH1=(R=0.0,G=0.0,B=0.0,A=1.0)
		AtmosAmbientH2=(R=0.0,G=0.0,B=0.0,A=1.0)

		bOverride_EnableAtmosNoiseD1=TRUE
		bOverride_EnableAtmosNoiseD2=TRUE
		bOverride_EnableAtmosNoiseH1=TRUE
		bOverride_EnableAtmosNoiseH2=TRUE
		AtmosNoiseD1=0.0
		AtmosNoiseD2=0.0
		AtmosNoiseH1=0.0
		AtmosNoiseH2=0.0

		bOverride_EnableAtmosHeightMapModD1=TRUE
		bOverride_EnableAtmosHeightMapModD2=TRUE
		bOverride_EnableAtmosHeightMapModH1=TRUE
		bOverride_EnableAtmosHeightMapModH2=TRUE
		AtmosHeightMapModD1=(X=0.0,Y=0.0,Z=1.0)
		AtmosHeightMapModD2=(X=0.0,Y=0.0,Z=1.0)
		AtmosHeightMapModH1=(X=0.0,Y=0.0,Z=1.0)
		AtmosHeightMapModH2=(X=0.0,Y=0.0,Z=1.0)

		bOverride_ExposureAutoBracketing=TRUE
		ExposureAutoBracketing=4.0
		bOverride_ExposureBaseOffset=TRUE
		ExposureBaseOffset=1.0

		BloomOverload=0.05
		BloomLowerCut=0.01

		DOF_ApertureStop=22.0
		DOF_FocusDistance=220.0
		DOF_InterpolationDuration=1

		MotionBlur_MaxVelocity=1.0
		MotionBlur_Amount=0.5
		MotionBlur_FullMotionBlur=TRUE
		MotionBlur_CameraRotationThreshold=45.0
		MotionBlur_CameraTranslationThreshold=10000.0
		MotionBlur_InterpolationDuration=1

		Scene_Desaturation=0
		Scene_Colorize=(X=1,Y=1,Z=1)
		Scene_ImageGrainScale=0.0
		Scene_HighLights=(R=1.0,G=1.0,B=1.0,A=1.0)
		Scene_MidTones=(R=0.5,G=0.5,B=0.5,A=1.0)
		Scene_Shadows=(R=0.0,G=0.0,B=0.0,A=1.0)
		Scene_InterpolationDuration=1

		bOverride_CompositeViewModeBeforeBlur=FALSE
		bCompositeViewModeBeforeBlur=FALSE
	}

};

/**
 * Priority of this volume. In the case of overlapping volumes the one with the highest priority
 * is chosen. The order is undefined if two or more overlapping volumes have the same priority.
 */
var()							float					Priority;

// BM
var()							bool					bOverrideWorldPostProcessChain;

/** Whether this volume is enabled or not. */
var()							bool					bEnabled;

/**
 * Post process settings to use for this volume.
 */
var()							PostProcessSettings		Settings;

/** Next volume in linked listed, sorted by priority in descending order. */
var const noimport transient	PostProcessVolume		NextLowerPriorityVolume;

// BM
var transient					vector					LevelOffset;

replication
{
	if (bNetDirty)
		bEnabled;
}

/**
 * Kismet support for toggling bDisabled.
 */
simulated function OnToggle(SeqAct_Toggle action)
{
	if (action.InputLinks[0].bHasImpulse)
	{
		// "Turn On" -- mapped to enabling of volume.
		bEnabled = TRUE;
	}
	else if (action.InputLinks[1].bHasImpulse)
	{
		// "Turn Off" -- mapped to disabling of volume.
		bEnabled = FALSE;
	}
	else if (action.InputLinks[2].bHasImpulse)
	{
		// "Toggle"
		bEnabled = !bEnabled;
	}
	ForceNetRelevant();
	SetForcedInitialReplicatedProperty(Property'Engine.PostProcessVolume.bEnabled', (bEnabled == default.bEnabled));
}

cpptext
{
	/**
	 * Routes ClearComponents call to Super and removes volume from linked list in world info.
	 */
	virtual void ClearComponents();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();
protected:
	/**
	 * Routes UpdateComponents call to Super and adds volume to linked list in world info.
	 */
	virtual void UpdateComponentsInternal(UBOOL bCollisionUpdate = FALSE);
public:
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=False
		BlockActors=False
		BlockZeroExtent=False
		BlockNonZeroExtent=False
		BlockRigidBody=False
	End Object

	bCollideActors=False
	bBlockActors=False
	bProjTarget=False
	bStatic=false
	bTickIsDisabled=true

	SupportedEvents.Empty
	SupportedEvents(0)=class'SeqEvent_Touch'

	bEnabled=True
}
