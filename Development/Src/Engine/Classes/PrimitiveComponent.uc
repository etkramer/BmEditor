/**
 * Copyright 1998-2011 Epic Games, Inc. All Rights Reserved.
 */
class PrimitiveComponent extends ActorComponent
	dependson(Scene,LightComponent)
	native(Mesh)
	noexport
	abstract;

/** Mirrored from Scene.h */
struct MaterialViewRelevance
{
	var bool bOpaque;
	var bool bTranslucent;
	var bool bDistortion;
	var bool bOneLayerDistortionRelevance;
	var bool bLit;
	var bool bUsesSceneColor;
};

/** Which heightmap layer an object overrides. */
enum EHeightmapOverrideFilter
{
	HMO_FearGasLayer,
	HMO_PollenLayer
};

/** Enum indicating different type of objects for rigid-body collision purposes. */
enum ERBCollisionChannel
{
	RBCC_Default,
	RBCC_Nothing, // Special channel that nothing should request collision with.
	RBCC_Pawn,
	RBCC_Vehicle,
	RBCC_Water,
	RBCC_GameplayPhysics,
	RBCC_EffectPhysics,
	RBCC_FloatingRaft,
	RBCC_Gargoyles,
	RBCC_PawnRagdoll,
	RBCC_Rope,
	RBCC_Cloth,
	RBCC_CapeOnlyCollision,
	RBCC_PropStaticChunks,
	RBCC_FlyingVehicle,
	RBCC_BlockingVolume,
	RBCC_DeadPawn,
	RBCC_Clothing,
	RBCC_ClothingCollision,
	RBCC_FlexAsset,
	RBCC_Cape,
	RBCC_CinematicCape,
	RBCC_PawnRagdollStrungUp,
	RBCC_Projectile,
	RBCC_PropDynamicChunks,
	RBCC_Grate,
	RBCC_Prop,
	RBCC_MagneticDynamicObjects,
	RBCC_MagneticProp,
	RBCC_VehicleBlocker,
	RBCC_RobinCape,
	RBCC_PhysicsPuzzleObject
};

/** Per-shape PhysX filter bits. */
enum EPhysXShapeFilterFlags
{
	EPSF_NotifyOnCollision,
	EPSF_DisableCollisionResponse,
	EPSF_UsePairwiseCollisionFilter,
	EPSF_ContactModification,
	EPSF_DoNotNotifyOnCollisionWithVehicle,
	EPSF_HasCollidedWithFloor,
	EPSF_NotifyOnSelfCollision,
	EPSF_ForceDisableContactModification,
	EPSF_CapeCollisionTrigger,
	EPSF_DetachedVehiclePart
};

/**
 *	Container for indicating a set of collision channel that this object will collide with.
 *	Mirrored manually in UnPhysPublic.h
 */
struct RBCollisionChannelContainer
{
	var()	const bool	Default;
	var		const bool	Nothing; // This is reserved to allow an object to opt-out of all collisions, and should not be set.
	var()	const bool	Pawn;
	var()	const bool	Vehicle;
	var()	const bool	Water;
	var()	const bool	GameplayPhysics;
	var()	const bool	EffectPhysics;
	var()	const bool	FloatingRaft;
	var()	const bool	Gargoyles;
	var()	const bool	PawnRagdoll;
	var()	const bool	Rope;
	var()	const bool	Cloth;
	var()	const bool	CapeOnlyCollision;
	var()	const bool	PropStaticChunks;
	var()	const bool	FlyingVehicle;
	var()	const bool	BlockingVolume;
	var()	const bool	DeadPawn;
	var()	const bool	Clothing;
	var()	const bool	ClothingCollision;
	var()	const bool	FlexAsset;
	var()	const bool	Cape;
	var()	const bool	CinematicCape;
	var()	const bool	PawnRagdollStrungUp;
	var()	const bool	Projectile;
	var()	const bool	PropDynamicChunks;
	var()	const bool	Grate;
	var()	const bool	Prop;
	var()	const bool	MagneticDynamicObjects;
	var()	const bool	MagneticProp;
	var()	const bool	VehicleBlocker;
	var()	const bool	RobinCape;
	var()	const bool	PhysicsPuzzleObject;
};

