//------------------------------------------------------------------------------
// Version information for the FaceFX SDK.
//
// Owner: Jamie Redmond
//
// Copyright (c) 2002-2010 OC3 Entertainment, Inc.
//------------------------------------------------------------------------------

#ifndef FxVersionInfo_H__
#define FxVersionInfo_H__

#include "FxPlatform.h"

namespace OC3Ent
{

namespace Face
{

// The file format version.  Increment this number when any change would alter
// the binary data stored in an FxArchive object (e.g. changing serialization).
static const FxSize FxFileFormatVersion = 0;

// Define this to build the evaluation version.
//#define EVALUATION_VERSION

// Define this to build the mod developer version.
//#define MOD_DEVELOPER_VERSION

// Define this to build the no-save version.
//#define NO_SAVE_VERSION

// Only define this during OC3's internal build process.
//#define OC3_INTERNAL_BUILD

} // namespace Face

} // namespace OC3Ent

#endif
