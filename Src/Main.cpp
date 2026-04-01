/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include "Dataset.hpp"

#include <cassert>
#include <cstdlib>
#include <vector>

namespace rl {

#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"
#include "ThirdParty/rlgl.h"

#define RLIGHTS_IMPLEMENTATION
#include "ThirdParty/rlights.h"

} // namespace rl

#include "Config.hpp"
#include "String.hpp"
#include "Types.hpp"
#include "Vec3.hpp"


// Stars and solar system textures taken from https://www.solarsystemscope.com/textures/, based off of NASA images.
// Earth 3D model has been taken from the NASA website


static constexpr float32 scMouseSensitivity = 0.005f;

static constexpr float cPoleLimitOffset = 0.005;
static constexpr float cDefaultZoom = 200.0f;


const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);


class Simulation
{
public:
    Simulation();

    void InitGraphics();

    void CheckControls();
    void Render();

    void StepNext();

    const std::vector<rl::Vector3>& GetPositionsForTimeFrame();

    ~Simulation();


public:
    Dataset TmpDataset;

    uint32 TimeFrameIndex = 0;

    rl::Model Earth;

    rl::Shader SatLit;
    rl::Shader EarthLit;
    rl::Material SatMaterial;
    rl::Material EarthMaterial;

    rl::Matrix* TransformBuffer;

    float Zoom = cDefaultZoom;

    double AngleX = 0.01;
    double AngleY = 0.01;

    rl::Camera Camera {};
    rl::Mesh SatModel;
    rl::Model Skybox;

    bool bAnimateTimeFrames = false;

    uint64 FrameCount = 0;
};


Simulation::Simulation()
{
    TmpDataset.LoadFromBin("../Positions.bin");
    InitGraphics();
}