/**
 *	Per-shape PhysX filter flags.
 *	Mirrored manually in UnPhysPublic.h
 */
struct PhysXShapeFilterFlagsContainer
{
	var	transient bool	NotifyOnCollision;
	var	transient bool	DisableCollisionResponse;
	var	transient bool	UsePairwiseCollisionFilter;
	var	transient bool	ContactModification;
	var	transient bool	DoNotNotifyOnCollisionWithVehicle;
	var	transient bool	HasCollidedWithFloor;
	var	transient bool	NotifyOnSelfCollision;
	var	transient bool	ForceDisableContactModification;
	var	transient bool	CapeCollisionTrigger;
	var	transient bool	DetachedVehiclePart;
};

/** Return codes for ClosestPointToPrimitive functions */
enum GJKResult
{
	GJK_Intersect,      //two primitives overlap (results invalid)
	GJK_NoIntersection, //two primitives don't overlap (results valid)
	GJK_Fail            //failed to find result in max iteration time (results valid but unoptimal)
};

/** Enum for controlling the falloff of strength of a radial impulse as a function of distance from Origin. */
enum ERadialImpulseFalloff
{
	/** Impulse is a constant strength, up to the limit of its range. */
	RIF_Constant,

	/** Impulse should get linearly weaker the further from origin. */
	RIF_Linear
};

/** How a radial impulse is applied to a body. */
enum ERadialImpulseType
{
	ERIT_Impulse,
	ERIT_VelocityChange,
	ERIT_SurfaceAreaApproximation
};

var const native transient int Tag;
// Primitive generated bounds.

var const native transient BoxSphereBounds Bounds;

/** The primitive's scene info. */
var private native transient const pointer SceneInfo{FPrimitiveSceneInfo};

// Scene data.

var native transient const matrix LocalToWorld;
var native transient const matrix CachedParentToWorld; //@todo please remove me if possible

/** A fence to track when the primitive is detached from the scene in the rendering thread. */
var private native const int DetachFence;

var native transient const float LocalToWorldDeterminant;
/**
 *	The index for the primitive component in the MotionBlurInfo array of the scene.
 *	Render-thread usage only.
 */
var native transient const int MotionBlurInfoIndex;

/** Replacement primitive to draw instead of this one (multiple UPrim's will point to the same Replacement) */
var(Rendering) edithide editinline export crosslevelpassive PrimitiveComponent ReplacementPrimitive;

var(Rendering) editoronly StaticMesh AutoLODOverride;

/** Keeps track of which fog component this primitive is using. */
var const editinline export transient FogVolumeDensityComponent FogVolumeComponent;

/**
 * The distance at which the renderer will switch from parent (low LOD) to children (high LOD).
 * This is basically the same as MinDrawDistance, except that the low LOD will draw even up close, if there are no children.
 */
var(LOD) float MassiveLODDistance;

var(LOD) editconst int MassiveLODAttachedPrimitives;

var(LOD) editoronly float AutoLODImportanceWeighting;

/**
 * Max draw distance exposed to LDs. The real max draw distance is the min (disregarding 0) of this and volumes affecting this object.
 * This is renamed to LDMaxDrawDistance in c++
 */
var(CullingAndOcclusion) const private noexport float MaxDrawDistance;

/**
 * The distance to cull this primitive at.
 * A CachedMaxDrawDistance of 0 indicates that the primitive should not be culled by distance.
 */
var editconst float CachedMaxDrawDistance;

var(Rendering) float MotionBlurInstanceScale;

/** Per-instance random value handed to the material. */
var duplicatetransient float PerInstanceRandom;

var(Rendering) interp float PerInstanceRandomOverride;

var const float CullArea;
var(CullingAndOcclusion) const float CullAreaMultiplier;

/** Environment shadow factor used when previewing unbuilt lighting on this primitive. */
var				editoronly byte		PreviewEnvironmentShadowing;

/** Allows you to override the PhysicalMaterial to use for this PrimitiveComponent. */
var(Physics)	const PhysicalMaterial			PhysMaterialOverride;

