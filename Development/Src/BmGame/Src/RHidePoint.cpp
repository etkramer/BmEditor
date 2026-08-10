/*=============================================================================
	RHidePoint.cpp
	BM: Native implementation for BM2's RHidePoint.

	AutoAdjust rebuilds a point's swing links whenever it is moved or edited, which
	is what populates the HidePoints array the swing prompt is gated on. Ported 1:1
	from ARHidePoint::AutoAdjust; the swing-arc config floats are resolved by name
	rather than by the offsets the reference uses, since those disagree with the
	script layout by one slot.
=============================================================================*/

#include "BmGame.h"

IMPLEMENT_CLASS_EXTENSION(ARHidePoint, "BmGame.RHidePoint");

TExtensionProperty<UObject*>			ARHidePoint::SwingMoveProp;
TExtensionProperty<FLOAT>				ARHidePoint::VantagePointRangeProp;
TExtensionProperty<TArray<FHideLink> >	ARHidePoint::HidePointsProp;
TExtensionProperty<TArray<FString> >	ARHidePoint::BlockedHidePointsProp;
TExtensionProperty<ARHidePoint*>		ARHidePoint::LinkedCrossLevelProp[6];
TExtensionProperty<ARHidePoint*>		ARHidePoint::LinkedSameLevelProp;
TExtensionProperty<BYTE>				ARHidePoint::HidePointCrossLevelProp;
TExtensionProperty<USpriteComponent*>	ARHidePoint::GoodSpriteProp;
TExtensionProperty<USpriteComponent*>	ARHidePoint::BadSpriteProp;
FExtensionBoolProperty					ARHidePoint::ValidHidePointProp;
FExtensionBoolProperty					ARHidePoint::AutoAdjustOnPathBuildProp;

// BM: these two carry bits our ETraceFlags doesn't know about (0x1000000 / 0x8000000).
#define RHIDEPOINT_TRACE_LINKS		0x1002486
#define RHIDEPOINT_TRACE_APEX		0x80020C6

/** Script classes we only need to identify, so they're resolved by name instead of linked against. */
static UClass* FindScriptClass( const TCHAR* PathName )
{
	return FindObject<UClass>(NULL, PathName);
}

/** The swing arc is described by the candidate's SwingMove, optionally behind a Configurable wrapper. */
static UObject* GetSwingToVantagePointConfig( UObject* SwingMove )
{
	static UClass* ConfigurableClass = NULL;
	static UClass* SwingClass = NULL;
	static TExtensionProperty<UObject*> ConfigProp;

	if( !SwingMove )
	{
		return NULL;
	}

	if( !ConfigurableClass )
	{
		ConfigurableClass = FindScriptClass(TEXT("BmGame.RSpecialMoveConfigConfigurable"));
		SwingClass = FindScriptClass(TEXT("BmGame.RSpecialMoveConfig_SwingToVantagePoint"));
		if( !ConfigurableClass || !SwingClass )
		{
			return NULL;
		}
		ConfigProp.Bind(ConfigurableClass, TEXT("Config"));
	}

	UObject* Config = SwingMove;
	if( SwingMove->IsA(ConfigurableClass) && ConfigProp.IsBound() )
	{
		Config = ConfigProp(SwingMove);
	}

	return (Config && Config->IsA(SwingClass)) ? Config : NULL;
}

/** Extent used for the link and apex traces - the player's collision cylinder. */
static UBOOL GetPlayerCylinderExtent( FVector& OutExtent, AActor*& OutPlayerDefault )
{
	static UClass* PawnClass = NULL;
	static TExtensionProperty<UCylinderComponent*> CylinderProp;

	if( !PawnClass )
	{
		PawnClass = FindScriptClass(TEXT("BmGame.RPawnPlayer"));
		if( !PawnClass )
		{
			return FALSE;
		}
		CylinderProp.Bind(PawnClass, TEXT("CylinderComponent"));
	}

	if( !CylinderProp.IsBound() )
	{
		return FALSE;
	}

	OutPlayerDefault = Cast<AActor>(PawnClass->GetDefaultObject());
	UCylinderComponent* Cylinder = OutPlayerDefault ? CylinderProp(OutPlayerDefault) : NULL;
	if( !Cylinder )
	{
		return FALSE;
	}

	OutExtent = FVector(Cylinder->CollisionRadius, Cylinder->CollisionRadius, Cylinder->CollisionHeight);
	return TRUE;
}

