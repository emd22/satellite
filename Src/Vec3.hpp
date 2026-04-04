#pragma once

#include "Config.hpp"
#include "Types.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <type_traits>

#define FClamp(x, minv, maxv) std::fminf(std::fmaxf(x, maxv), minv)


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

    Vec3r(float32 scalar)
    {
        if constexpr (std::is_same_v<Real, float64>) {
            float64 d = static_cast<float64>(scalar);
            X = d;
            Y = d;
            Z = d;
        }
        else {
            X = scalar;
            Y = scalar;
            Z = scalar;
        }
    }

    Vec3r(Real x, Real y, Real z) : X(x), Y(y), Z(z) {}

    void Print() { std::printf("{ %f, %f, %f }\n", X, Y, Z); }

    float32 Dot(const Vec3r& other) const { return (X * other.X + Y * other.Y + Z * other.Z); }

    FORCE_INLINE Vec3r operator+(const Vec3r& other) const { return Vec3r(X + other.X, Y + other.Y, Z + other.Z); }
    FORCE_INLINE Vec3r& operator+=(const Vec3r& other)
    {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }
    FORCE_INLINE Vec3r operator-(const Vec3r& other) const { return Vec3r(X - other.X, Y - other.Y, Z - other.Z); }
    FORCE_INLINE Vec3r operator/(const Vec3r& other) const { return Vec3r(X / other.X, Y / other.Y, Z / other.Z); }

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

    float32 Length() const { return sqrt(Dot(*this)); }

    Vec3r Normalize() const { return (*this) / Vec3r(Length()); }

    Vec3r SLerp(const Vec3r& dest, float32 t) const
    {
        // Normalize inputs
        Vec3r v0 = Normalize();
        Vec3r v1 = dest.Normalize();

        // Compute the cosine of the angle between them
        float32 dp = Dot(v1);

        // Clamp to avoid precision issues
        dp = FClamp(dp, -1.0f, 1.0f);

        float theta = acosf(dp) * t;

        // Compute perpendicular component
        Vec3r relative = ((v1 - (v0 * dp))).Normalize();

        // Slerp result
        return ((v0 * cosf(theta)) + (relative * sinf(theta)));
    }


    FORCE_INLINE Vec3r& LerpIP(const Vec3r& dest, const float step)
    {
        X = std::lerp(X, dest.X, step);
        Y = std::lerp(Y, dest.Y, step);
        Z = std::lerp(Z, dest.Z, step);

        return *this;
    }


    FORCE_INLINE Vec3r Lerp(const Vec3r& dest, const float step) const
    {
        Vec3r x = *this;

        x.X = std::lerp(X, dest.X, step);
        x.Y = std::lerp(Y, dest.Y, step);
        x.Z = std::lerp(Z, dest.Z, step);

        return x;
    }

    FORCE_INLINE Vec3r SmoothInterpolate(const Vec3r& dest, const float speed, const float delta_time) const
    {
        Vec3r x = *this;
        x.LerpIP(dest, 1.0f - std::expf(-speed * delta_time));
        // *this = SLerp(dest, 1.0 - std::expf(-speed * delta_time));
        return x;
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