var	const native RB_BodyInstance	BodyInstance;

var() const vector			Translation;
var() const rotator			Rotation;
var() const float			Scale <UIMin=0.0 | UIMax=4.0>;
var() const vector			Scale3D;

/** Plane the reflection shadow of this primitive is projected onto. */
var() plane					ReflectionShadowPlane;

/**
 * The value of WorldInfo->TimeSeconds for the frame when this actor was last rendered.  This is written
 * from the render thread, which is up to a frame behind the game thread.
 */
var transient float	LastRenderTime;

/** if > 0, the script RigidBodyCollision() event will be called on our Owner when a physics collision involving
 * this PrimitiveComponent occurs and the relative velocity is greater than or equal to this
 */
var float ScriptRigidBodyCollisionThreshold;

/** Named collision setup this primitive uses. */
var(Collision)	const ECollisionFilter			CollisionFilter;

/** Which heightmap layer this primitive overrides. */
var()			const EHeightmapOverrideFilter	HeightmapOverrideType;

var bool bForceEdCanScale;
var bool bForceEdCanNonUniformScale;

var(ViewMode) const bool bDrawAsGauntletProjectionMesh;
var(ViewMode) const bool bDrawInFrontOfPostProcessWhenInXrayMode;
var(ViewMode) const bool bDrawInForegroundAndInFrontOfPostProcessWhenInXrayMode;
var(ViewMode) const bool bDrawInFrontOfPostProcessWhenInScanMode;
var(ViewMode) const bool bDrawInForegroundAndInFrontOfPostProcessWhenInScanMode;
var(ViewMode) const bool bDrawInFrontOfPostProcessWhenInThermalMode;
var(ViewMode) const bool bDrawInForegroundAndInFrontOfPostProcessWhenInThermalMode;
var(ViewMode) const bool bDrawInFrontOfPostProcessWhenInBatmobileViewMode;
var(ViewMode) const bool bDrawInForegroundAndInFrontOfPostProcessWhenInBatmobileViewMode;
var(ViewMode) const bool bDrawAsDisruptorSniperHighlightMesh;
var(ViewMode) const bool bDrawInFrontOfPostProcessWhenInDLCViewMode;
var(ViewMode) const bool bDrawInForegroundAndInFrontOfPostProcessWhenInDLCViewMode;

/** Whether to accept cull distance volumes to modify cached cull distance. */
var(CullingAndOcclusion) const bool	bAllowCullDistanceVolume;

var(CullingAndOcclusion) const bool	HiddenGame;
var(CullingAndOcclusion) const bool	HiddenEditor;

/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
var const bool bOwnerNoSee;

/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
var const bool bOnlyOwnerSee;

var(ViewMode) const bool bXrayNoSee;
var(ViewMode) const bool bOnlyXraySee;
var(ViewMode) const bool bScanModeNoSee;
var(ViewMode) const bool bOnlyScanModeSee;
var(ViewMode) const bool bThermalNoSee;
var(ViewMode) const bool bOnlyThermalSee;
var(ViewMode) const bool bBatmobileViewNoSee;
var(ViewMode) const bool bOnlyBatmobileViewSee;
var(CullingAndOcclusion) const bool bOnlyReflectionSee;
var(CullingAndOcclusion) const bool bReflectionNoSee;
var(CullingAndOcclusion) const bool bOnlyReflectionProbeSee;
var(CullingAndOcclusion) const bool bReflectionProbeNoSee;
var(CullingAndOcclusion) const bool bExcludeFromRainVolume;
var const bool bOverrideHeightmapObjectOnly;
var const transient bool DontDrawThisFrame;
var(LOD) bool NotCountedInParentMassiveLODAttachedPrimitives;
var(LOD) bool ForceOnBakeIntoBackgroundForAutoLOD;
var(LOD) bool ForceOffBakeIntoBackgroundForAutoLOD;
var(LOD) bool ForceOffAutoLODOverride;
var(LOD) bool ForceOffAutoLODMasked;
var(LOD) bool ForceOnAutoLODMasked;
var(LOD) bool ForceOffTwoSided;
var(LOD) bool NeverHideDuringAutoLOD;
var(LOD) bool AlwaysHideDuringAutoLOD;

