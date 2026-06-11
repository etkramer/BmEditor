#ifndef PX_FOUNDATION_PX_APEX11_CALLBACKS_H
#define PX_FOUNDATION_PX_APEX11_CALLBACKS_H

#include "PxErrors.h"

namespace physx
{
namespace pubfnd2
{

PX_PUSH_PACK_DEFAULT

// BM: APEX 1.1 uses the PhysX 3-style callback layout.
class PxAllocatorCallback
{
public:
	virtual ~PxAllocatorCallback() {}
	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line) = 0;
	virtual void deallocate(void* ptr) = 0;
};

// BM: APEX 1.1 error callback companion for PxAllocatorCallback.
class PxErrorCallback
{
public:
	virtual ~PxErrorCallback() {}
	virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) = 0;
};

PX_POP_PACK

} // namespace pubfnd2

using pubfnd2::PxAllocatorCallback;
using pubfnd2::PxErrorCallback;

} // namespace physx

#endif
