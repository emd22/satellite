#pragma once

#include "String.hpp"
#include "Vec3.hpp"

#include <algorithm>
#include <vector>

struct Satellite
{
public:
    struct TimeStep
    {
        Vec3r Position = Vec3r::sZero;
    };

public:
    const TimeStep& GetInfoAtTimeStep(uint32 time_step) const { return TimeSteps[time_step]; }

    void AddTimeStep(const TimeStep& ts) { TimeSteps.push_back(ts); }
    const TimeStep& GetTimeStep(uint32 time_step) const { return TimeSteps[time_step % TimeSteps.size()]; }

    /**
     * @brief Places the satellite at a time step, ready to interpolate towards the next one.
     * The last time step has no successor, so the satellite holds still there instead of heading back to the first.
     */
    const Vec3r& MoveToTimeStep(uint32 time_step)
    {
        const uint32 last_step = uint32(TimeSteps.size()) - 1;

        StartPosition = TimeSteps[std::min(time_step, last_step)].Position;
        GoalPosition = TimeSteps[std::min(time_step + 1, last_step)].Position;
        Position = StartPosition;

        return Position;
    }

    bool IsDebris()
    {
        static constexpr Hash32 scDebrisHash = HashStr32("Debris");

        return Series.GetHash() == scDebrisHash;
    }

    void UpdatePosition(float32 fraction) { Position = StartPosition + (GoalPosition - StartPosition) * fraction; }

public:
    String Name;
    String Series;
    String Identifier;
    uint32 NoradId;

    uint32 FrameStart = 0;
    Vec3r Position = Vec3r::sZero;
    Vec3r StartPosition = Vec3r::sZero;
    Vec3r GoalPosition = Vec3r::sZero;

    uint16 LaunchYear = 0;

    std::vector<TimeStep> TimeSteps;
};