/** If true, bHidden on the Owner of this component will be ignored. */
var const bool bIgnoreOwnerHidden;

/**
 * Whether to render the primitive in the depth only pass.
 */
var(CullingAndOcclusion) bool bUseAsOccluder;

var(CullingAndOcclusion) bool bUseAsOccluderAutomatic;
var(CullingAndOcclusion) bool bAllowOcclusionTesting;

/** If this is True, this component doesn't need exact occlusion info. */
var(CullingAndOcclusion) bool bAllowApproximateOcclusion;

var(CullingAndOcclusion) bool bUmbraUseAsOccluder;
var bool bUmbraDoNotIdCull;

/** If this is True, this component will return 0.0f as their occlusion when first rendered. */
var bool bFirstFrameOcclusion;

/** If True, this component will still be queried for occlusion even when it intersects the near plane. */
var bool bIgnoreNearPlaneIntersection;

/** If this is True, this component can be selected in the editor. */
var bool bSelectable;

/** If TRUE, forces mips for textures used by this component to be resident when this component's level is loaded. */
var(Rendering) const bool bForceMipStreaming;

/** If TRUE, this primitive accepts static level placed decals in the editor. */
var(Rendering) const bool bAcceptsStaticDecals;

/** If TRUE, this primitive accepts dynamic decals spawned during gameplay.  */
var(Rendering) const bool bAcceptsDynamicDecals;

var native transient const bool bIsRefreshingDecals;

var transient bool bAllowDecalAutomaticReAttach;

var bool bUsePerInstanceHitProxies;

// Lighting flags

/** Whether to cast any shadows or not */
var(Lighting)	bool		CastShadow;

/** If true, forces all static lights to use light-maps for direct lighting on this primitive. */
var				const bool	bForceDirectLightMap;

/** If false, primitive does not cast dynamic shadows. */
var(Lighting)	bool		bCastDynamicShadow;

/** If false, primitive does not cast static shadows. */
var(Lighting)	bool		bCastStaticShadow;

/**
 * If true, the primitive will only shadow itself and will not cast a shadow on other primitives.
 */
var				bool		bSelfShadowOnly;

var transient	bool		bMeshAddedToScene;

/**
 * Optimization for objects which don't need to receive dominant light shadows.
 */
var				bool		bAcceptsDynamicDominantLightShadows;

/**
 *	If TRUE, the primitive will cast shadows even if bHidden is TRUE.
 */
var				bool		bCastHiddenShadow;

/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
var				bool		bCastShadowAsTwoSided;

/** Does this primitive accept lights? */
var				const bool	bAcceptsLights;

/** Whether this primitives accepts dynamic lights */
var				const bool	bAcceptsDynamicLights;

/**
 * If TRUE, dynamically lit translucency on this primitive will render in one pass.
 */
var				const bool bUseOnePassLightingOnTranslucency;

/** Whether the primitive supports/ allows static shadowing */
var				const bool	bUsePrecomputedShadows;

var				const bool	bAgeSorted;

var deprecated bool bAllowAmbientOcclusion;

// Collision flags.

var(Collision)	const bool	CollideActors <DMCOnly=true>;
var(Collision)	const bool	BlockActors <DMCOnly=true>;
var			const bool	BlockZeroExtent <DMCOnly=true>;
var			const bool	BlockNonZeroExtent <DMCOnly=true>;
/** TRUE if this primitive is eligible to block camera traces, FALSE if the camera should ignore it. */
var			const bool	CanBlockCamera;
var(Collision)	const bool	BlockRigidBody;
var			const bool	BlockRigidBodyInitial;
var			const bool	bBlockFootPlacement;
var			const bool	BlockRigidBodyPhysX;

/** If TRUE, CollisionFilter is used in place of the one the owner supplies. */
var(Collision)	const bool	OverrideCollisionFilter;

var(Collision)	nontransactional const bool	BlockTurbulence;

/** Never create any physics engine representation for this body. */
var(Physics) const bool bDisableAllRigidBody;

