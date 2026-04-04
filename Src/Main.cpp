/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include "Dataset.hpp"

#include <cassert>
#include <cstdlib>

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

/// The number of frames it takes for a satellite to reach its destination.
static constexpr uint32 scNumLerpFrames = 40;


// Stars and solar system textures taken from https://www.solarsystemscope.com/textures/, based off of NASA images.
// Earth 3D model has been taken from the NASA website


static constexpr float32 scMouseSensitivity = 0.005f;

static constexpr float cPoleLimitOffset = 0.005;
static constexpr float cDefaultZoom = 15.0f;


const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);


class Simulation
{
public:
    Simulation();

    void InitGraphics();

    void CheckControls();
    void Render();

    void StepNext();
    void UpdateTransformations(bool warp_to = false);

    void DrawText(const String& text, float32 x, float32 y, float32 size);

    void UpdateSatellites();

    rl::Vector3 GetSunPosition() { return SunPos.ToRL(); }

    ~Simulation();


public:
    Dataset TmpDataset;

    uint32 TimeFrameIndex = 0;

    rl::Model Earth;

    rl::Shader SatLit;
    rl::Shader EarthLit;
    rl::Shader AtmosphereShader;

    rl::Material SatMaterial;
    rl::Material EarthMaterial;
    rl::Material AtmosphereMaterial;

    rl::Matrix* TransformBuffer;

    float Zoom = cDefaultZoom;

    double AngleX = 0.01;
    double AngleY = 0.01;

    rl::Camera Camera {};

    rl::Mesh SatModel;
    rl::Mesh AtmosphereModel;

    rl::Model Skybox;
    rl::Model SunModel;

    Vec3r SunPos = Vec3r { 100.0f, 50.0f, 0.0f };

    bool bAnimateTimeFrames = false;

    uint64 FrameCount = 0;

    rl::Font Font;
};

void Simulation::UpdateSatellites()
{
    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        sat.UpdatePosition(FrameCount, rl::GetFrameTime());

        const rl::Vector3 pos = sat.Position.ToRL();
        TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    }
}


Simulation::Simulation() {}

