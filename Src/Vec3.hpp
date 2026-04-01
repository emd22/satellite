#pragma once

#include "Types.hpp"

#include <cstdio>

class Vec3r
{
public:
    static const Vec3r sZero;

public:
    Vec3r(float64 x, float64 y, float64 z) : X(x), Y(y), Z(z) {}

    void Print() { std::printf("{ %f, %f, %f }\n", X, Y, Z); }

    Vec3r operator*(float32 multiplier) const { return Vec3r(X * multiplier, Y * multiplier, Z * multiplier); }

public:
    union
    {
        struct
        {
            float64 X;
            float64 Y;
            float64 Z;
            float64 W; // Alignment
        };

        float64 Values[4];
    };
};
