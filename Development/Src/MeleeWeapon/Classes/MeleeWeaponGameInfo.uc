class MeleeWeaponGameInfo extends UDKGame;

var() const archetype MeleeWeaponPawn PawnArchetype;
var() const archetype MeleeWeaponSword SwordArchetype;

function Pawn SpawnDefaultPawnFor(Controller NewPlayer, NavigationPoint StartSpot)
{
	local Pawn SpawnedPawn;

	if (NewPlayer == none || StartSpot == none)
	{
		return none;
	}

	SpawnedPawn = Spawn(PawnArchetype.Class,,, StartSpot.Location,, PawnArchetype);

	return SpawnedPawn;
}

event AddDefaultInventory(Pawn P)
{
	local MeleeWeaponInventoryManager MWInventoryManager;

	super.AddDefaultInventory(P);

	if (SwordArchetype != None)
	{
		MWInventoryManager = MeleeWeaponInventoryManager(P.InvManager);

		if (MWInventoryManager != None)
		{
			MWInventoryManager.CreateInventoryArchetype(SwordArchetype, false);
		}
	}
}

DefaultProperties
{
	PawnArchetype=MeleeWeaponPawn'MeleeWeaponContent.Archetypes.MeleeWeaponPawn'
	SwordArchetype=MeleeWeaponSword'MeleeWeaponContent.Archetypes.MeleeWeaponSword'
	PlayerControllerClass=class'MeleeWeapon.MeleeWeaponPlayerController'
}