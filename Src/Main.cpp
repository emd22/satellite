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
// Raylib + Imgui
#include "rlImGui.hpp"


} // namespace rl

#include "Config.hpp"
#include "Filter.hpp"
#include "Mem.hpp"
#include "String.hpp"
#include "Types.hpp"
#include "Vec3.hpp"

#include "ThirdParty/imgui/imgui.h"

/// The number of frames it takes for a satellite to reach its destination.
static uint32 sNumLerpFrames = 240;

// Stars and solar system textures taken from https://www.solarsystemscope.com/textures/, based off of NASA images.
// Earth 3D model has been taken from the NASA website


static constexpr float32 scZoomSensitivity = 10.0f;
static constexpr float32 scMouseSensitivity = 0.005f;

static constexpr float32 cPoleLimitOffset = 0.005;
static constexpr float32 cDefaultZoom = 10.0f;
static constexpr float32 cSinglePointZoom = 2.0f;

static constexpr float32 scUIPaddingHorizontal = 20;
static constexpr float32 scUIPaddingVertical = 10;

static constexpr float32 scUIGridHorizontalSplits = 24;
static constexpr float32 scUIGridVerticalSplits = 32;

static constexpr float32 scDefaultMinZoom = 3.85f;
static constexpr float32 scDefaultMaxZoom = 45.0f;

static constexpr float32 scSelectedMinZoom = 1.0f;
static constexpr float32 scSelectedMaxZoom = 4.5f;

const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);


class Simulation
{
public:
    Simulation();

    void InitGraphics();

    void CheckControls();
    void Render();

    void Step(int32 by, bool warp = false);
    void UpdateTransformations(bool warp_to = false);

    void DrawText(const String& text, int32 cell_x, int32 cell_y, float32 size);
    void UpdateSatellites();
    rl::Vector3 GetSunPosition() { return SunPos.ToRL(); }
    Satellite* CheckForSatPicking();
    void SelectSatellite(Satellite* selection);
    void SetCameraTarget(const Vec3r& target);

    bool IsDefaultFilter() const { return (pFilterInUse == &DefaultFilter); }
    void UpdateLerpSpeed(uint32 lerp_frames);

    rl::Color GetSatColor(Hash32 series_hash);

    void DisplayControls();
    void DisplayControlsDefault();

    void DeselectSatellites();

    void UpdateSpeed(bool forward)
    {
        static constexpr int32 scLerpStep = 12;

        if (forward && (int32(sNumLerpFrames) - scLerpStep) >= 10) {
            UpdateLerpSpeed(sNumLerpFrames - scLerpStep);
        }
        else if (!forward && (int32(sNumLerpFrames) + scLerpStep) <= 300) {
            UpdateLerpSpeed(sNumLerpFrames + scLerpStep);
        }
    }

    float32 GetZoomPercent() const
    {
        float32 min_zoom = scSelectedMinZoom;
        float32 max_zoom = scSelectedMaxZoom;

        if (IsDefaultFilter()) {
            min_zoom = scDefaultMinZoom;
            max_zoom = scDefaultMaxZoom;
        }

        return 100 - ((Zoom - min_zoom) / (max_zoom - min_zoom)) * 100.0f;
    }

    float32 GetSpeedPercent() const { return (1.0f - (float32(sNumLerpFrames - 10) / (300.0f - 10.0f))) * 100.0f; }

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
    // rl::Font Font;
    ImFont* FontNormal;
    ImFont* FontLarge;

    uint32 WindowWidth = 0;
    uint32 WindowHeight = 0;

    Satellite* pPickedSatellite = nullptr;

    Vec3r SunPos = Vec3r { 100.0f, 50.0f, 0.0f };


    uint64 FrameCount = 0;

    bool bAnimateTimeFrames = false;
    bool bHideSatellites = false;
    bool bShowControls = false;

    // HACKY but I am running out of time
    bool bTemporarilyShowDebris = false;

    std::unordered_map<Hash32, rl::Color, Hash32Stl> ColorMap;
};

