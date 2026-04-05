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
#include "Filter.hpp"
#include "Mem.hpp"
#include "String.hpp"
#include "Types.hpp"
#include "Vec3.hpp"

/// The number of frames it takes for a satellite to reach its destination.
static constexpr uint32 scNumLerpFrames = 250;


// Stars and solar system textures taken from https://www.solarsystemscope.com/textures/, based off of NASA images.
// Earth 3D model has been taken from the NASA website


static constexpr float32 scMouseSensitivity = 0.005f;

static constexpr float32 cPoleLimitOffset = 0.005;
static constexpr float32 cDefaultZoom = 10.0f;
static constexpr float32 cSinglePointZoom = 2.0f;

static constexpr float32 scUIPaddingHorizontal = 20;
static constexpr float32 scUIPaddingVertical = 10;

static constexpr float32 scUIGridHorizontalSplits = 24;
static constexpr float32 scUIGridVerticalSplits = 32;

const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);


class Simulation
{
public:
    Simulation();

    void InitGraphics();

    void CheckControls();
    void Render();

    void Step(int32 by);
    void UpdateTransformations(bool warp_to = false);

    void DrawText(const String& text, int32 cell_x, int32 cell_y, float32 size);
    void UpdateSatellites();
    rl::Vector3 GetSunPosition() { return SunPos.ToRL(); }
    Satellite* CheckForSatPicking();
    void SelectSatellite(Satellite* selection);
    void SetCameraTarget(const Vec3r& target);

    bool IsDefaultFilter() const { return (pFilterInUse == &DefaultFilter); }

    ~Simulation();


public:
    Dataset TmpDataset;

    uint32 TimeFrameIndex = 0;

    rl::Model Earth;

    rl::Shader SatLit;
    rl::Shader EarthLit;
    rl::Shader AtmosphereShader;

    rl::Material SatMaterial;
    rl::Material SelectedMaterial;
    rl::Material EarthMaterial;
    rl::Material AtmosphereMaterial;

    NormalFilter DefaultFilter;
    SelectionFilter SelectedFilter;
    BaseFilter* pFilterInUse = &DefaultFilter;

    Vec3r CameraPosition = Vec3r::sZero;
    Vec3r CameraCenter = Vec3r::sZero;

    float32 Zoom = cDefaultZoom;
    float32 SavedZoom = Zoom;

    double SavedAngleX = 0.01;
    double SavedAngleY = 0.01;

    double AngleX = 0.01;
    double AngleY = 0.01;

    float32 UICellSizeX = 1.0f;
    float32 UICellSizeY = 1.0f;

    rl::Camera Camera {};

    rl::Mesh SatModel;
    rl::Mesh AtmosphereModel;

    rl::Model Skybox;
    rl::Model SunModel;
    rl::Font Font;

    Satellite* pPickedSatellite = nullptr;

    Vec3r SunPos = Vec3r { 100.0f, 50.0f, 0.0f };


    uint64 FrameCount = 0;


    bool bAnimateTimeFrames = false;
};

void Simulation::UpdateSatellites() { pFilterInUse->UpdateSatellites({}, false); }


Simulation::Simulation() {}

Satellite* Simulation::CheckForSatPicking()
{
    rl::Ray ray = rl::GetScreenToWorldRay(rl::GetMousePosition(), Camera);

    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        rl::RayCollision sphere_hit = rl::GetRayCollisionSphere(ray, sat.Position.ToRL(), 0.01f);

        if (sphere_hit.hit) {
            return &sat;
        }
    }

    return nullptr;
}

rl::Color HSVToRGB(float32 H, float32 S, float32 V)
{
    float r, g, b;

    float h = H / 360;
    float s = S / 100;
    float v = V / 100;

    int i = floor(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
    case 0:
        r = v, g = t, b = p;
        break;
    case 1:
        r = q, g = v, b = p;
        break;
    case 2:
        r = p, g = v, b = t;
        break;
    case 3:
        r = p, g = q, b = v;
        break;
    case 4:
        r = t, g = p, b = v;
        break;
    case 5:
        r = v, g = p, b = q;
        break;
    }

    rl::Color color;
    color.r = r * 255;
    color.g = g * 255;
    color.b = b * 255;

    return color;
}

static rl::Color GenerateRandomColor(Hash32 series_hash)
{
    static uint32 seed = 0;
    if (seed == series_hash) {
        return rl::WHITE;
    }

    srand(series_hash);
    seed = series_hash;

    float32 hue = float32(rand() % 100);

    return HSVToRGB(hue, 70, 100);
}

void Simulation::SetCameraTarget(const Vec3r& target)
{
    CameraCenter = target;
    Camera.target = CameraCenter.ToRL();
}

