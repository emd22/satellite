#pragma once

#include "Vec3.hpp"

#include <vector>

namespace rl {
#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"
} // namespace rl

class String;

struct Force
{
    Vec3r Position = Vec3r::sZero;
    Vec3r Velocity = Vec3r::sZero;
};

struct TimeFrame
{
    std::vector<rl::Vector3> Positions;
    uint32 CurrentStep = 0;
};

class Dataset
{
public:
    static void SaveFromTLE(const String& tle_path, const String& dst_path);
    void LoadFromBin(const String& bin_path);

    inline uint32 Size() const { return TimeFrames.size(); }

    const std::vector<rl::Vector3>& GetPositionsForIndex(uint32 index) const { return TimeFrames[index].Positions; }


public:
    std::vector<TimeFrame> TimeFrames;
};