UBOOL ARHidePoint::BindProperties()
{
	UClass* Cls = GetClass();

	SwingMoveProp.Bind(Cls, TEXT("SwingMove"));
	VantagePointRangeProp.Bind(Cls, TEXT("VantagePointRange"));
	HidePointsProp.Bind(Cls, TEXT("HidePoints"));

	// TExtensionProperty only sizes the array itself, so check FHideLink against the loaded struct.
	UArrayProperty* HidePointsArray = FindField<UArrayProperty>(Cls, TEXT("HidePoints"));
	checkf(!HidePointsArray || HidePointsArray->Inner->ElementSize == sizeof(FHideLink),
		TEXT("HideLink is %i bytes in script, expected %i"), HidePointsArray->Inner->ElementSize, (INT)sizeof(FHideLink));

	BlockedHidePointsProp.Bind(Cls, TEXT("BlockedHidePoints"));
	LinkedSameLevelProp.Bind(Cls, TEXT("LinkedHidePointsSameLevel"), 6);
	HidePointCrossLevelProp.Bind(Cls, TEXT("HidePointCrossLevel"), 6);
	GoodSpriteProp.Bind(Cls, TEXT("GoodSprite"));
	BadSpriteProp.Bind(Cls, TEXT("BadSprite"));
	ValidHidePointProp.Bind(Cls, TEXT("ValidHidePoint"));
	AutoAdjustOnPathBuildProp.Bind(Cls, TEXT("AutoAdjustOnPathBuild"));

	for( INT I=0; I<6; I++ )
	{
		LinkedCrossLevelProp[I].Bind(Cls, *FString::Printf(TEXT("LinkedHidePointsCrossLevel%i"), I+1));
	}

	UBOOL bCrossLevelBound = TRUE;
	for( INT I=0; I<6; I++ )
	{
		bCrossLevelBound = bCrossLevelBound && LinkedCrossLevelProp[I].IsBound();
	}

	return SwingMoveProp.IsBound() && VantagePointRangeProp.IsBound() && HidePointsProp.IsBound()
		&& LinkedSameLevelProp.IsBound() && HidePointCrossLevelProp.IsBound() && bCrossLevelBound;
}

ARHidePoint* ARHidePoint::GetLinkedHidePoint( INT I )
{
	check(I >= 0 && I < 6);

	if( !BindProperties() )
	{
		return NULL;
	}

	return HidePointCrossLevelProp(this, I) ? LinkedCrossLevelProp[I](this) : LinkedSameLevelProp(this, I);
}

UBOOL ARHidePoint::IsHidePointLinked( ARHidePoint* TestPoint )
{
	if( !TestPoint || !BindProperties() || !SwingMoveProp(TestPoint) )
	{
		return FALSE;
	}

	for( INT I=0; I<HidePoints().Num(); I++ )
	{
		if( GetLinkedHidePoint(I) == TestPoint )
		{
			return TRUE;
		}
	}

	return FALSE;
}

void ARHidePoint::UpdateIcons()
{
	if( !BindProperties() || !ValidHidePointProp.IsBound() )
	{
		return;
	}

	const UBOOL bValid = ValidHidePointProp(this);

	USpriteComponent* Good = GoodSpriteProp.IsBound() ? GoodSpriteProp(this) : NULL;
	if( Good )
	{
		Good->SetHiddenEditor(!bValid);
	}

	USpriteComponent* Bad = BadSpriteProp.IsBound() ? BadSpriteProp(this) : NULL;
	if( Bad )
	{
		Bad->SetHiddenEditor(bValid);
	}
}

void ARHidePoint::PostEditMove( UBOOL bFinished )
{
	AutoAdjust(bFinished);
	UpdateIcons();
	Super::PostEditMove(bFinished);
}

void ARHidePoint::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	AutoAdjust(TRUE);
	UpdateIcons();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ARHidePoint::CheckForErrors()
{
	if( BindProperties() )
	{
		for( INT I=0; I<HidePoints().Num(); I++ )
		{
			ARHidePoint* Linked = GetLinkedHidePoint(I);
			if( Linked && (Linked->Location - Location).SizeSquared() < 10000.f )
			{
				GWarn->MapCheck_Add(MCTYPE_ERROR, this,
					*FString::Printf(TEXT("%s : 2 linked hide points too close to each other.  This will cause a crash!"), *GetName()),
					MCACTION_DELETE, TEXT("HidePointInvalid"));
			}
		}
	}

	Super::CheckForErrors();
}