void Simulation::InitGraphics()
{
    rl::InitWindow(900, 900, "Space");


    // Load font
    Font = rl::LoadFontEx(ASSET_BASE_DIR "/font.ttf", 32, 0, 250);

    TransformBuffer = (rl::Matrix*)RL_CALLOC(TmpDataset.Size(), sizeof(rl::Matrix));
    UpdateTransformations();

    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        sat.CalculateMoveSpeed(scNumLerpFrames);
    }

    Earth = rl::LoadModel(ASSET_BASE_DIR "/Earth.glb");
    Skybox = rl::LoadModel(ASSET_BASE_DIR "/Skybox.glb");
    SunModel = rl::LoadModel(ASSET_BASE_DIR "/Sun.glb");

    SatModel = rl::GenMeshSphere(0.005f, 6.0f, 6.0f);
    AtmosphereModel = rl::GenMeshSphere(3.0f, 24.0f, 24.0f);


    { // Load Atmosphere shader
        AtmosphereShader = rl::LoadShader(ASSET_BASE_DIR "/Shaders/LightingDefault.vs",
                                          ASSET_BASE_DIR "/Shaders/Atmosphere.fs");
        // Get shader locations
        AtmosphereShader.locs[rl::SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(AtmosphereShader, "mvp");
        AtmosphereShader.locs[rl::SHADER_LOC_VECTOR_VIEW] = rl::GetShaderLocation(AtmosphereShader, "viewPos");
        AtmosphereShader.locs[rl::SHADER_LOC_MATRIX_MODEL] = rl::GetShaderLocationAttrib(AtmosphereShader, "matModel");
        AtmosphereShader.locs[rl::SHADER_LOC_MATRIX_VIEW] = rl::GetShaderLocationAttrib(AtmosphereShader, "matView");

        // Set shader value: ambient light level
        int ambientLoc = rl::GetShaderLocation(AtmosphereShader, "ambient");
        rl::SetShaderValue(AtmosphereShader, ambientLoc, (float[4]) { 0.2f, 0.2f, 0.2f, 0.1f },
                           rl::SHADER_UNIFORM_VEC4);

        // Create one light
        rl::CreateLight(rl::LIGHT_DIRECTIONAL, GetSunPosition(), rl::Vector3Zero(), rl::Color { 230, 180, 130 },
                        AtmosphereShader);
    }

    { // Load lighting shader
        SatLit = rl::LoadShader(ASSET_BASE_DIR "/Shaders/LightingInstanced.vs", nullptr);
        // Get shader locations
        SatLit.locs[rl::SHADER_LOC_MATRIX_MVP] = GetShaderLocation(SatLit, "mvp");
        SatLit.locs[rl::SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(SatLit, "viewPos");
        SatLit.locs[rl::SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(SatLit, "instanceTransform");

        // Set shader value: ambient light level
        int ambientLoc = GetShaderLocation(SatLit, "ambient");
        rl::SetShaderValue(SatLit, ambientLoc, (float[4]) { 0.2f, 0.2f, 0.2f, 1.0f }, rl::SHADER_UNIFORM_VEC4);

        // Create one light
        rl::CreateLight(rl::LIGHT_DIRECTIONAL, GetSunPosition(), rl::Vector3Zero(), rl::Color { 230, 180, 130 },
                        SatLit);
    }


    { // Load lighting shader
        EarthLit = rl::LoadShader(ASSET_BASE_DIR "/Shaders/LightingDefault.vs", ASSET_BASE_DIR "/Shaders/Lighting.fs");
        // Get shader locations
        EarthLit.locs[rl::SHADER_LOC_MATRIX_MVP] = rl::GetShaderLocation(EarthLit, "mvp");
        EarthLit.locs[rl::SHADER_LOC_VECTOR_VIEW] = rl::GetShaderLocation(EarthLit, "viewPos");
        EarthLit.locs[rl::SHADER_LOC_MATRIX_MODEL] = rl::GetShaderLocationAttrib(EarthLit, "matModel");

        // Set shader value: ambient light level
        int ambientLoc = rl::GetShaderLocation(EarthLit, "ambient");
        rl::SetShaderValue(EarthLit, ambientLoc, (float[4]) { 0.2f, 0.2f, 0.2f, 1.0f }, rl::SHADER_UNIFORM_VEC4);

        // Create one light
        rl::CreateLight(rl::LIGHT_DIRECTIONAL, GetSunPosition(), rl::Vector3Zero(), rl::Color { 230, 180, 130 },
                        EarthLit);
    }

    // NOTE: We are assigning the intancing shader to material.shader
    // to be used on mesh drawing with DrawMeshInstanced()
    SatMaterial = rl::LoadMaterialDefault();
    SatMaterial.shader = SatLit;
    SatMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::WHITE;


    AtmosphereMaterial = rl::LoadMaterialDefault();
    AtmosphereMaterial.shader = AtmosphereShader;
    // AtmosphereMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::WHITE, 1.0);

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


Simulation::~Simulation()
{
    RL_FREE(TransformBuffer);

    rl::UnloadModel(Earth);
    rl::UnloadModel(Skybox);
    rl::UnloadModel(SunModel);

    rl::CloseWindow();
}

void Simulation::UpdateTransformations(bool warp_to)
{
    for (int i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        const Vec3r& position = sat.MoveToTimeStep(FrameCount, TimeFrameIndex, warp_to);

        const rl::Vector3 pos = position.ToRL();
        TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    }
}

void Simulation::StepNext()
{
    TimeFrameIndex = ((TimeFrameIndex + 1) % scNumTimeCaptures);

    bool warp = false;

    if (TimeFrameIndex == 0) {
        // Reset all goals
        warp = true;
    }

    UpdateTransformations(warp);
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

void Simulation::DrawText(const String& text, float32 x, float32 y, float32 size)
{
    rl::DrawTextEx(Font, text.CStr(), rl::Vector2 { x, y }, float32(size), 2, rl::WHITE);
}


void Simulation::Render()
{
    if (bAnimateTimeFrames) {
        UpdateSatellites();
    }

    if (bAnimateTimeFrames && !(FrameCount % scNumLerpFrames)) {
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
        rl::SetShaderValue(AtmosphereShader, AtmosphereShader.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos,
                           rl::SHADER_UNIFORM_VEC3);
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

    rl::rlDisableBackfaceCulling();
    rl::DrawModel(SunModel, (SunPos * 0.8).ToRL(), 2.0, rl::WHITE);
    rl::rlEnableBackfaceCulling();


    rl::Matrix newmat = rl::MatrixScale(0.0040, 0.0040, 0.0040);
    rl::DrawMeshInstanced(Earth.meshes[0], EarthMaterial, &newmat, 1);

    rl::DrawMeshInstanced(SatModel, SatMaterial, TransformBuffer, TmpDataset.Size());
    newmat = rl::MatrixScale(1.0, 1.0, 1.0);
    // rl::rlDisableBackfaceCulling();
    rl::BeginBlendMode(rl::BLEND_ADDITIVE);
    rl::DrawMeshInstanced(AtmosphereModel, AtmosphereMaterial, &newmat, 1);
    rl::EndBlendMode();
    // rl::rlEnableBackfaceCulling();


    rl::EndMode3D();

    DrawText(String::Fmt("Frame\t{}/{}", TimeFrameIndex + 1, TmpDataset.NumTimesteps), 10, 10, 20);
    DrawText(String::Fmt("{} Dataset", TmpDataset.Name), 10, 30, 14);
    DrawText(String::Fmt("Satellites\t{}", TmpDataset.Size()), 10, 45, 14);

    rl::EndDrawing();

    ++FrameCount;
}


int main(int argc, char* argv[])
{
    Simulation sim {};

    if (argc > 1 && !strcmp(argv[1], "rebuild")) {
        // GeneratePositions();
        printf("Rebuilding satellite cache\n");

        sim.TmpDataset.LoadFromTLE(ASSET_BASE_DIR "/Datasets/NORAD_TLE.txt");
        printf("Loaded %d satellites\n", sim.TmpDataset.Size());


        sim.TmpDataset.SaveToBin(ASSET_BASE_DIR);
    }
    else {
        sim.TmpDataset.LoadFromBin(ASSET_BASE_DIR "/NORAD_TLE.bin");
    }

    sim.InitGraphics();


    while (!rl::WindowShouldClose()) {
        sim.CheckControls();
        sim.Render();
    }


    return 0;
}
