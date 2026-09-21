/**
 * Base class for actors whose geometry Batman can climb or shimmy along.
 */
class RStaticClimbableActor extends Actor
	native
	abstract
	notplaceable;

struct native RailingBlockerPair
{
	var int CollectionIndex;
	var int RailingIndex;
	var array<RStaticClimbableActor> OtherActors;
};

var(Climbing) const bool bRailing;
var(AdvancedClimb) const bool bSpikeyRailing;
var(AdvancedClimb) const bool bUseBoundingBoxForClimbing;
var(Climbing) const bool bClimbableSlopedRailing;
var(AdvancedClimb) const bool bNeverUseBracedShimmy;
var(AdvancedClimb) const bool bAllowWideRailings;
var(AdvancedClimb) const bool bDontBlockShimmyEdges;
var(AdvancedClimb) const bool bNoRailingTops;
var bool bCanBeRailingBlocker;
var array<RailingBlockerPair> RailingBlockers;

defaultproperties
{
}
