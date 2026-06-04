/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 *
 * Batman: Arkham City atmospheric fog post process effect (RockAtmos).
 * Two distance-based layers (D1/D2) and two height-based layers (H1/H2), plus
 * a global gradient and optional volume noise. Authored values here act as the
 * defaults that the world's FPostProcessSettings Atmos* overrides can replace.
 */
class RockAtmos extends DOFBloomMotionBlurEffect
	native;

var(Atmosphere) color  AtmosD1_Colour_PP;
var(Atmosphere) float  AtmosD1_Density_PP;
var(Atmosphere) float  AtmosD1_DistanceStart_PP;
var(Atmosphere) float  AtmosD1_DistanceEnd_PP;

var(Atmosphere) color  AtmosD2_Colour_PP;
var(Atmosphere) float  AtmosD2_Density_PP;
var(Atmosphere) float  AtmosD2_DistanceStart_PP;
var(Atmosphere) float  AtmosD2_DistanceEnd_PP;

var(Atmosphere) color  AtmosH1_Colour_PP;
var(Atmosphere) float  AtmosH1_Density_PP;
var(Atmosphere) float  AtmosH1_GradientSize_PP;
var(Atmosphere) float  AtmosH1_GradientPosition_PP;

var(Atmosphere) color  AtmosH2_Colour_PP;
var(Atmosphere) float  AtmosH2_Density_PP;
var(Atmosphere) float  AtmosH2_GradientSize_PP;
var(Atmosphere) float  AtmosH2_GradientPosition_PP;

var(Atmosphere) vector AtmosNoiseWind_PP;

var(Atmosphere) color  AtmosGlobal_Gradient_Colour_PP;
var(Atmosphere) vector AtmosGlobal_Gradient_Direction_PP;
var(Atmosphere) float  AtmosGlobal_Gradient_Density_PP;

var(Atmosphere) vector AtmosNoiseOffset_PP;
var(Atmosphere) float  AtmosNoiseFade_PP;

cpptext
{
	// UPostProcessEffect interface
	virtual class FPostProcessSceneProxy* CreateSceneProxy(const FPostProcessSettings* WorldSettings);

	// UObject interface
	virtual void PostLoad();
}

defaultproperties
{
	bShowInEditor=TRUE
	bShowInGame=TRUE
}