void Simulation::InitGraphics()
{
    int window_width = 900;
    int window_height = 900;

    rl::InitWindow(window_width, window_height, "Satellite Visualization");

    UICellSizeX = (float32(window_width) - scUIPaddingHorizontal) / scUIGridHorizontalSplits;
    UICellSizeY = (float32(window_height) - scUIPaddingVertical) / scUIGridVerticalSplits;

    // Load font
    Font = rl::LoadFontEx(ASSET_BASE_DIR "/font.ttf", 32, 0, 250);

    UpdateTransformations();

    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        sat.CalculateMoveSpeed(scNumLerpFrames);

        DefaultFilter.AddSatellite(sat, GenerateRandomColor(sat.Series.GetHash()));
    }

    DefaultFilter.Finalize();

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

    SelectedMaterial = rl::LoadMaterialDefault();
    SelectedMaterial.shader = SatLit;
    SelectedMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].color = rl::ColorAlpha(rl::Color(255, 165, 0), 255);

    AtmosphereMaterial = rl::LoadMaterialDefault();
    AtmosphereMaterial.shader = AtmosphereShader;

    rl::Texture2D texture = Earth.materials[1].maps[rl::MATERIAL_MAP_DIFFUSE].texture;

    EarthMaterial = rl::LoadMaterialDefault();
    EarthMaterial.shader = EarthLit;
    EarthMaterial.maps[rl::MATERIAL_MAP_DIFFUSE].texture = texture;

    Camera.position = rl::Vector3 { 100.0f, 0.0f, -100.0f };
    SetCameraTarget(Vec3r::sZero);

    Camera.up = rl::Vector3 { 0.0f, 1.0f, 0.0f };
    Camera.fovy = 70.0f;
    Camera.projection = rl::CAMERA_PERSPECTIVE;
    rl::rlSetClipPlanes(0.05f, 2000.0f);

    rl::SetTargetFPS(120);
}


void Simulation::SelectSatellite(Satellite* selected)
{
    if (IsDefaultFilter()) {
        SavedAngleX = AngleX;
        SavedAngleY = AngleY;
        SavedZoom = Zoom;
    }

    if (!selected) {
        SetCameraTarget(Vec3r::sZero);
        Zoom = SavedZoom;

        AngleX = SavedAngleX;
        AngleY = SavedAngleY;

        pFilterInUse = &DefaultFilter;
        pPickedSatellite = nullptr;

        return;
    }

    SelectedFilter.Selected.Satellites.clear();
    SelectedFilter.Unselected.Satellites.clear();

    for (Satellite& sat : TmpDataset.Satellites) {
        if (sat.Series.GetHash() == selected->Series.GetHash()) {
            SelectedFilter.Selected.Satellites.push_back(&sat);
        }
        else {
            SelectedFilter.Unselected.Satellites.push_back(&sat);
        }
    }

    SelectedFilter.Finalize();
    pFilterInUse = &SelectedFilter;

    SelectedFilter.pPickedSatellite = selected;

    SetCameraTarget(selected->Position);

    Zoom = cSinglePointZoom;
}

Simulation::~Simulation()
{
    // RL_FREE(TransformBuffer);

    rl::UnloadModel(Earth);
    rl::UnloadModel(Skybox);
    rl::UnloadModel(SunModel);

    rl::CloseWindow();
}

void Simulation::UpdateTransformations(bool warp_to)
{
    pFilterInUse->UpdateSatellites(std::make_optional(TimeFrameIndex), warp_to);

    // for (int i = 0; i < FilterInUse->Unselected.Size(); i++) {
    //     Satellite* sat = FilterInUse->Unselected.Satellites[i];
    //     const Vec3r& position = sat->MoveToTimeStep(TimeFrameIndex, warp_to);

    //     const rl::Vector3 pos = position.ToRL();
    //     FilterInUse->Unselected.TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    // }

    // for (int i = 0; i < FilterInUse->Selected.Size(); i++) {
    //     Satellite* sat = FilterInUse->Selected.Satellites[i];
    //     const Vec3r& position = sat->MoveToTimeStep(TimeFrameIndex, warp_to);

    //     const rl::Vector3 pos = position.ToRL();
    //     FilterInUse->Selected.TransformBuffer[i] = rl::MatrixTranslate(pos.x, pos.y, pos.z);
    // }
}

void Simulation::Step(int32 by)
{
    if (int32(TimeFrameIndex) + by < 0) {
        TimeFrameIndex = scNumTimeCaptures - 1;
    }
    else if (int32(TimeFrameIndex) + by >= scNumTimeCaptures) {
        TimeFrameIndex = 0;
    }
    else {
        TimeFrameIndex += by;
    }


    bool warp = false;

    if (TimeFrameIndex == 0) {
        warp = true;
    }

    UpdateTransformations(warp);
}

void Simulation::CheckControls()
{
    if (rl::IsKeyPressed(rl::KEY_PERIOD)) { // Greater than symbol
        Step(1);
    }
    else if (rl::IsKeyPressed(rl::KEY_COMMA)) {
        Step(-1);
    }

    if (rl::IsKeyPressed(rl::KEY_P)) {
        bAnimateTimeFrames = !bAnimateTimeFrames;
    }
}