void Simulation::DisplayControls()
{
    static constexpr ImGuiWindowFlags scWindowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                                                      ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Controls", &bShowControls, scWindowFlags)) {
        ImGui::PushFont(FontNormal);

        DisplayControlsDefault();


        ImGui::PopFont();
    }

    ImGui::End();
}


void Simulation::DisplayControlsDefault()
{
    ImGui::LabelText("Select", "%s", "[LeftClick]");

    ImGui::LabelText(bAnimateTimeFrames ? "Pause Animation" : "Play Animation", "%s", "[Space]");
    ImGui::LabelText(bHideSatellites ? "Show Satellites" : "Hide Satellites", "%s", "[H]");

    if (pPickedSatellite) {
        ImGui::LabelText("Deselect", "%s", "[Escape]");
        ImGui::LabelText("Cycle Filter", "%s", "[I]");
    }
}


void Simulation::UpdateSatellites() { pFilterInUse->UpdateSatellites({}, false); }


Simulation::Simulation() {}

Satellite* Simulation::CheckForSatPicking()
{
    rl::Ray ray = rl::GetScreenToWorldRay(rl::GetMousePosition(), Camera);

    return pFilterInUse->RaycastToSatellite(ray);
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
    color.a = 255;

    return color;
}

rl::Color Simulation::GetSatColor(Hash32 series_hash)
{
    auto it = ColorMap.find(series_hash);

    if (it != ColorMap.end()) {
        return it->second;
    }


    float32 hue = float32(rand() % 360);

    rl::Color color = HSVToRGB(hue, 40, 100);
    ColorMap[series_hash] = color;

    return color;
}

void Simulation::SetCameraTarget(const Vec3r& target)
{
    CameraCenter = target;
    Camera.target = CameraCenter.ToRL();
}

void Simulation::UpdateLerpSpeed(uint32 lerp_frames)
{
    sNumLerpFrames = lerp_frames;

    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        sat.CalculateMoveSpeed(sNumLerpFrames);
    }
}

void Simulation::InitGraphics()
{
    WindowWidth = 900;
    WindowHeight = 900;


    rl::SetConfigFlags(rl::FLAG_WINDOW_RESIZABLE);
    rl::InitWindow(WindowWidth, WindowHeight, "Satellite Visualization");

    UICellSizeX = (float32(WindowWidth) - scUIPaddingHorizontal) / scUIGridHorizontalSplits;
    UICellSizeY = (float32(WindowHeight) - scUIPaddingVertical) / scUIGridVerticalSplits;

    // Load font
    // Font = rl::LoadFontEx(ASSET_BASE_DIR "/font.ttf", 96, 0, 250);


    UpdateTransformations();

    DefaultFilter.Create(TmpDataset.Size() + 1);
    SelectedFilter.Create(TmpDataset.Size() + 1);

    for (uint32 i = 0; i < TmpDataset.Size(); i++) {
        Satellite& sat = TmpDataset.GetSatellite(i);
        sat.CalculateMoveSpeed(sNumLerpFrames);

        sat.MoveToTimeStep(0, true);

        DefaultFilter.AddSatellite(sat, GetSatColor(sat.Series.GetHash()));
    }

    DefaultFilter.Finalize(SatModel, SatMaterial.shader);

    Earth = rl::LoadModel(ASSET_BASE_DIR "/Earth.glb");
    Skybox = rl::LoadModel(ASSET_BASE_DIR "/Skybox.glb");
    SunModel = rl::LoadModel(ASSET_BASE_DIR "/Sun.glb");

    SatModel = rl::GenMeshSphere(0.005f, 6.0f, 6.0f);
    AtmosphereModel = rl::GenMeshSphere(3.0f, 24.0f, 24.0f);

    float ambient_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };


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
        rl::SetShaderValue(AtmosphereShader, ambientLoc, ambient_color, rl::SHADER_UNIFORM_VEC4);

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
        SatLit.locs[rl::SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(SatLit, "instanceColor");

        // Set shader value: ambient light level
        int ambientLoc = GetShaderLocation(SatLit, "ambient");
        rl::SetShaderValue(SatLit, ambientLoc, ambient_color, rl::SHADER_UNIFORM_VEC4);

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
        rl::SetShaderValue(EarthLit, ambientLoc, ambient_color, rl::SHADER_UNIFORM_VEC4);

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
    rl::SetExitKey(rl::KEY_NULL);

    rl::rlImGuiSetup(true);


    ImGuiIO& io = ImGui::GetIO();

    io.ConfigWindowsMoveFromTitleBarOnly = true;

    FontNormal = io.Fonts->AddFontFromFileTTF(ASSET_BASE_DIR "/font.ttf", 16.0f);
    FontLarge = io.Fonts->AddFontFromFileTTF(ASSET_BASE_DIR "/font.ttf", 32.0f);
}


