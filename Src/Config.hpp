#pragma once

#include "Types.hpp"

#define DISABLE_BINARY_CACHING 1

static constexpr uint32 scNumTimeCaptures = 250;
// Time steps are measured in minutes from the reference epoch shared by every satellite (see Dataset::ReferenceJd)
static constexpr float64 scStartTime = 10.0;
static constexpr float32 scTimeJump = 1.5f;

// Scene units per kilometre. This is used to reduce the scale of everything to reduce floating point error.
static constexpr float32 scKmToUnits = 0.00035f;

/// Radius of the Earth in km
static constexpr float64 scEarthRadiusKm = 6378.137;

// Angle of the +Y pole at which the Greenwich meridian sits on the Earth texture
static constexpr float64 scGreenwichSceneAngle = -1.5707963267948966;

// #define VEC3_DOUBLE_PRECISION

#ifndef ASSET_BASE_DIR
#define ASSET_BASE_DIR "."
#endif