void Simulation::InitGraphics()
{
    rl::InitWindow(1024, 720, "Space");


    auto& positions = GetPositionsForTimeFrame();

    TransformBuffer = (rl::Matrix*)RL_CALLOC(positions.size(), sizeof(rl::Matrix));

    for (int i = 0; i < positions.size(); i++) {
        const rl::Vector3& pos = positions[i];
        TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    }

    Earth = rl::LoadModel("../Earth.glb");
    Skybox = rl::LoadModel("../Skybox.glb");
    SatModel = rl::GenMeshSphere(0.10f, 6.0f, 6.0f);


    { // Load lighting shader
        SatLit = rl::LoadShader("../Shaders/LightingInstanced.vs", "../Shaders/Lighting.fs");
        // Get shader locations
        SatLit.locs[rl::SHADER_LOC_MATRIX_MVP] = GetShaderLocation(SatLit, "mvp");
        SatLit.locs[rl::SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(SatLit, "viewPos");
        SatLit.locs[rl::SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(SatLit, "instanceTransform");

        // Set shader value: ambient light level
        int ambientLoc = GetShaderLocation(SatLit, "ambient");
        rl::SetShaderValue(SatLit, ambientLoc, (float[4]) { 0.5f, 0.5f, 0.5f, 1.0f }, rl::SHADER_UNIFORM_VEC4);

        // Create one light
        rl::CreateLight(rl::LIGHT_DIRECTIONAL, (rl::Vector3) { 1000.0f, 100.0f, 0.0f }, rl::Vector3Zero(),
                        rl::Color { 230, 200, 150 }, SatLit);
    }


    { // Load lighting shader
        EarthLit = rl::LoadShader("../Shaders/LightingDefault.vs", "../Shaders/Lighting.fs");
        // Get shader locations
        EarthLit.locs[rl::SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(EarthLit, "mvp");
        EarthLit.locs[rl::SHADER_LOC_VECTOR_VIEW] = rl::GetShaderLocation(EarthLit, "viewPos");
        EarthLit.locs[rl::SHADER_LOC_MATRIX_MODEL] = rl::GetShaderLocationAttrib(EarthLit, "matModel");

        // Set shader value: ambient light level
        int ambientLoc = rl::GetShaderLocation(EarthLit, "ambient");
        rl::SetShaderValue(EarthLit, ambientLoc, (float[4]) { 0.5f, 0.5f, 0.5f, 1.0f }, rl::SHADER_UNIFORM_VEC4);

        // Create one light
        rl::CreateLight(rl::LIGHT_DIRECTIONAL, (rl::Vector3) { 1000.0f, 100.0f, 0.0f }, rl::Vector3Zero(),
                        rl::Color { 230, 200, 150 }, EarthLit);
    }

    // NOTE: We are assigning the intancing shader to material.shader
    // to be used on mesh drawing with DrawMeshInstanced()
    SatMaterial = rl::LoadMaterialDefault();
    SatMaterial.shader = SatLit;
    SatMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::WHITE;

    rl::Texture2D texture = Earth.materials[1].maps[rl::MATERIAL_MAP_DIFFUSE].texture;

    EarthMaterial = rl::LoadMaterialDefault();
    EarthMaterial.shader = EarthLit;
    EarthMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].texture = texture;

    Camera.position = rl::Vector3 { 100.0f, 0.0f, -100.0f };
    Camera.target = rl::Vector3 { 0.0f, 0.0f, 0.0f };
    Camera.up = rl::Vector3 { 0.0f, 1.0f, 0.0f };
    Camera.fovy = 65.0f;
    Camera.projection = rl::CAMERA_PERSPECTIVE;
    rl::rlSetClipPlanes(0.05f, 2000.0f);

    // Earth.materials[0].shader = EarthLit;
    // Earth.materials[1] = LoadMaterialDefault();
    // Earth.materials[1].shader = EarthLit;
    // Earth.materials[1].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;


    rl::SetTargetFPS(60);
}


const std::vector<rl::Vector3>& Simulation::GetPositionsForTimeFrame()
{
    assert(TimeFrameIndex < TmpDataset.Size());
    return TmpDataset.GetPositionsForIndex(TimeFrameIndex);
}

Simulation::~Simulation()
{
    RL_FREE(TransformBuffer);

    rl::UnloadModel(Earth);
    rl::UnloadModel(Skybox);

    rl::CloseWindow();
}

void Simulation::StepNext()
{
    TimeFrameIndex = ((TimeFrameIndex + 1) % scNumTimeCaptures);

    auto& positions = GetPositionsForTimeFrame();

    for (int i = 0; i < positions.size(); i++) {
        const rl::Vector3& pos = positions[i];
        TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    }
}

void Simulation::CheckControls()
{
    if (rl::IsKeyPressed(rl::KEY_SPACE)) {
        StepNext();
    }

    if (rl::IsKeyPressed(rl::KEY_P)) {
        bAnimateTimeFrames = !bAnimateTimeFrames;
    }
}

void Simulation::Render()
{
    if (bAnimateTimeFrames && !(FrameCount % 5)) {
        StepNext();
    }

    if (AngleX > M_PI) {
        AngleX = -M_PI + 0.0001;
    }
    if (AngleX < -M_PI) {
        AngleX = M_PI - 0.0001;
    }

    AngleY = rl::Clamp(AngleY, -M_PI + cPoleLimitOffset, -cPoleLimitOffset);

    Camera.position.x = Zoom * cosf(AngleX) * sinf(AngleY);
    Camera.position.y = Zoom * cosf(AngleY);
    Camera.position.z = Zoom * sinf(AngleY) * sinf(AngleX);

    {
        float camera_pos[3] = { Camera.position.x, Camera.position.y, Camera.position.z };
        rl::SetShaderValue(SatLit, SatLit.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos, rl::SHADER_UNIFORM_VEC3);
        rl::SetShaderValue(EarthLit, EarthLit.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos, rl::SHADER_UNIFORM_VEC3);
    }


    float zoom_movement = rl::GetMouseWheelMove();
    if (abs(zoom_movement) > 0.005f) {
        Zoom += zoom_movement * 0.5;
    }

    if (rl::IsMouseButtonDown(rl::MOUSE_LEFT_BUTTON)) {
        rl::Vector2 mouse_delta = rl::GetMouseDelta();

        if (rl::Vector2LengthSqr(mouse_delta) > 0.05f) {
            AngleX += mouse_delta.x * scMouseSensitivity;
            AngleY += mouse_delta.y * scMouseSensitivity;
        }
    }

    rl::BeginDrawing();

    rl::ClearBackground(rl::BLACK);

    rl::BeginMode3D(Camera);


    rl::DrawModel(Skybox, rl::Vector3 { 0.0f, 0.0f, 0.0f }, 1.0 + (Zoom / cDefaultZoom), rl::WHITE);
    // DrawModel(Earth, Vector3 { 0.0f, 0.0f, 0.0f }, 0.065f, WHITE);
    //
    rl::Matrix newmat = rl::MatrixScale(0.065, 0.065, 0.065);
    rl::DrawMeshInstanced(Earth.meshes[0], EarthMaterial, &newmat, 1);
    rl::DrawMeshInstanced(SatModel, SatMaterial, TransformBuffer, GetPositionsForTimeFrame().size());

    rl::EndMode3D();

    rl::DrawText(rl::TextFormat("aframe:   %02i/%02i", TimeFrameIndex + 1, TmpDataset.Size()), 10, 10, 20, rl::WHITE);
    rl::DrawText("dataset: default", 10, 30, 14, rl::WHITE);
    rl::DrawText(rl::TextFormat("sats:    %04i", TmpDataset.GetPositionsForIndex(0).size()), 10, 45, 14, rl::WHITE);

    rl::EndDrawing();

    ++FrameCount;
}


int main(int argc, char* argv[])
{
    if (argc > 1 && !strcmp(argv[1], "rebuild")) {
        // GeneratePositions();
        Dataset::SaveFromTLE("../NORAD_TLE.txt", "../Positions.bin");
    }

    Simulation sim {};

    while (!rl::WindowShouldClose()) {
        sim.CheckControls();
        sim.Render();
    }


    return 0;
}
