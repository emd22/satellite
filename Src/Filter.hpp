#pragma once

#include "Mem.hpp"
#include "RenderHelpers.hpp"
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
                Render::DrawMeshInstanced_Colors(sat_model, material, TransformBuffer, ColorBuffer, Size());
            }
        }

        uint32 Size() const { return Satellites.size(); }
        bool Exists() const { return TransformBuffer != nullptr; }

        ~Component() { Mem::Free(TransformBuffer); }

        std::vector<Satellite*> Satellites;
        rl::Matrix* TransformBuffer = nullptr;
        rl::Color* ColorBuffer = nullptr;

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
    void Create(uint32 size)
    {
        Selected.ColorBuffer = Mem::Alloc<rl::Color>(size * sizeof(rl::Color));
        Unselected.ColorBuffer = Mem::Alloc<rl::Color>(size * sizeof(rl::Color));
    }

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


    void AddSatellite(Satellite& sat, rl::Color color, bool selected)
    {
        if (selected) {
            Selected.Satellites.push_back(&sat);

            rl::Color& vc = Selected.ColorBuffer[Selected.Size() - 1];
            vc = color;
        }
        else {
            Unselected.Satellites.push_back(&sat);

            rl::Color& vc = Unselected.ColorBuffer[Unselected.Size() - 1];
            vc = color;
        }
    }

    void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
    {
        rl::Color default_color = material.maps[rl::MATERIAL_MAP_DIFFUSE].color;

        if (bShowInactive) {
            Unselected.RenderSatellites(sat_model, material);
            Selected.RenderSatellites(sat_model, material);
        }
        else {
            Selected.RenderSatellites(sat_model, material);
        }

        if (pPickedSatellite) {
            const rl::Vector3 pos = pPickedSatellite->Position.ToRL();
            rl::Matrix matrix = rl::MatrixMultiply(rl::MatrixScale(3.0, 3.0, 3.0),
                                                   rl::MatrixTranslate(pos.x, pos.y, pos.z));

            rl::BeginBlendMode(rl::BLEND_ADDITIVE);
            // rl::DrawMeshInstanced(sat_model, material, &matrix, 1);
            rl::Color picked_color = rl::ColorAlpha(rl::Color(150, 0, 0), 255);
            Render::DrawMeshInstanced_Colors(sat_model, material, &matrix, &picked_color, 1);

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
        if (Sats.ColorBuffer != nullptr) {
            Sats.ColorBuffer = Mem::Realloc<rl::Color>(Sats.ColorBuffer, size * sizeof(rl::Color));
        }
        else {
            Sats.ColorBuffer = Mem::Alloc<rl::Color>(size * sizeof(rl::Color));
        }
    }

    void Finalize(const rl::Mesh& sat_model, rl::Shader shader) { Sats.Finalize(); }

    void AddSatellite(Satellite& sat, rl::Color color)
    {
        Sats.Satellites.push_back(&sat);

        rl::Color& vc = Sats.ColorBuffer[Sats.Size() - 1];
        vc = color;
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