void ARHidePoint::AutoAdjust( UBOOL bAdjustNeighbours )
{
	static UClass* HidePointClass = NULL;
	static UClass* HidePointMeshClass = NULL;
	if( !HidePointClass )
	{
		HidePointClass = FindScriptClass(TEXT("BmGame.RHidePoint"));
		HidePointMeshClass = FindScriptClass(TEXT("BmGame.RHidePoint_Mesh"));
	}

	if( !GWorld || !HidePointClass || !BindProperties() )
	{
		return;
	}

	ULevel* MyLevel = Cast<ULevel>(GetOuter());

	HidePoints().Empty();
	for( INT I=0; I<6; I++ )
	{
		LinkedCrossLevelProp[I](this) = NULL;
		LinkedSameLevelProp(this, I) = NULL;
		HidePointCrossLevelProp(this, I) = 0;
	}

	FVector Extent;
	AActor* PlayerDefault = NULL;

	// Everything below needs a swing move and the player extent; the neighbour pass still runs without them.
	if( SwingMove() && GetPlayerCylinderExtent(Extent, PlayerDefault) )
	{
		UBOOL bBlockedBehind = FALSE;
		if( HidePointMeshClass && IsA(HidePointMeshClass) )
		{
			FCheckResult Hit(1.f);
			const FVector Behind = Location - Rotation.Vector() * 16.f;
			bBlockedBehind = !GWorld->SinglePointCheck(Hit, Behind, Extent, RHIDEPOINT_TRACE_LINKS, this);
		}

		if( !bBlockedBehind )
		{
			BlockedHidePoints().Empty();

			for( FActorIterator It; It; ++It )
			{
				AActor* Candidate = *It;
				if( !Candidate->IsA(HidePointClass) || Candidate == this )
				{
					continue;
				}

				ARHidePoint* Other = (ARHidePoint*)Candidate;
				if( !SwingMoveProp(Other) )
				{
					continue;
				}

				const FVector Delta = Other->Location - Location;
				const FLOAT Dist = Delta.Size();
				const FVector Dir = Delta / Dist;

				if( VantagePointRangeProp(this) < Dist || Abs(Dir.Z) >= 0.5f )
				{
					continue;
				}

				FMemMark Mark(GMainThreadMemStack);

				// A candidate is only reachable if nothing but foliage sits between the two points.
				FCheckResult* Blocker = NULL;
				for( FCheckResult* Hit=GWorld->MultiLineCheck(GMainThreadMemStack, Other->Location, Location, Extent, RHIDEPOINT_TRACE_LINKS, this); Hit; Hit=Hit->GetNext() )
				{
					const FString OuterName = Hit->Actor ? Hit->Actor->GetOutermost()->GetName() : FString();
					if( appStristr(*OuterName, TEXT("_Plant")) == NULL )
					{
						Blocker = Hit;
						break;
					}
				}

				if( Blocker )
				{
					if( Blocker->Actor )
					{
						BlockedHidePoints().AddItem(FString::Printf(TEXT("%s - blocked by %s (time %s)"),
							*Other->GetPathName(), *Blocker->Actor->GetPathName(), *appSystemTimeString()));
					}
					continue;
				}

				// Collapse links that point in near enough the same direction, keeping the nearer one.
				UBOOL bSkip = FALSE;
				INT Replace = INDEX_NONE;
				const INT NumLinks = HidePoints().Num();
				const FLOAT CosLimit = appCos(0.1745329201221466f);
				for( INT I=0; I<NumLinks; I++ )
				{
					ARHidePoint* Existing = GetLinkedHidePoint(I);
					const FVector ExistingDelta = Existing->Location - Location;
					const FLOAT ExistingDist = ExistingDelta.Size();

					if( ((ExistingDelta / ExistingDist) | Dir) > CosLimit )
					{
						if( Dist - 200.f > ExistingDist )
						{
							bSkip = TRUE;
							break;
						}
						if( ExistingDist - 200.f > Dist )
						{
							Replace = I;
							break;
						}
					}
				}

				if( bSkip )
				{
					continue;
				}

				if( Replace != INDEX_NONE )
				{
					for( INT I=Replace; I<5; I++ )
					{
						LinkedCrossLevelProp[I](this) = LinkedCrossLevelProp[I+1](this);
						LinkedSameLevelProp(this, I) = LinkedSameLevelProp(this, I+1);
						HidePointCrossLevelProp(this, I) = HidePointCrossLevelProp(this, I+1);
					}
					LinkedCrossLevelProp[5](this) = NULL;
					LinkedSameLevelProp(this, 5) = NULL;
					HidePointCrossLevelProp(this, 5) = 0;
					HidePoints().Remove(Replace, 1);
				}

				// Retail leaves SwingPosition uninitialised when the candidate has no swing config.
				FHideLink Link;
				appMemzero(&Link, sizeof(Link));

				UObject* SwingConfig = GetSwingToVantagePointConfig(SwingMoveProp(Other));
				if( SwingConfig )
				{
					static TExtensionProperty<FLOAT> SwingDistanceProp;
					static TExtensionProperty<FLOAT> SwingRadiusProp;
					SwingDistanceProp.Bind(SwingConfig->GetClass(), TEXT("DefaultSwingDistance"));
					SwingRadiusProp.Bind(SwingConfig->GetClass(), TEXT("DefaultSwingRadius"));

					if( SwingDistanceProp.IsBound() && SwingRadiusProp.IsBound() )
					{
						FVector Flat(Other->Location.X - Location.X, Other->Location.Y - Location.Y, 0.f);
						Flat.Normalize();

						const FVector A = Location + Flat * 200.f;
						const FVector B = Other->Location - Flat * 200.f;
						const FLOAT Len = (B - A).Size();
						const FLOAT Radius = SwingRadiusProp(SwingConfig) * (Len / SwingDistanceProp(SwingConfig));

						const FVector Mid = (A + B) * 0.5f;
						const FVector ArcDir = (B - A) / Len;

						FVector Up(0.f, 0.f, 1.f);
						Up.Normalize();
						FVector Perp = (ArcDir ^ Up) ^ ArcDir;
						Perp.Normalize();

						const FLOAT Height = appSqrt(Radius * Radius - (Len * 0.5f) * (Len * 0.5f));
						FVector Apex = Mid + Perp * Height;

						FVector Dir2D(ArcDir.X, ArcDir.Y, 0.f);
						Dir2D.Normalize();

						// Pull the swing pivot onto nearby geometry if the space above the arc is clear.
						FCheckResult ClearHit(1.f);
						if( PlayerDefault && GWorld->SingleLineCheck(ClearHit, PlayerDefault, Mid + (Apex - Mid) * 2.f, Mid, RHIDEPOINT_TRACE_APEX) )
						{
							const FLOAT FanLength = Height * 1.5f;
							FLOAT BestPlusTime = 1.f, BestMinusTime = 1.f;
							FVector BestPlus(0.f), BestMinus(0.f);

							for( INT I=0; I<9; I++ )
							{
								FVector Fan(-Dir2D.Y, Dir2D.X, (I - 5) * 0.25f);
								Fan.Normalize();

								FCheckResult PlusHit(1.f), MinusHit(1.f);
								GWorld->SingleLineCheck(PlusHit, PlayerDefault, Apex + Fan * FanLength, Apex, RHIDEPOINT_TRACE_APEX);
								GWorld->SingleLineCheck(MinusHit, PlayerDefault, Apex - Fan * FanLength, Apex, RHIDEPOINT_TRACE_APEX);

								if( BestPlusTime > PlusHit.Time )
								{
									BestPlusTime = PlusHit.Time;
									BestPlus = PlusHit.Location;
								}
								if( BestMinusTime > MinusHit.Time )
								{
									BestMinusTime = MinusHit.Time;
									BestMinus = MinusHit.Location;
								}
							}

							if( BestPlusTime < 1.f || BestMinusTime < 1.f )
							{
								Apex = (BestMinusTime <= BestPlusTime) ? BestMinus : BestPlus;
							}
						}

						// Keep the pivot at least 45 degrees above the midpoint.
						const FVector Diff = Apex - Mid;
						if( appSin(PI / 4.f) > Diff.SafeNormal().Z )
						{
							Apex.Z = Mid.Z + appSqrt(Diff.X * Diff.X + Diff.Y * Diff.Y) * appTan(PI / 4.f);
						}

						Link.SwingPosition = Apex;
					}
				}

				const INT Slot = HidePoints().Num();
				if( Slot < 6 )
				{
					Link.MidPoint = (Location + Other->Location) * 0.5f;

					if( Other->GetOuter() == MyLevel )
					{
						LinkedSameLevelProp(this, Slot) = Other;
						HidePointCrossLevelProp(this, Slot) = 0;
					}
					else
					{
						LinkedCrossLevelProp[Slot](this) = Other;
						HidePointCrossLevelProp(this, Slot) = 1;
					}

					HidePoints()(HidePoints().Add(1)) = Link;
				}
			}
		}
	}

	if( bAdjustNeighbours )
	{
		for( FActorIterator It; It; ++It )
		{
			AActor* Candidate = *It;
			if( Candidate->IsA(HidePointClass) && Candidate != this && Candidate->GetOuter() == MyLevel
				&& AutoAdjustOnPathBuildProp.IsBound() && AutoAdjustOnPathBuildProp(Candidate) )
			{
				((ARHidePoint*)Candidate)->AutoAdjust(FALSE);
			}
		}
	}
}

static FNativeFunctionLookup GRHidePointNatives[] =
{
	MAP_NATIVE(ARHidePoint, execGetLinkedHidePoint)
	MAP_NATIVE(ARHidePoint, execIsHidePointLinked)
	{NULL, NULL}
};

void RegisterRHidePointNatives()
{
	RegisterExtensionNatives(TEXT("RHidePoint"), GRHidePointNatives);
}