/** When creating rigid body, will skip normal geometry creation step, and will rely on ModifyNxActorDesc to fill in geometry. */
var const bool	bSkipRBGeomCreation;

/**
 *	Flag that indicates if OnRigidBodyCollision function should be called for physics collisions involving this PrimitiveComponent.
 */
var(Physics) const bool	bNotifyRigidBodyCollision;

var bool bNotifyRigidBodyCollisionIgnoredWhenFarAway;
var const bool bNotifyRigidBodyCollisionOnSelfCollision;
var const bool	bEnableContactModificationCallback;
var const bool	bEnableSleepWakeNotifies;
var(Collision) bool bDisableMinCollisionThickness;

// Novodex fluids

/** Whether this object should act as a 'drain' for fluid, and destroy fluid particles when they contact it. */
var const bool	bFluidDrain;

/** Indicates that fluid interaction with this object should be 'two-way'. */
var const bool	bFluidTwoWay;

// Physics

/** Will ignore radial impulses applied to this component. */
var(Physics)	bool		bIgnoreRadialImpulse;

/** Will ignore radial forces applied to this component. */
var(Physics)	bool		bIgnoreRadialForce;

/** Will ignore force field applied to this component. */
var				bool		bIgnoreForceField;

/** Place into a NxCompartment that will run in parallel with the primary scene's physics. */
var				const bool		bUseCompartment;

// General flags.

/** If this is True, this component must always be loaded on clients, even if HiddenGame && !CollideActors. */
var private const bool AlwaysLoadOnClient;

/** If this is True, this component must always be loaded on servers, even if !CollideActors. */
var private const bool AlwaysLoadOnServer;

/** Allow certain components to render even if the parent actor is part of the camera's HiddenActors array. */
var bool bIgnoreHiddenActorsMembership;

var() const bool			AbsoluteTranslation;
var() const bool			AbsoluteRotation;
var() const bool			AbsoluteScale;

/** Determines whether or not we allow shadowing fading. **/
var bool bAllowShadowFade;

var bool bSupportedOnMobile;

// Internal scene data.

var const native transient bool bWasSNFiltered;

var editoronly bool bEnableRBFixedFlag;
var bool bAddPxShapesToSceneQueryStructure;

var const native transient int QuadTreeEntry;

/**
 * Translucent objects with a lower sort priority draw behind objects with a higher priority.
 **/
var(Rendering) int TranslucencySortPriority;

/** Index into the level's precomputed visibility data. */
var duplicatetransient int VisibilityId;

/** Umbra occlusion object id. */
var duplicatetransient int UmbraId;

var duplicatetransient editoronly int UmbraIdVersion;

/**
 * Lighting channels controlling light/ primitive interaction. Only allows interaction if at least one channel is shared
 */
var(Lighting)	const LightingChannelContainer	LightingChannels;

/** Types of objects that this physics objects will collide with. */
var(Collision) const RBCollisionChannelContainer	RBCollideWithChannels;

/** Per-shape PhysX filter bits. */
var transient PhysXShapeFilterFlagsContainer	PhysXShapeFilterFlags;

/** Enum indicating what type of object this should be considered for rigid body collision. */
var(Collision)	const ERBCollisionChannel	RBChannel;

/** The scene depth priority group to draw the primitive in. */
var(Rendering) const ESceneDepthPriorityGroup DepthPriorityGroup;

/** If detail mode is >= system detail mode, primitive won't be rendered. */
var const EDetailMode DetailMode;

/**
 *	Used for creating one-way physics interactions (via constraints or contacts)
 *	Groups with lower RBDominanceGroup push around higher values in a 'one way' fashion. Must be <32.
 */
var(Physics)	byte		RBDominanceGroup;

var int LevelEdgeCollectionIndex;

/** Physics scene this component is forced into, instead of the world's. */
var native pointer OverrideRBPhysScene{FRBPhysScene};

/**
 *	Add an impulse to the physics of this PrimitiveComponent.
 *
 * Good for zero time.  One time insta burst.
 *
 *	@param	Impulse		Magnitude and direction of impulse to apply.
 *	@param	Position	Point in world space to apply impulse at. If Position is (0,0,0), impulse is applied at center of mass ie. no rotation.
 *	@param	BoneName	If a SkeletalMeshComponent, name of bone to apply impulse to.
 *	@param	bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
 */
