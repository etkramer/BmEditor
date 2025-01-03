class MeleeWeaponInventoryManager extends InventoryManager;

function Inventory CreateInventoryArchetype(Inventory NewInventoryItemArchetype, optional bool bDoNotActivate)
{
	local Inventory	Inv;
	
	if (NewInventoryItemArchetype != none)
	{
		Inv = Spawn(NewInventoryItemArchetype.Class, Owner,,,, NewInventoryItemArchetype);

		if (Inv != none)
		{
			if (!AddInventory(Inv, bDoNotActivate))
			{
				Inv.Destroy();
				Inv = none;
			}
		}
	}

	return Inv;
}

DefaultProperties
{
	PendingFire(0)=0
}