void Simulation::DrawText(const String& text, int32 cell_x, int32 cell_y, float32 size)
{
    rl::DrawTextEx(Font, text.CStr(),
                   rl::Vector2 { float32(cell_x) * UICellSizeX + (scUIPaddingHorizontal / 2.0f),
                                 float32(cell_y) * UICellSizeY + (scUIPaddingVertical / 2.0f) },
                   float32(size), 2, rl::WHITE);
}


void Simulation::Render()
{
    static bool mouse_pressed = false;

    if (bAnimateTimeFrames) {
        UpdateSatellites();


        if (pPickedSatellite) {
            SetCameraTarget(pPickedSatellite->Position);
        }
    }

    if (bAnimateTimeFrames && !(FrameCount % scNumLerpFrames)) {
        Step(1);
    }

    if (AngleX > M_PI) {
        AngleX = -M_PI + 0.0001;
    }
    if (AngleX < -M_PI) {
        AngleX = M_PI - 0.0001;
    }

    AngleY = rl::Clamp(AngleY, -M_PI + cPoleLimitOffset, -cPoleLimitOffset);

    CameraPosition.X = Zoom * cosf(AngleX) * sinf(AngleY);
    CameraPosition.Y = Zoom * cosf(AngleY);
    CameraPosition.Z = Zoom * sinf(AngleY) * sinf(AngleX);


    Camera.position = (CameraPosition + CameraCenter).ToRL();


    {
        float camera_pos[3] = { Camera.position.x, Camera.position.y, Camera.position.z };
        rl::SetShaderValue(SatLit, SatLit.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos, rl::SHADER_UNIFORM_VEC3);
        rl::SetShaderValue(EarthLit, EarthLit.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos, rl::SHADER_UNIFORM_VEC3);
        rl::SetShaderValue(AtmosphereShader, AtmosphereShader.locs[rl::SHADER_LOC_VECTOR_VIEW], camera_pos,
                           rl::SHADER_UNIFORM_VEC3);
    }


    float zoom_movement = rl::GetMouseWheelMove();
    if (abs(zoom_movement) > 0.005f) {
        float32 distance_to_planet = abs((CameraPosition - CameraCenter).Length());

        Zoom += zoom_movement * ((0.005 * distance_to_planet));
    }

    if (rl::IsMouseButtonDown(rl::MOUSE_LEFT_BUTTON)) {
        rl::Vector2 mouse_delta = rl::GetMouseDelta();

        float32 move_magnitude = abs(rl::Vector2Length(mouse_delta));

        if (move_magnitude > 0.005f) {
            AngleX += mouse_delta.x * scMouseSensitivity;
            AngleY += mouse_delta.y * scMouseSensitivity;
            mouse_pressed = false;
        }
    }

    if (rl::IsMouseButtonPressed(rl::MOUSE_LEFT_BUTTON)) {
        mouse_pressed = true;
    }


    if (rl::IsMouseButtonReleased(rl::MOUSE_LEFT_BUTTON) && mouse_pressed) {
        Satellite* sat = CheckForSatPicking();

        pPickedSatellite = sat;
        SelectSatellite(pPickedSatellite);

        mouse_pressed = false;
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

    pFilterInUse->RenderSatellites(SatModel, SatMaterial);

    // if (FilterInUse == &DefaultFilter && FilterInUse->Unselected.Exists()) {
    //     rl::DrawMeshInstanced(SatModel, SatMaterial, FilterInUse->Unselected.TransformBuffer,
    //                           FilterInUse->Unselected.Size());
    // }

    // if (FilterInUse->Selected.Exists()) {
    //     rl::DrawMeshInstanced(SatModel, SelectedMaterial, FilterInUse->Selected.TransformBuffer,
    //                           FilterInUse->Selected.Size());
    // }

    newmat = rl::MatrixScale(1.0, 1.0, 1.0);
    rl::BeginBlendMode(rl::BLEND_ADDITIVE);
    rl::DrawMeshInstanced(AtmosphereModel, AtmosphereMaterial, &newmat, 1);
    rl::EndBlendMode();


    rl::EndMode3D();

    DrawText(String::Fmt("Frame {}/{}", TimeFrameIndex + 1, TmpDataset.NumTimesteps), 0, 0, 32);
    DrawText(String::Fmt("{} Dataset", TmpDataset.Name), 0, 2, 20);
    DrawText(String::Fmt("Count {} / {}", pFilterInUse->GetSatelliteCount(), DefaultFilter.GetSatelliteCount()), 0, 3,
             20);

    if (pPickedSatellite) {
        uint32 picked_x = 14;
        DrawText(String::Fmt("Series {} - Id {}", pPickedSatellite->Series.CStr(), pPickedSatellite->Identifier.CStr()),
                 picked_x, 0, 20);
    }


    rl::EndDrawing();

    ++FrameCount;
}


int main(int argc, char* argv[])
{
    Simulation sim {};

#ifdef DISABLE_BINARY_CACHING
    sim.TmpDataset.LoadFromTLE(ASSET_BASE_DIR "/Datasets/NORAD_TLE.txt");

#else
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
#endif

    sim.InitGraphics();


    while (!rl::WindowShouldClose()) {
        sim.CheckControls();
        sim.Render();
    }


    return 0;
}