void Simulation::SelectSatellite(Satellite* selected)
{
    if (!selected) {
        return;
    }

    if (IsDefaultFilter()) {
        SavedAngleX = AngleX;
        SavedAngleY = AngleY;
        SavedZoom = Zoom;
    }

    // YUCKY
    if (selected->IsDebris() && !SelectedFilter.bShowDebris) {
        bTemporarilyShowDebris = true;
        SelectedFilter.bShowDebris = true;
    }

    SelectedFilter.Selected.Satellites.clear();
    SelectedFilter.Unselected.Satellites.clear();
    SelectedFilter.Debris.Satellites.clear();

    for (Satellite& sat : TmpDataset.Satellites) {
        if (sat.Series.GetHash() == selected->Series.GetHash()) {
            SelectedFilter.AddSatellite(sat, GetSatColor(sat.Series.GetHash()), true);
        }
        else {
            SelectedFilter.AddSatellite(sat, GetSatColor(sat.Series.GetHash()), false);
        }
    }

    SelectedFilter.Finalize();
    pFilterInUse = &SelectedFilter;

    SelectedFilter.pPickedSatellite = selected;

    SetCameraTarget(selected->Position);

    Zoom = cSinglePointZoom;
}


void Simulation::DeselectSatellites()
{
    SetCameraTarget(Vec3r::sZero);
    Zoom = SavedZoom;

    AngleX = SavedAngleX;
    AngleY = SavedAngleY;

    pFilterInUse = &DefaultFilter;
    pPickedSatellite = nullptr;

    if (bTemporarilyShowDebris) {
        // Revert back to the original state
        SelectedFilter.bShowDebris = false;
    }
}