native final function AddImpulse(vector Impulse, optional vector Position, optional name BoneName, optional bool bVelChange);

/**
 * Add an impulse to this component, radiating out from the specified position.
 * In the case of a skeletal mesh, may affect each bone of the mesh.
 *
 * @param Origin		Point of origin for the radial impulse blast
 * @param Radius		Size of radial impulse. Beyond this distance from Origin, there will be no affect.
 * @param Strength		Maximum strength of impulse applied to body.
 * @param Falloff		Allows you to control the strength of the impulse as a function of distance from Origin.
 * @param bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
 */
native final function AddRadialImpulse(vector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, optional bool bVelChange);

/**
 *	Add a force to this component.
 *
 * This is like a thruster. Good for adding a burst over some (non zero) time.
 *
 *	@param Force		Force vector to apply. Magnitude indicates strength of force.
 *	@param Position		Position on object to apply force. If (0,0,0), force is applied at center of mass.
 *	@param BoneName		Used in the skeletal case to apply a force to a single body.
 */
native final function AddForce(vector Force, optional vector Position, optional name BoneName);

/**
 *	Add a force originating from the supplied world-space location.
 *
 *	@param Origin		Origin of force in world space.
 *	@param Radius		Radius within which to apply the force.
 *	@param Strength		Strength of force to apply.
 *  @param Falloff		Allows you to control the strength of the force as a function of distance from Origin.
 */
native final function AddRadialForce(vector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff);

/**
*	Add a torque to this component.
*	@param Torque		Force vector to apply. Magnitude indicates strength of force.
*	@param BoneName		Used in the skeletal case to apply a force to a single body.
*/
native final function AddTorque(vector Torque, optional name BoneName);

/**
 * Set the linear velocity of the rigid body physics of this PrimitiveComponent. If no rigid-body physics is active, will do nothing.
 * In the case of a SkeletalMeshComponent will affect all bones.
 * This should be used cautiously - it may be better to use AddForce or AddImpulse.
 *
 * @param	NewVel			New linear velocity to apply to physics.
 * @param	bAddToCurrent	If true, NewVel is added to the existing velocity of the body.
 */
native final function SetRBLinearVelocity(vector NewVel, optional bool bAddToCurrent);

/**
 * Set the angular velocity of the rigid body physics of this PrimitiveComponent. If no rigid-body physics is active, will do nothing.
 * In the case of a SkeletalMeshComponent will affect all bones - and will apply the linear velocity necessary to get all bones to rotate around the root.
 * This should be used cautiously - it may be better to use AddForce or AddImpulse.
 *
 * @param	NewAngVel		New angular velocity to apply to physics.
 * @param	bAddToCurrent	If true, NewAngVel is added to the existing velocity of the body.
 */
native final function SetRBAngularVelocity(vector NewAngVel, optional bool bAddToCurrent);

/**
 *	Reduce velocity of rigid body physics in the direction supplied. This decomposes body velocity into that along supplied vector and that perpendicular to the vector.
 *	That along vector, if in same direction as vector, is scale by VelScale. If it is moving in the opposite direction to supplied vector it is not affected.
 *
 *	@param	RetardDir		Unit vector indicating direction to check velocity of physics against
 *	@param	VelScale		Value from 0.0 to 1.0 - 1.0 will stop all motion along RetardDir
 */
native final function RetardRBLinearVelocity(vector RetardDir, float VelScale);

/**
 * Called if you want to move the physics of a component which has dynamics running (ie actor is in PHYS_RigidBody).
 * Be careful calling this when this is jointed to something else, or when it does not fit in the destination (no checking is done).
 * @param NewPos new position of the body
 * @param BoneName (SkeletalMeshComponent only) if specified, the bone to change position of
 * 			if not specified for a SkeletalMeshComponent, all bodies are moved by the delta
 * 			between the desired location and that of the root body.
 */
native final function SetRBPosition(vector NewPos, optional name BoneName);

