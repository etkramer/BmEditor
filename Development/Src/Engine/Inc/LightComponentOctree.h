/*=============================================================================
	LightComponentOctree.h: BM2's world-level static light octree.
=============================================================================*/

#ifndef __LIGHTCOMPONENTOCTREE_H__
#define __LIGHTCOMPONENTOCTREE_H__

#if BATMAN

#include "GenericOctree.h"

/** Defines how a light component is stored in the world's static light octree. */
struct FLightComponentOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static FBoxCenterAndExtent GetBoundingBox(ULightComponent* const& Element)
	{
		return FBoxCenterAndExtent(Element->GetBoundingBox());
	}

	FORCEINLINE static UBOOL AreElementsEqual(ULightComponent* const& A,ULightComponent* const& B)
	{
		return A == B;
	}

	FORCEINLINE static void SetElementId(ULightComponent* const& Element,FOctreeElementId Id)
	{
		Element->OctreeId = Id;
	}
};

#endif // BATMAN

#endif // __LIGHTCOMPONENTOCTREE_H__