Simulation::~Simulation()
{
    // RL_FREE(TransformBuffer);

    rl::UnloadModel(Earth);
    rl::UnloadModel(Skybox);
    rl::UnloadModel(SunModel);

    rl::rlImGuiShutdown();

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

void Simulation::Step(int32 by, bool warp)
{
    if (int32(TimeFrameIndex) + by < 0) {
        TimeFrameIndex = scNumTimeCaptures - 1;
    }
    else if (int32(TimeFrameIndex) + by >= scNumTimeCaptures - 1) {
        TimeFrameIndex = 0;
    }
    else {
        TimeFrameIndex += by;
    }

    if (TimeFrameIndex == 0) {
        warp = true;
    }

    UpdateTransformations(warp);
}

void Simulation::CheckControls()
{
    if (rl::IsKeyPressed(rl::KEY_PERIOD)) { // Greater than symbol
        Step(1, true);
    }
    else if (rl::IsKeyPressed(rl::KEY_COMMA)) {
        Step(-1, true);
    }


    if (rl::IsKeyPressed(rl::KEY_EQUAL)) {
        UpdateSpeed(true);
    }
    else if (rl::IsKeyPressed(rl::KEY_MINUS)) {
        UpdateSpeed(false);
    }

    if (rl::IsKeyPressed(rl::KEY_SPACE)) {
        bAnimateTimeFrames = !bAnimateTimeFrames;
    }

    if (rl::IsKeyPressed(rl::KEY_H)) {
        bHideSatellites = !bHideSatellites;
    }

    if (rl::IsKeyPressed(rl::KEY_I)) {
        // Cycle should go Selection Only -> All -> Unselected Only

        switch (SelectedFilter.Filter) {
        case SelectionFilter::FilterType::eOnlySelected:
            SelectedFilter.Filter = SelectionFilter::FilterType::eShowAll;
            break;
        case SelectionFilter::FilterType::eShowAll:
            SelectedFilter.Filter = SelectionFilter::FilterType::eOnlyUnselected;
            break;
        case SelectionFilter::FilterType::eOnlyUnselected:
            SelectedFilter.Filter = SelectionFilter::FilterType::eOnlySelected;
            break;
        }
    }

    if (rl::IsKeyPressed(rl::KEY_ESCAPE)) {
        DeselectSatellites();
    }
}

void Simulation::DrawText(const String& text, int32 cell_x, int32 cell_y, float32 size)
{
    // return;
    // rl::DrawTextEx(Font, text.CStr(),
    //                rl::Vector2 { float32(cell_x) * UICellSizeX + (scUIPaddingHorizontal / 2.0f),
    //                              float32(cell_y) * UICellSizeY + (scUIPaddingVertical / 2.0f) },
    //                float32(size), 2, rl::WHITE);
}


void Simulation::Render()
{
    if (rl::GetScreenWidth() != WindowWidth || rl::GetScreenHeight() != WindowHeight) {
        WindowWidth = rl::GetScreenWidth();
        WindowHeight = rl::GetScreenHeight();
        UICellSizeX = (float32(WindowWidth) - scUIPaddingHorizontal) / scUIGridHorizontalSplits;
        UICellSizeY = (float32(WindowHeight) - scUIPaddingVertical) / scUIGridVerticalSplits;
    }

    static bool mouse_pressed = false;

    if (bAnimateTimeFrames) {
        UpdateSatellites();


        if (pPickedSatellite) {
            SetCameraTarget(pPickedSatellite->Position);
        }
    }

    if (bAnimateTimeFrames && !(FrameCount % sNumLerpFrames)) {
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


    rl::BeginDrawing();

    rl::ClearBackground(rl::BLACK);

    rl::BeginMode3D(Camera);

    rl::DrawModel(Skybox, rl::Vector3 { 0.0f, 0.0f, 0.0f }, 1.0 + (Zoom / cDefaultZoom), rl::WHITE);

    rl::rlDisableBackfaceCulling();
    rl::DrawModel(SunModel, (SunPos * 0.8).ToRL(), 2.0, rl::WHITE);
    rl::rlEnableBackfaceCulling();


    rl::Matrix newmat = rl::MatrixScale(0.0040, 0.0040, 0.0040);
    rl::DrawMeshInstanced(Earth.meshes[0], EarthMaterial, &newmat, 1);

    if (!bHideSatellites) {
        pFilterInUse->RenderSatellites(SatModel, SatMaterial);
    }


    newmat = rl::MatrixScale(1.0, 1.0, 1.0);
    rl::BeginBlendMode(rl::BLEND_ADDITIVE);
    rl::DrawMeshInstanced(AtmosphereModel, AtmosphereMaterial, &newmat, 1);
    rl::EndBlendMode();

    rl::EndMode3D();


    rl::rlImGuiBegin();

    static constexpr ImGuiWindowFlags scWindowFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize |
                                                      ImGuiWindowFlags_AlwaysAutoResize;
    bool open = true;

    ImGui::SetNextWindowSize(ImVec2(300, 500));

    if (ImGui::Begin("Tools", nullptr, scWindowFlags)) {
        ImGui::PushFont(FontNormal);
        ImGui::PushFont(FontLarge);
        ImGui::TextUnformatted(String::Fmt("Frame {}/{}", TimeFrameIndex + 1, TmpDataset.NumTimesteps).CStr());
        ImGui::PopFont();

        if (ImGui::Button("<")) {
            Step(-1, true);
        }

        ImGui::SameLine();

        if (ImGui::Button(bAnimateTimeFrames ? "Pause" : "Play")) {
            bAnimateTimeFrames = !bAnimateTimeFrames;
        }
        ImGui::SameLine();

        if (ImGui::Button(">")) {
            Step(1, true);
        }

        ImGui::SameLine();

        ImGui::Dummy(ImVec2(20.0f, 00.0f));
        ImGui::SameLine();


        {
            // Increase or decrease speed
            if (ImGui::Button("-")) {
                UpdateSpeed(false);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(String::Fmt("{:.00f}%", GetSpeedPercent()).CStr());
            ImGui::SameLine();

            if (ImGui::Button("+")) {
                UpdateSpeed(true);
            }
        }

        ImGui::SeparatorText("Information");
        {
            ImGui::LabelText("Zoom", "%.02f%%", (GetZoomPercent()));

            ImGui::LabelText("Dataset", "%s", TmpDataset.Name.CStr());
            ImGui::LabelText("Count", "%d / %d", pFilterInUse->GetSatelliteCount(), DefaultFilter.GetSatelliteCount());
        }


        ImGui::Checkbox("Show Debris", &pFilterInUse->bShowDebris);
        ImGui::Checkbox("Show Controls", &bShowControls);

        ImGui::PopFont();
    }
    ImGui::End();

    open = pPickedSatellite != nullptr;

    ImGui::SetNextWindowSize(ImVec2(300, 350));
    if (ImGui::Begin("Detail", nullptr, scWindowFlags)) {
        ImGui::PushFont(FontNormal);

        if (pPickedSatellite) {
            ImGui::LabelText("Name", "%s", pPickedSatellite->Name.CStr());
            ImGui::LabelText("Series", "%s", pPickedSatellite->Series.CStr());
            ImGui::LabelText("Id", "%s", pPickedSatellite->Identifier.CStr());
            ImGui::LabelText("NORAD ID", "%d", pPickedSatellite->NoradId);
            ImGui::LabelText("Launched", "'%02d", pPickedSatellite->LaunchYear);

            // ImGui::TextUnformatted(String::Fmt("Series {}", pPickedSatellite->Series).CStr());
            // ImGui::TextUnformatted(String::Fmt("Id {}", pPickedSatellite->Identifier).CStr());

            auto& pos = pPickedSatellite->Position;
            float pos_v[3] = { static_cast<float32>(pos.X), static_cast<float32>(pos.Y), static_cast<float32>(pos.Z) };
            ImGui::InputFloat3("Position", pos_v);

            bool show_selected = SelectedFilter.CanShowSelected();
            bool show_unselected = SelectedFilter.CanShowUnselected();

            ImGui::Checkbox("Show Unselected", &show_unselected);
            ImGui::Checkbox("Show Selected", &show_selected);

            if (show_selected != SelectedFilter.CanShowSelected() ||
                show_unselected != SelectedFilter.CanShowUnselected()) {
                SelectedFilter.SetFilterType(show_selected, show_unselected);
            }

            if (ImGui::Button("Deselect", ImVec2(150, 25))) {
                DeselectSatellites();
            }
        }
        ImGui::PopFont();
    }

    if (bShowControls) {
        DisplayControls();
    }

    ImGui::End();

    open = true;


    rl::rlImGuiEnd();


    rl::EndDrawing();

    ImGuiIO& imgui_io = ImGui::GetIO();

    if (!ImGui::IsAnyItemHovered()) {
        float zoom_movement = rl::GetMouseWheelMove();
        if (abs(zoom_movement) > 0.005f) {
            float32 distance_to_planet = abs((CameraPosition - CameraCenter).Length());

            Zoom += zoom_movement * ((scZoomSensitivity * distance_to_planet)) * rl::GetFrameTime();

            if (IsDefaultFilter()) {
                Zoom = rl::Clamp(Zoom, scDefaultMinZoom, scDefaultMaxZoom);
            }
            else {
                Zoom = rl::Clamp(Zoom, scSelectedMinZoom, scSelectedMaxZoom);
            }
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
            if (sat) {
                pPickedSatellite = sat;
                SelectSatellite(pPickedSatellite);
            }

            mouse_pressed = false;
        }
    }


    ++FrameCount;
}


int main(int argc, char* argv[])
{
    Simulation sim {};

#ifdef DISABLE_BINARY_CACHING
    sim.TmpDataset.LoadFromTLE(ASSET_BASE_DIR "/Datasets/2026-04-09.tle");

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
