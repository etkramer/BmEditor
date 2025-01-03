class MeleeWeaponPlayerController extends UDKPlayerController;

function UpdateRotation(float DeltaTime)
{
	local MeleeWeaponPawn MWPawn;

	super.UpdateRotation(DeltaTime);

	MWPawn = MeleeWeaponPawn(self.Pawn);

	if (MWPawn != none)
	{
		MWPawn.CamPitch = Clamp(MWPawn.CamPitch + self.PlayerInput.aLookUp, -MWPawn.IsoCamAngle, MWPawn.IsoCamAngle);
	}
}

DefaultProperties
{
}