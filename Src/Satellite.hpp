#pragma once

#include "String.hpp"
#include "Vec3.hpp"

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

    const Vec3r& MoveToTimeStep(uint32 time_step, bool warp_to = false)
    {
        Position = GoalPosition;

        GoalPosition = GetTimeStep(time_step + 1).Position;

        if (warp_to) {
            Position = GetTimeStep(time_step).Position;
        }

        Movement = (GoalPosition - Position) / LerpFrames;

        return Position;
    }

    void UpdatePosition() { Position += Movement; }

    void CalculateMoveSpeed(uint32 lerp_frames) { LerpFrames = lerp_frames; }

public:
    String Series;
    String Identifier;

    uint32 FrameStart = 0;
    Vec3r Position = Vec3r::sZero;
    Vec3r GoalPosition = Vec3r::sZero;
    Vec3r Movement = Vec3r::sZero;

    uint16 LaunchYear = 0;

    uint32 LerpFrames = 10;

    std::vector<TimeStep> TimeSteps;
};
