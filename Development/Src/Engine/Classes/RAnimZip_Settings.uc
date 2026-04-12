/**
 * Compression settings for the AnimZip animation encoder.
 * Referenced from UAnimSet / UAnimSequence as `Compression_CustomSettings`.
 * When unset, the encoder falls back to this class's default object.
 *
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */

class RAnimZip_Settings extends Object
	native(Anim)
	editinlinenew
	hidecategories(Object);

enum EAnimZipPreset
{
	AZP_Default,
	AZP_Default50,
	AZP_Default25,
	AZP_Default10,
	AZP_AlmostNone,
	AZP_Custom,
};

enum EAnimZipRotationCodec
{
	AZRC_QuatMax_48,
	AZRC_QuatMax_40,
	AZRC_QuatRelative_32,
	AZRC_QuatRelative_24,
	AZRC_QuatRelative_16,
	AZRC_FixedAxis_16,
	AZRC_FixedAxis_8,
};

enum EAnimZipTranslationScaleCodec
{
	AZTSC_Float_128,
	AZTSC_NoScale_Float_96,
	AZTSC_NoScale_Interval_Fixed_48,
	AZTSC_NoScale_Interval_Fixed_24,
};

struct native AnimZipErrorBounds
{
	var() float Rotation;
	var() float Translation;
	var() float Scale;
};

struct native AnimZipTrackSettings
{
	var() AnimZipErrorBounds ErrorBounds;
	var() bool AllowRotationRetargeting;
};

struct native AnimZipNamedTrackSettings
{
	var() name TrackName;
	var() AnimZipTrackSettings Settings;
};

var() float CompressionAmount;

var(Advanced)	bool	StripTracksIfSameAsReferencePose;
var(Advanced)	bool	EnableAdaptiveDownsample;
var				bool	ForceDownsample_Enabled;
var(Advanced)	bool	EnableAdaptiveDownsampleEnergy;
var(Advanced)	bool	EnableRotationRetargeting;
var				bool	ForceRotationCodec_Enabled;
var				bool	ForceTranslationScaleCodec_Enabled;
var(Character)	bool	EnableCharacterOptimisations;
var(Cape)		bool	EnableCapeOptimisations;
var(Debug)	transient bool Log;

var(Advanced)	array<int>		AdaptiveDownsampleDivisors;
var(Advanced)	array<int>		AdaptiveDownsampleNumbers;
var(Advanced)	float			ForceDownsample;
var(Advanced)	float			AdaptiveDownsampleEnergyLowScale;
var(Advanced)	float			AdaptiveDownsampleEnergyHighScale;
var(Advanced)	float			AdaptiveDownsampleEnergyPower;

var(Advanced)	AnimZipTrackSettings					DefaultTrackSettings;
var(Advanced)	array<AnimZipNamedTrackSettings>		ForcedTrackSettings;
var(Advanced)	AnimZipTrackSettings					MotionTrackSettings;

var(Advanced)	EAnimZipRotationCodec					ForceRotationCodec;
var(Advanced)	EAnimZipTranslationScaleCodec			ForceTranslationScaleCodec;
var(Advanced)	array<EAnimZipRotationCodec>			DisableRotationCodecs;
var(Advanced)	array<EAnimZipTranslationScaleCodec>	DisableTranslationScaleCodecs;

var(Character)	AnimZipTrackSettings	CharacterTrackSettings;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Bip01;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Pelvis;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Spine;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Face;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Head;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Clavicle;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Arm;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Hand;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Finger;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Gundummy;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Leg;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Foot;
var(Character)	AnimZipTrackSettings	CharacterTrackSettings_Toe;

var(Cape)		AnimZipTrackSettings	CapeTrackSettings;

defaultproperties
{
	CompressionAmount=1.0

	StripTracksIfSameAsReferencePose=true
	EnableAdaptiveDownsample=true
	EnableAdaptiveDownsampleEnergy=true
	EnableRotationRetargeting=true
	EnableCharacterOptimisations=true
	EnableCapeOptimisations=true

	AdaptiveDownsampleDivisors(0)=8
	AdaptiveDownsampleDivisors(1)=7
	AdaptiveDownsampleDivisors(2)=6
	AdaptiveDownsampleDivisors(3)=5
	AdaptiveDownsampleDivisors(4)=4
	AdaptiveDownsampleDivisors(5)=3
	AdaptiveDownsampleDivisors(6)=2
	AdaptiveDownsampleNumbers(0)=1

	ForceDownsample=1.0
	AdaptiveDownsampleEnergyLowScale=1.0
	AdaptiveDownsampleEnergyHighScale=20.0
	AdaptiveDownsampleEnergyPower=0.1

	DefaultTrackSettings=(ErrorBounds=(Rotation=0.1,Translation=0.05,Scale=0.01),AllowRotationRetargeting=true)
	MotionTrackSettings=(ErrorBounds=(Rotation=0.5,Translation=0.2,Scale=0.01),AllowRotationRetargeting=false)

	CharacterTrackSettings=(ErrorBounds=(Rotation=0.2,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Bip01=(ErrorBounds=(Rotation=0.1,Translation=0.02,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Pelvis=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Spine=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Face=(ErrorBounds=(Rotation=0.05,Translation=0.05,Scale=0.01),AllowRotationRetargeting=false)
	CharacterTrackSettings_Head=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Clavicle=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Arm=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Hand=(ErrorBounds=(Rotation=0.2,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Finger=(ErrorBounds=(Rotation=3.0,Translation=0.1,Scale=0.01),AllowRotationRetargeting=false)
	CharacterTrackSettings_Gundummy=(ErrorBounds=(Rotation=0.2,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Leg=(ErrorBounds=(Rotation=0.1,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Foot=(ErrorBounds=(Rotation=0.2,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
	CharacterTrackSettings_Toe=(ErrorBounds=(Rotation=2.0,Translation=0.1,Scale=0.01),AllowRotationRetargeting=false)

	CapeTrackSettings=(ErrorBounds=(Rotation=0.6,Translation=0.1,Scale=0.01),AllowRotationRetargeting=true)
}