/**
 * Called if you want to rotate the physics of a component which has dynamics running (ie actor is in PHYS_RigidBody).
 * @param NewRot new rotation of the body
 * @param BoneName (SkeletalMeshComponent only) if specified, the bone to change rotation of
 * 			if not specified for a SkeletalMeshComponent, all bodies are moved by the delta
 * 			between the desired rotation and that of the root body.
 */
native final function SetRBRotation(rotator NewRot, optional name BoneName);

/**
 *	Ensure simulation is running for this component.
 *	If a SkeletalMeshComponent and no BoneName is specified, will wake all bones in the PhysicsAsset.
 */
native final function WakeRigidBody(optional name BoneName);

/**
 * Put a simulation back to sleep.
 */
native final function PutRigidBodyToSleep(optional name BoneName);

/**
 *	Returns if the body is currently awake and simulating.
 *	If a SkeletalMeshComponent, and no BoneName is specified, will pick a random bone -
 *	so does not make much sense if not all bones are jointed together.
 */
native final function bool RigidBodyIsAwake(optional name BoneName);

/**
 *	Change the value of BlockRigidBody.
 *
 *	@param NewBlockRigidBody - The value to assign to BlockRigidBody.
 */
native final function SetBlockRigidBody(bool bNewBlockRigidBody);

/**
 *	Changes a member of the RBCollideWithChannels container for this PrimitiveComponent.
 *
 * @param bNewCollides whether or not to collide with passed in channel
 */
final native function SetRBCollidesWithChannel(ERBCollisionChannel Channel, bool bNewCollides);

/**
 *	Sets the collision channels based on the settings in the Channel container.
 *
 * @param Channels is a list of channels with which the component should collide
 */
final native function SetRBCollisionChannels(RBCollisionChannelContainer Channels);

/**
 *	Changes the rigid-body channel that this object is defined in.
 */
final native function SetRBChannel(ERBCollisionChannel Channel);

/** Changes the value of bNotifyRigidBodyCollision
 * @param bNewNotifyRigidBodyCollision - The value to assign to bNotifyRigidBodyCollision
 */
native final function SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision);

/** initializes rigid body physics for this component
 * this is done automatically for PrimitiveComponents attached via Actor defaults,
 * but if a component is attached at runtime you may need to call this function to set it up
 * @note: this function does nothing if not attached or bDisableAllRigidBody is set
 */
native final function InitRBPhys();

/**
 *	Changes the current PhysMaterialOverride for this component.
 *	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc, it will only change its
 *	surface properties like friction and the damping.
 */
native final function SetPhysMaterialOverride(PhysicalMaterial NewPhysMaterial);

/** returns the physics RB_BodyInstance for the root body of this component (if any) */
native final function RB_BodyInstance GetRootBodyInstance();

/**
 *	Used for creating one-way physics interactions.
 *	@see RBDominanceGroup
 */
native final function SetRBDominanceGroup(BYTE InDomGroup);

/**
 *  Looking at various values of the component, determines if this
 *  component should be added to the scene
 * @return TRUE if the component is visible and should be added to the scene, FALSE otherwise
 */
native final function bool ShouldComponentAddToScene();

/**
 * Changes the value of HiddenGame.
 *
 * @param NewHidden	- The value to assign to HiddenGame.
 */
native final function k2call SetHidden(bool NewHidden);

/**
 * Changes the value of bOwnerNoSee.
 */
native final function SetOwnerNoSee(bool bNewOwnerNoSee);

/**
 * Changes the value of bOnlyOwnerSee.
 */
native final function SetOnlyOwnerSee(bool bNewOnlyOwnerSee);

/**
* Changes the value of bIgnoreOwnerHidden.
*/
native final function SetIgnoreOwnerHidden(bool bNewIgnoreOwnerHidden);

/**
 * Changes the value of CullDistance.
 * @param NewCullDistance - The value to assign to CullDistance.
 */
native final function SetCullDistance(float NewCullDistance);

/**
 * Changes the value of LightingChannels.
 * @param NewLightingChannels - The value to assign to LightingChannels.
 */
native final function SetLightingChannels(LightingChannelContainer NewLightingChannels);

