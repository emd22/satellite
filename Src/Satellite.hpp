#pragma once

#include "String.hpp"
#include "Vec3.hpp"

struct Satellite
{
public:
    struct TimeStep
    {
        Vec3r Position = Vec3r::sZero;
        Vec3r Velocity = Vec3r::sZero;
    };

public:
    const TimeStep& GetInfoAtTimeStep(uint32 time_step) const { return TimeSteps[time_step]; }

    void AddTimeStep(const TimeStep& ts) { TimeSteps.push_back(ts); }
    const TimeStep& GetTimeStep(uint32 time_step) const { return TimeSteps[time_step % TimeSteps.size()]; }

public:
    String Series;
    String Identifier;

    Vec3r CurrentPosition = Vec3r::sZero;
    Vec3r GoalPosition = Vec3r::sZero;

    std::vector<TimeStep> TimeSteps;
};
