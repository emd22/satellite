#pragma once

#include "Mem.hpp"
#include "Satellite.hpp"

#include <optional>

namespace rl {

#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"
#include "ThirdParty/rlgl.h"

} // namespace rl

class BaseFilter
{
public:
    struct Component
    {
        void UpdateTransforms()
        {
            for (int i = 0; i < Satellites.size(); i++) {
                Satellite* sat = Satellites[i];
                if (!sat) {
                    continue;
                }

                const Vec3r& position = sat->Position;

                const rl::Vector3 pos = position.ToRL();
                TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
            }
        }

        void Finalize()
        {
            if (TransformBuffer != nullptr) {
                TransformBuffer = Mem::Realloc<rl::Matrix>(TransformBuffer, Satellites.size() * sizeof(rl::Matrix));
            }
            else {
                TransformBuffer = Mem::Alloc<rl::Matrix>(Satellites.size() * sizeof(rl::Matrix));
            }

            UpdateTransforms();
        }

        void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to)
        {
            for (int i = 0; i < Size(); i++) {
                Satellite* sat = Satellites[i];
                if (use_timestep.has_value()) {
                    sat->MoveToTimeStep(use_timestep.value(), warp_to);
                }

                sat->UpdatePosition();

                const rl::Vector3 pos = sat->Position.ToRL();
                TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
            }
        }

        void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) const
        {
            // material.maps[rl::MATERIAL_MAP_DIFFUSE].color = Color;

            if (Exists()) {
                rl::DrawMeshInstanced(sat_model, material, TransformBuffer, Size());
            }
        }

        uint32 Size() const { return Satellites.size(); }
        bool Exists() const { return TransformBuffer != nullptr; }

        ~Component() { Mem::Free(TransformBuffer); }

        std::vector<Satellite*> Satellites;
        rl::Matrix* TransformBuffer = nullptr;

        Hash32 SeriesHash = HashNull32;

        rl::Color Color = rl::WHITE;
    };

public:
    virtual void Finalize() {}
    virtual void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to) {}
    virtual void MoveSatellitesToTimeStep() {}
    virtual void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) {}
    virtual Satellite* RaycastToSatellite(rl::Ray& ray) { return nullptr; }
    virtual uint32 GetSatelliteCount() const { return 0; }
};

class SelectionFilter : public BaseFilter
{
public:
    void Finalize() override
    {
        Unselected.Finalize();
        Selected.Finalize();
    }

    void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to = false) override
    {
        Selected.UpdateSatellites(use_timestep, warp_to);
        Unselected.UpdateSatellites(use_timestep, warp_to);
    }

    void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
    {
        rl::Color default_color = material.maps[rl::MATERIAL_MAP_DIFFUSE].color;

        if (bShowInactive) {
            Unselected.RenderSatellites(sat_model, material);
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::Color(200, 165, 0), 255);
            Selected.RenderSatellites(sat_model, material);
        }
        else {
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::Color(200, 165, 0), 255);
            Selected.RenderSatellites(sat_model, material);
        }

        if (pPickedSatellite) {
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::Color(200, 0, 0), 100);

            const rl::Vector3 pos = pPickedSatellite->Position.ToRL();
            rl::Matrix matrix = rl::MatrixMultiply(rl::MatrixScale(3.0, 3.0, 3.0),
                                                   rl::MatrixTranslate(pos.x, pos.y, pos.z));

            rl::BeginBlendMode(rl::BLEND_ADDITIVE);
            rl::DrawMeshInstanced(sat_model, material, &matrix, 1);
            rl::EndBlendMode();
        }

        material.maps[rl::MATERIAL_MAP_DIFFUSE].color = default_color;
    }

    Satellite* RaycastToSatellite(rl::Ray& ray) override
    {
        for (uint32 i = 0; i < Selected.Size(); i++) {
            Satellite* sat = Selected.Satellites[i];

            rl::RayCollision sphere_hit = rl::GetRayCollisionSphere(ray, sat->Position.ToRL(), 0.02f);

            if (sphere_hit.hit) {
                return sat;
            }
        }
        return nullptr;
    }


    uint32 GetSatelliteCount() const override { return Selected.Size(); }