/**
 * Changes the value of DepthPriorityGroup.
 * @param NewDepthPriorityGroup - The value to assign to DepthPriorityGroup.
 */
native final function SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup);

/**
 * Changes the value of bUseViewOwnerDepthPriorityGroup and ViewOwnerDepthPriorityGroup.
 * @param bNewUseViewOwnerDepthPriorityGroup - The value to assign to bUseViewOwnerDepthPriorityGroup.
 * @param NewViewOwnerDepthPriorityGroup - The value to assign to ViewOwnerDepthPriorityGroup.
 */
native final function SetViewOwnerDepthPriorityGroup(
	bool bNewUseViewOwnerDepthPriorityGroup,
	ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
	);

native final function SetTraceBlocking(bool NewBlockZeroExtent, bool NewBlockNonZeroExtent);

native final function SetActorCollision(bool NewCollideActors, bool NewBlockActors);

// Copied from TransformComponent
native function k2call SetTranslation(vector NewTranslation);
native function k2call SetRotation(rotator NewRotation);
native function k2call SetScale(float NewScale);
native function k2call SetScale3D(vector NewScale3D);
native function SetAbsolute(optional bool NewAbsoluteTranslation,optional bool NewAbsoluteRotation,optional bool NewAbsoluteScale);

final function vector GetPosition()
{
	local vector Position;
	Position.X = LocalToWorld.WPlane.X;
	Position.Y = LocalToWorld.WPlane.Y;
	Position.Z = LocalToWorld.WPlane.Z;
	return Position;
}

/** Returns rotation of the component, in world space. */
final native function rotator GetRotation();

/**
* Calculates the closest point on this primitive to a point given
* @param POI - Point in world space to determine closest point to
* @param Extent - Convex primitive
* @param OutPointA - The point closest on the extent box
* @param OutPointB - Point on this primitive closest to the extent box
*
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
native final function GJKResult ClosestPointOnComponentToPoint(out vector POI, out vector Extent, out vector OutPointA, out vector OutPointB);

/**
* Calculates the closest point this component to another component
* @param PrimitiveComponent - Another Primitive Component
* @param PointOnComponentA - Point on this primitive closest to other primitive
* @param PointOnComponentB - Point on other primitive closest to this primitive
*
* @return An enumeration indicating the result of the query (intersection/non-intersection/failure)
*/
native function GJKResult ClosestPointOnComponentToComponent(out PrimitiveComponent OtherComponent, out vector PointOnComponentA, out vector PointOnComponentB);

// BM: defaults are AK's, read out of Default__PrimitiveComponent's cooked tag stream.
defaultproperties
{
	MotionBlurInstanceScale=1.0
	PerInstanceRandom=0.3731803
	PerInstanceRandomOverride=-1.0
	CullArea=10000000000000.0
	CullAreaMultiplier=1.0
	Scale=1.0
	Scale3D=(X=1.0,Y=1.0,Z=1.0)

	bAllowCullDistanceVolume=TRUE
	bUseAsOccluderAutomatic=TRUE
	bAllowOcclusionTesting=TRUE
	bAllowApproximateOcclusion=TRUE
	bUmbraUseAsOccluder=TRUE
	bSelectable=TRUE
	bAllowDecalAutomaticReAttach=TRUE
	bCastDynamicShadow=TRUE
	bAcceptsDynamicDominantLightShadows=TRUE
	bAcceptsDynamicLights=TRUE
	CanBlockCamera=TRUE
	bBlockFootPlacement=TRUE
	bNotifyRigidBodyCollisionIgnoredWhenFarAway=TRUE
	bDisableMinCollisionThickness=TRUE
	AlwaysLoadOnClient=TRUE
	AlwaysLoadOnServer=TRUE
	bAllowShadowFade=TRUE
	bSupportedOnMobile=TRUE
	bAddPxShapesToSceneQueryStructure=TRUE

	VisibilityId=-1
	UmbraId=-1
	RBCollideWithChannels=(Default=TRUE)
	DepthPriorityGroup=SDPG_World
	RBDominanceGroup=15
	LevelEdgeCollectionIndex=65535
}
