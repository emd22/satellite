#pragma once

#include "Config.hpp"
#include "Types.hpp"

#include <cstddef>
#include <cstdio>
#include <type_traits>

namespace rl {
#include "ThirdParty/raylib.h"
} // namespace rl

class Vec3r
{
public:
    static const Vec3r sZero;

#ifdef VEC3_DOUBLE_PRECISION
    using Real = float64;
#else
    using Real = float32;
#endif

public:
    Vec3r(float64 buffer[4])
    {
        if constexpr (std::is_same_v<Real, float32>) {
            X = static_cast<float32>(buffer[0]);
            Y = static_cast<float32>(buffer[1]);
            Z = static_cast<float32>(buffer[2]);
        }
        else {
            X = buffer[0];
            Y = buffer[1];
            Z = buffer[2];
        }
    }

    Vec3r(float32 buffer[4])
    {
        if constexpr (std::is_same_v<Real, float64>) {
            X = static_cast<float64>(buffer[0]);
            Y = static_cast<float64>(buffer[1]);
            Z = static_cast<float64>(buffer[2]);
        }
        else {
            X = buffer[0];
            Y = buffer[1];
            Z = buffer[2];
        }
    }

    Vec3r(Real x, Real y, Real z) : X(x), Y(y), Z(z) {}

    void Print() { std::printf("{ %f, %f, %f }\n", X, Y, Z); }

    Vec3r operator*(const Real multiplier) const { return Vec3r(X * multiplier, Y * multiplier, Z * multiplier); }
    Vec3r& operator*=(const Real multiplier)
    {
        X *= multiplier;
        Y *= multiplier;
        Z *= multiplier;
        return *this;
    }

    rl::Vector3 ToRL() const
    {
        rl::Vector3 r {};

        r.x = static_cast<float32>(X);
        r.y = static_cast<float32>(Y);
        r.z = static_cast<float32>(Z);

        return r;
    }


public:
    union
    {
        struct
        {
            Real X;
            Real Y;
            Real Z;
            Real W; // Alignment
        };

        Real Values[4];
    };
};