public:
    Component Unselected;
    Component Selected;

    Satellite* pPickedSatellite = nullptr;

    bool bShowInactive = false;
};

class NormalFilter : public BaseFilter
{
public:
    void Create(uint32 size)
    {
        if (ColorBuffer != nullptr) {
            ColorBuffer = Mem::Realloc<rl::Vector4>(ColorBuffer, size * sizeof(rl::Vector4));
        }
        else {
            ColorBuffer = Mem::Alloc<rl::Vector4>(size * sizeof(rl::Vector4));
        }
    }

    void Finalize(const rl::Mesh& sat_model, rl::Shader shader)
    {
        Sats.Finalize();

        // int colorLoc = GetShaderLocationAttrib(shader, "instanceColor");

        // unsigned int colorVboId = rlLoadVertexBuffer(ColorBuffer, Sats.Size() * sizeof(rl::Vector4), false);

        // rl::rlEnableVertexArray(sat_model.vaoId);
        // rl::rlEnableVertexAttribute(colorLoc);
        // rl::rlSetVertexAttribute(colorLoc, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        // rl::rlSetVertexAttributeDivisor(colorLoc, 1);
        // rl::rlDisableVertexArray();
    }

    void AddSatellite(Satellite& sat, rl::Color color)
    {
        Sats.Satellites.push_back(&sat);

        rl::Vector4& vc = ColorBuffer[Sats.Size() - 1];
        vc.x = 255.0f;
        vc.y = 255.0;
        vc.z = 255.0;
        vc.w = 255.0f;
    }

    void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to = false) override
    {
        Sats.UpdateSatellites(use_timestep, warp_to);
    }
    void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
    {
        Sats.RenderSatellites(sat_model, material);
    }

    uint32 GetSatelliteCount() const override { return Sats.Size(); }


    Satellite* RaycastToSatellite(rl::Ray& ray) override
    {
        for (uint32 i = 0; i < Sats.Size(); i++) {
            Satellite* sat = Sats.Satellites[i];

            rl::RayCollision sphere_hit = rl::GetRayCollisionSphere(ray, sat->Position.ToRL(), 0.02f);

            if (sphere_hit.hit) {
                return sat;
            }
        }
        return nullptr;
    }


public:
    Component Sats;

    rl::Vector4* ColorBuffer = nullptr;

    uint32 TransformVBO;
    uint32 ColorVBO;
};


// class NormalFilter : public BaseFilter
// {
// public:
//     void Finalize() override
//     {
//         for (auto& group : Groups) {
//             group.second.Finalize();
//         }
//     }

//     void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to = false) override
//     {
//         for (auto& group : Groups) {
//             group.second.UpdateSatellites(use_timestep, warp_to);
//         }
//     }

//     void AddSatellite(Satellite& sat, rl::Color color)
//     {
//         auto found_group = Groups.find(sat.Series.GetHash());

//         if (found_group != Groups.end()) {
//             found_group->second.Satellites.push_back(&sat);
//             return;
//         }
//         // Group was not found

//         Component group {};
//         group.Satellites.push_back(&sat);
//         group.Color = color;

//         Groups[sat.Series.GetHash()] = std::move(group);
//     }

//     void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
//     {
//         for (auto& group : Groups) {
//             group.second.RenderSatellites(sat_model, material);
//         }

//         // printf("Submitting %zu Draw calls\n", Groups.size());
//     }

//     uint32 GetSatelliteCount() const override
//     {
//         uint32 size = 0;
//         for (auto& group : Groups) {
//             size += group.second.Size();
//         }
//         return size;
//     }


//     Satellite* RaycastToSatellite(rl::Ray& ray) override
//     {
//         for (auto& group : Groups) {
//             for (uint32 i = 0; i < group.second.Size(); i++) {
//                 Satellite* sat = group.second.Satellites[i];

//                 rl::RayCollision sphere_hit = rl::GetRayCollisionSphere(ray, sat->Position.ToRL(), 0.02f);

//                 if (sphere_hit.hit) {
//                     return sat;
//                 }
//             }
//         }


//         return nullptr;
//     }

// public:
//     std::unordered_map<Hash32, Component, Hash32Stl> Groups;
// };
