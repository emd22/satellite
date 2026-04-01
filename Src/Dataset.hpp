#pragma once

#include "Vec3.hpp"

#include <vector>

#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"

struct Force
{
    Vec3r Position = Vec3r::sZero;
    Vec3r Velocity = Vec3r::sZero;
};

struct TimeFrame
{
    std::vector<Vector3> Positions;
    uint32 CurrentStep = 0;
};

class Dataset
{
public:
    static void SaveFromTLE(const char* tle_path, const char* dst_path);
    void LoadFromBin(const char* bin_path);

    inline uint32 Size() const { return TimeFrames.size(); }

    const std::vector<Vector3>& GetPositionsForIndex(uint32 index) const { return TimeFrames[index].Positions; }


public:
    std::vector<TimeFrame> TimeFrames;
};
