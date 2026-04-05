#pragma once

#include "Mem.hpp"
#include "Satellite.hpp"

#include <optional>

namespace rl {

#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"

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

        void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material)
        {
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = Color;

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
        Selected.RenderSatellites(sat_model, material);

        if (pPickedSatellite) {
            rl::Color default_color = material.maps[rl::MATERIAL_MAP_DIFFUSE].color;
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::Color(100, 0, 0), 255);

            const rl::Vector3 pos = pPickedSatellite->Position.ToRL();
            rl::Matrix matrix = rl::MatrixMultiply(rl::MatrixScale(2.0, 2.0, 2.0),
                                                   rl::MatrixTranslate(pos.x, pos.y, pos.z));

            rl::DrawMeshInstanced(sat_model, material, &matrix, 1);
            material.maps[rl::MATERIAL_MAP_DIFFUSE].color = default_color;
        }
    }

    uint32 GetSatelliteCount() const override { return Selected.Size(); }


public:
    Component Unselected;
    Component Selected;

    Satellite* pPickedSatellite = nullptr;
};

class NormalFilter : public BaseFilter
{
public:
    void Finalize() override { Sats.Finalize(); }

    void AddSatellite(Satellite& sat, rl::Color color) { Sats.Satellites.push_back(&sat); }

    void UpdateSatellites(std::optional<uint32> use_timestep, bool warp_to = false) override
    {
        Sats.UpdateSatellites(use_timestep, warp_to);
    }
    void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
    {
        Sats.RenderSatellites(sat_model, material);
    }

    uint32 GetSatelliteCount() const override { return Sats.Size(); }


public:
    Component Sats;
};


// class NormalFilter : public BaseFilter
// {
// public:
//     void Finalize() override
//     {
//         for (auto& group : Groups) {
//             group.Finalize();
//         }
//     }

//     void UpdateSatellites(std::optional<uint32> use_timestep) override
//     {
//         for (auto& group : Groups) {
//             group.UpdateSatellites(use_timestep);
//         }
//     }

//     void AddSatellite(Satellite& sat, rl::Color color)
//     {
//         for (Component& group : Groups) {
//             if (group.SeriesHash == sat.Series.GetHash()) {
//                 group.Satellites.push_back(&sat);
//                 return;
//             }
//         }

//         // Group was not found

//         Component group {};
//         group.Satellites.push_back(&sat);
//         group.Color = color;
//         group.SeriesHash = sat.Series.GetHash();

//         Groups.push_back(std::move(group));
//     }

//     void RenderSatellites(const rl::Mesh& sat_model, rl::Material& material) override
//     {
//         for (auto& group : Groups) {
//             group.RenderSatellites(sat_model, material);
//         }
//     }

// public:
//     std::vector<Component> Groups;
// };
