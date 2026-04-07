#pragma once

#include "Types.hpp"

#define DISABLE_BINARY_CACHING 1

static constexpr uint32 scNumTimeCaptures = 75;
static constexpr float64 scStartTime = 10.0;
static constexpr float32 scTimeJump = 1.5f;

#define VEC3_DOUBLE_PRECISION

#ifndef ASSET_BASE_DIR
#define ASSET_BASE_DIR "."
#endif
