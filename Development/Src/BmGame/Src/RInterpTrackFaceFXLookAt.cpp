/*=============================================================================
	RInterpTrackFaceFXLookAt.cpp
	BM: Matinee track that drives the group actor's FaceFX look-at registers.
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS(URInterpTrackFaceFXLookAt);

INT URInterpTrackFaceFXLookAt::AddKeyframe(FLOAT Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode)
{
	INT NewKeyIndex = FloatTrack.AddPoint( Time, 0.f );
	FloatTrack.Points(NewKeyIndex).InterpMode = InitInterpMode;

	FloatTrack.AutoSetTangents(CurveTension);

	return NewKeyIndex;
}

void URInterpTrackFaceFXLookAt::PreviewUpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst)
{
	AActor* Actor = TrInst->GetGroupActor();
	if( Actor )
	{
		const FLOAT Weight = FloatTrack.Eval(NewPosition, 1.f);

		FLOAT Yaw, Pitch;
		if( GetLookAtRegisters(Actor, Target, Weight, Yaw, Pitch) )
		{
			Actor->PreviewSetFaceFXRegister(YawRegisterName, Yaw, FXREGISTEROWNER_MatineeLookAtTrack);
			Actor->PreviewSetFaceFXRegister(PitchRegisterName, Pitch, FXREGISTEROWNER_MatineeLookAtTrack);
		}
	}
}

void URInterpTrackFaceFXLookAt::UpdateTrack(FLOAT NewPosition, UInterpTrackInst* TrInst, UBOOL bJump)
{
	AActor* Actor = TrInst->GetGroupActor();
	if( Actor )
	{
		const FLOAT Weight = FloatTrack.Eval(NewPosition, 1.f);

		FLOAT Yaw, Pitch;
		if( GetLookAtRegisters(Actor, Target, Weight, Yaw, Pitch) )
		{
			Actor->eventMatineeSetFaceFXRegister(YawRegisterName, Yaw, FXREGISTEROWNER_MatineeLookAtTrack);
			Actor->eventMatineeSetFaceFXRegister(PitchRegisterName, Pitch, FXREGISTEROWNER_MatineeLookAtTrack);
		}
	}
}
