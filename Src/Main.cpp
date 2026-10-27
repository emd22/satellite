/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include "Dataset.hpp"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

/// The number of frames it takes for a satellite to reach its destination.
static uint32 sNumLerpFrames = 240;

// Stars and solar system textures taken from https://www.solarsystemscope.com/textures/, based off of NASA images.
// Earth 3D model has been taken from the NASA website


static constexpr float32 scZoomSensitivity = 0.06f;
static constexpr float32 scZoomSmoothing = 12.0f;
static constexpr float32 scMouseSensitivity = 0.005f;


static constexpr float32 scEarthModelRadius = 500.0f;
static constexpr float32 scEarthRadius = float32(scEarthRadiusKm * scKmToUnits);

/// Scale that takes the Earth model to scEarthRadius
static constexpr float32 scEarthScale = scEarthRadius / scEarthModelRadius;

static constexpr float32 scEarthSurfaceClearance = 0.1f;

static constexpr float32 cPoleLimitOffset = 0.005;
static constexpr float32 cDefaultZoom = 10.0f;
static constexpr float32 cSinglePointZoom = 2.0f;

static constexpr float32 scUIPaddingHorizontal = 20;
static constexpr float32 scUIPaddingVertical = 10;

static constexpr float32 scUIGridHorizontalSplits = 24;
static constexpr float32 scUIGridVerticalSplits = 32;

static constexpr float32 scUIDesignSize = 900.0f;
static constexpr float32 scMinUIScale = 0.65f;
static constexpr uint32 scCollapsedUIWidth = 600;

static constexpr float32 scDefaultMinZoom = 3.85f;
static constexpr float32 scDefaultMaxZoom = 45.0f;

static constexpr float32 scSelectedMinZoom = 1.0f;
static constexpr float32 scSelectedMaxZoom = 4.5f;

const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);


/**
 * @brief Loads a shader from disk given the vertex and pixel shader paths.
 * NOTE: The version header is modified for web builds
 */
static rl::Shader LoadShaderPortable(const char* vs_path, const char* fs_path)
{
#if defined(__EMSCRIPTEN__)
    auto LoadShaderSource = [](const char* path) -> char*
    {
        if (!path) {
            return nullptr;
        }

        char* source = rl::LoadFileText(path);
        if (!source) {
            return nullptr;
        }

        static constexpr char scDesktopHeader[] = "#version 330";
        static constexpr char scWebHeader[] = "#version 300 es\nprecision highp float;\nprecision highp int;";

        char* version_start = strstr(source, scDesktopHeader);
        if (!version_start) {
            return source;
        }

        const char* rest = version_start + strlen(scDesktopHeader);

        size_t prefix_len = version_start - source;
        size_t total = prefix_len + strlen(scWebHeader) + strlen(rest) + 1;

        char* patched = static_cast<char*>(malloc(total));
        memcpy(patched, source, prefix_len);
        strcpy(patched + prefix_len, scWebHeader);
        strcat(patched, rest);

        rl::UnloadFileText(source);

        return patched;
    };

    char* vs_source = LoadShaderSource(vs_path);
    char* fs_source = LoadShaderSource(fs_path);

    rl::Shader shader = rl::LoadShaderFromMemory(vs_source, fs_source);

    free(vs_source);
    free(fs_source);

    return shader;
#else
    return rl::LoadShader(vs_path, fs_path);
#endif
}


class Simulation
{
public:
    Simulation();

    void InitGraphics();

    void CheckControls();
    void Render();

    void Step(int32 by);
    void UpdateTransformations();

    void DrawText(const String& text, int32 cell_x, int32 cell_y, float32 size);
    void UpdateSatellites();
    rl::Vector3 GetSunPosition() { return SunPos.ToRL(); }
    Satellite* CheckForSatPicking();
    Satellite* FindSatelliteFromNorad(uint32 norad);
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

    void GetZoomLimits(float32& min_zoom, float32& max_zoom) const
    {
        min_zoom = scSelectedMinZoom;
        max_zoom = scSelectedMaxZoom;

        if (IsDefaultFilter()) {
            min_zoom = scDefaultMinZoom;
            max_zoom = scDefaultMaxZoom;
        }
    }

    float32 GetZoomPercent() const
    {
        float32 min_zoom, max_zoom;
        GetZoomLimits(min_zoom, max_zoom);

        return 100 - ((Zoom - min_zoom) / (max_zoom - min_zoom)) * 100.0f;
    }

    float32 GetSurfaceZoomLimit() const;
    void UpdateZoom(float32 delta_time);
    void UpdateUIScale();

    float32 GetSpeedPercent() const { return (1.0f - (float32(sNumLerpFrames - 10) / (300.0f - 10.0f))) * 100.0f; }

    float32 GetLerpFraction() const { return std::min(float32(LerpFrame) / float32(sNumLerpFrames), 1.0f); }

    ~Simulation();


public:
    Dataset TmpDataset;

    uint32 TimeFrameIndex = 0;

    /// Frames of animation since the current time step began
    uint32 LerpFrame = 0;

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
    /// The zoom that `Zoom` is smoothly moving towards.
    float32 TargetZoom = Zoom;
    float32 SavedZoom = Zoom;

    double SavedAngleX = 0.01;
    double SavedAngleY = 0.01;

    double AngleX = 0.01;
    double AngleY = 0.01;

    float32 UICellSizeX = 1.0f;
    float32 UICellSizeY = 1.0f;

    float32 UIScale = 1.0f;
    ImGuiStyle BaseUIStyle;

    float32 PrevPinchDistance = 0.0f;
    bool bInTouchGesture = false;

    uint32 NoradIdQuery = 0;


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

Satellite* Simulation::FindSatelliteFromNorad(uint32 norad)
{
    for (Satellite& sat : TmpDataset.Satellites) {
        if (sat.NoradId == norad) {
            return &sat;
        }
    }

    return nullptr;
}

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


void Simulation::UpdateSatellites() { pFilterInUse->UpdateSatellites({}, GetLerpFraction()); }


Simulation::Simulation() {}

Satellite* Simulation::CheckForSatPicking()
{
    if (bHideSatellites) {
        return nullptr;
    }

    rl::Ray ray = rl::GetScreenToWorldRay(rl::GetMousePosition(), Camera);


    // Satellites behind the Earth are hidden by it, so they can't be picked
    float32 max_distance = FLT_MAX;

    rl::RayCollision earth_hit = rl::GetRayCollisionSphere(ray, rl::Vector3Zero(), scEarthRadius);
    if (earth_hit.hit) {
        max_distance = earth_hit.distance;
    }

    return pFilterInUse->RaycastToSatellite(ray, max_distance);
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
    // Keep the same progress through the current time step, so satellites don't jump when the speed changes
    const float32 fraction = GetLerpFraction();

    sNumLerpFrames = lerp_frames;
    LerpFrame = uint32(fraction * float32(sNumLerpFrames));
}

#ifdef __EMSCRIPTEN__
/**
 * @brief Gets the size of the element containing the canvas, so the page decides how big the canvas is.
 * @returns False if the container has no size yet.
 */
static bool GetCanvasContainerSize(int32& width, int32& height)
{
    width = EM_ASM_INT({
        const parent = Module['canvas'].parentElement;
        return parent ? parent.clientWidth : 0;
    });
    height = EM_ASM_INT({
        const parent = Module['canvas'].parentElement;
        return parent ? parent.clientHeight : 0;
    });

    return width > 0 && height > 0;
}

/**
 * @brief Tracks the number of fingers on the screen, read by GetActiveTouchCount().
 */
static void InstallTouchCounter()
{
    EM_ASM({
        const canvas = Module['canvas'];
        Module['activeTouches'] = 0;
        const update = (event) => { Module['activeTouches'] = event.touches.length; };
        for (const type of['touchstart', 'touchmove', 'touchend', 'touchcancel']) {
            canvas.addEventListener(type, update);
        }
    });
}
#endif

/**
 * @brief Gets the number of fingers on the screen. On the web Raylib's count gets stuck at 1 when several fingers are
 * lifted at once, so we can just use the browser's touch count instead.
 */
static int32 GetActiveTouchCount()
{
#ifdef __EMSCRIPTEN__
    return EM_ASM_INT({ return Module['activeTouches'] | 0; });
#else
    return rl::GetTouchPointCount();
#endif
}

void Simulation::InitGraphics()
{
#ifdef __EMSCRIPTEN__

    // Fill the canvases container. The resizable flag in Raylib is not used on the web as it always sizes the canvas to
    // the whole browser window, so Render() follows the container size instead.
    int32 container_width, container_height;
    if (GetCanvasContainerSize(container_width, container_height)) {
        WindowWidth = container_width;
        WindowHeight = container_height;
    }
    else {
        WindowWidth = 900;
        WindowHeight = 900;
    }

    InstallTouchCounter();
#else
    WindowWidth = 900;
    WindowHeight = 900;

    rl::SetConfigFlags(rl::FLAG_WINDOW_RESIZABLE);
#endif

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
        sat.MoveToTimeStep(0);

        DefaultFilter.AddSatellite(sat, GetSatColor(sat.Series.GetHash()));
    }

    DefaultFilter.Finalize(SatModel, SatMaterial.shader);

    Earth = rl::LoadModel(ASSET_BASE_DIR "/Earth.glb");
    Skybox = rl::LoadModel(ASSET_BASE_DIR "/Skybox.glb");
    SunModel = rl::LoadModel(ASSET_BASE_DIR "/Sun.glb");

    SatModel = rl::GenMeshSphere(0.005f, 6.0f, 6.0f);

    AtmosphereModel = rl::GenMeshSphere(scEarthRadius * 1.5f, 24.0f, 24.0f);

    float ambient_color[4] = { 0.2f, 0.2f, 0.2f, 1.0f };


    { // Load Atmosphere shader
        AtmosphereShader = LoadShaderPortable(ASSET_BASE_DIR "/Shaders/LightingDefault.vs",
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
        SatLit = LoadShaderPortable(ASSET_BASE_DIR "/Shaders/LightingInstanced.vs", nullptr);
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
        EarthLit = LoadShaderPortable(ASSET_BASE_DIR "/Shaders/LightingDefault.vs",
                                      ASSET_BASE_DIR "/Shaders/Lighting.fs");
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

#ifndef __EMSCRIPTEN__
    // On the web the browser paces frames with requestAnimationFrame
    rl::SetTargetFPS(120);
#endif
    rl::SetExitKey(rl::KEY_NULL);

    rl::rlImGuiSetup(true);


    ImGuiIO& io = ImGui::GetIO();

    io.ConfigWindowsMoveFromTitleBarOnly = true;

    FontNormal = io.Fonts->AddFontFromFileTTF(ASSET_BASE_DIR "/font.ttf", 16.0f);
    FontLarge = io.Fonts->AddFontFromFileTTF(ASSET_BASE_DIR "/font.ttf", 32.0f);

    BaseUIStyle = ImGui::GetStyle();
    UpdateUIScale();
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
    TargetZoom = Zoom;
}


float32 Simulation::GetSurfaceZoomLimit() const
{
    const float32 min_radius = scEarthRadius + scEarthSurfaceClearance;

    const Vec3r direction(cosf(AngleX) * sinf(AngleY), cosf(AngleY), sinf(AngleY) * sinf(AngleX));

    const float32 b = CameraCenter.Dot(direction);
    const float32 c = CameraCenter.Dot(CameraCenter) - (min_radius * min_radius);

    // If the center is inside the clearance sphere (e.g. orbiting the Earth itself) there is nothing to clamp to
    if (c <= 0.0f) {
        return FLT_MAX;
    }

    const float32 discriminant = (b * b) - c;

    // The ray is pointing away from the Earth, or misses it completely
    if (b >= 0.0f || discriminant <= 0.0f) {
        return FLT_MAX;
    }

    return -b - sqrtf(discriminant);
}


void Simulation::UpdateZoom(float32 delta_time)
{
    float32 min_zoom, max_zoom;
    GetZoomLimits(min_zoom, max_zoom);

    TargetZoom = rl::Clamp(TargetZoom, min_zoom, max_zoom);

    Zoom = TargetZoom + (Zoom - TargetZoom) * expf(-scZoomSmoothing * delta_time);

    const float32 surface_limit = GetSurfaceZoomLimit();
    TargetZoom = std::min(TargetZoom, surface_limit);
    Zoom = std::min(Zoom, surface_limit);
}


void Simulation::UpdateUIScale()
{
    const float32 scale = std::clamp(float32(std::min(WindowWidth, WindowHeight)) / scUIDesignSize, scMinUIScale, 1.0f);
    if (scale == UIScale) {
        return;
    }

    UIScale = scale;

    ImGuiStyle& style = ImGui::GetStyle();
    style = BaseUIStyle;
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}


void Simulation::DeselectSatellites()
{
    SetCameraTarget(Vec3r::sZero);
    Zoom = SavedZoom;
    TargetZoom = Zoom;

    AngleX = SavedAngleX;
    AngleY = SavedAngleY;

    pFilterInUse = &DefaultFilter;
    pPickedSatellite = nullptr;

    if (bTemporarilyShowDebris) {
        // Revert back to the original state
        SelectedFilter.bShowDebris = false;
    }

    DefaultFilter.UpdateSatellites({}, GetLerpFraction());
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

void Simulation::UpdateTransformations()
{
    pFilterInUse->UpdateSatellites(std::make_optional(TimeFrameIndex), GetLerpFraction());

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
    else if (int32(TimeFrameIndex) + by >= int32(scNumTimeCaptures)) {
        TimeFrameIndex = 0;
    }
    else {
        TimeFrameIndex += by;
    }

    // Every step lands exactly on a time step
    LerpFrame = 0;

    UpdateTransformations();
}

void Simulation::CheckControls()
{
    if (rl::IsKeyPressed(rl::KEY_PERIOD)) { // Greater than symbol
        Step(1);
    }
    else if (rl::IsKeyPressed(rl::KEY_COMMA)) {
        Step(-1);
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
#ifdef __EMSCRIPTEN__
    // Keep the canvas the same size as its container. SetWindowSize resizes the canvas and updates the screen size.
    int32 container_width, container_height;
    if (!rl::IsWindowFullscreen() && GetCanvasContainerSize(container_width, container_height) &&
        (container_width != rl::GetScreenWidth() || container_height != rl::GetScreenHeight())) {
        rl::SetWindowSize(container_width, container_height);
    }
#endif

    if (rl::GetScreenWidth() != WindowWidth || rl::GetScreenHeight() != WindowHeight) {
        WindowWidth = rl::GetScreenWidth();
        WindowHeight = rl::GetScreenHeight();
        UICellSizeX = (float32(WindowWidth) - scUIPaddingHorizontal) / scUIGridHorizontalSplits;
        UICellSizeY = (float32(WindowHeight) - scUIPaddingVertical) / scUIGridVerticalSplits;

        UpdateUIScale();
    }

    static bool mouse_pressed = false;

    if (bAnimateTimeFrames) {
        ++LerpFrame;

        if (LerpFrame >= sNumLerpFrames) {
            Step(1);
        }
        else {
            UpdateSatellites();
        }
    }

    // Follow the selection whether it moved by animation or by stepping
    if (pPickedSatellite) {
        SetCameraTarget(pPickedSatellite->Position);
    }

    if (AngleX > M_PI) {
        AngleX = -M_PI + 0.0001;
    }
    if (AngleX < -M_PI) {
        AngleX = M_PI - 0.0001;
    }

    AngleY = rl::Clamp(AngleY, -M_PI + cPoleLimitOffset, -cPoleLimitOffset);

    UpdateZoom(rl::GetFrameTime());

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


    rl::Matrix newmat = rl::MatrixScale(scEarthScale, scEarthScale, scEarthScale);
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

    // Default layout for when there is no imgui.ini (e.g. on the web)
    const bool collapse_ui = WindowWidth < scCollapsedUIWidth;

    ImGui::SetNextWindowPos(ImVec2(0, 1), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(collapse_ui, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tools", nullptr, scWindowFlags)) {
        ImGui::PushFont(FontNormal);
        ImGui::PushFont(FontLarge);
        ImGui::TextUnformatted(String::Fmt("Frame {}/{}", TimeFrameIndex + 1, TmpDataset.NumTimesteps).CStr());
        ImGui::PopFont();

        if (ImGui::Button("<")) {
            Step(-1);
        }

        ImGui::SameLine();

        if (ImGui::Button(bAnimateTimeFrames ? "Pause" : "Play")) {
            bAnimateTimeFrames = !bAnimateTimeFrames;
        }
        ImGui::SameLine();

        if (ImGui::Button(">")) {
            Step(1);
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

        ImGui::InputScalar("NORAD ID", ImGuiDataType_U32, &NoradIdQuery);
        if (ImGui::Button("Select NORAD ID")) {
            Satellite* found = FindSatelliteFromNorad(NoradIdQuery);
            if (found) {
                pPickedSatellite = found;
                SelectSatellite(found);
            }
        }

        ImGui::PopFont();
    }
    ImGui::End();

    open = pPickedSatellite != nullptr;

    ImGui::SetNextWindowPos(ImVec2(float32(WindowWidth) - 304.0f * UIScale, 1), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 350));
    ImGui::SetNextWindowCollapsed(collapse_ui, ImGuiCond_FirstUseEver);
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

    // NOTE: Input is handled before EndDrawing(), as that is where Raylib advances its input state. On the web,
    // events arrive between frames, so any presses/scrolls would already be cleared after EndDrawing().
    ImGuiIO& imgui_io = ImGui::GetIO();

    if (!ImGui::IsAnyItemHovered()) {
        // Only vertical scrolling zooms; GetMouseWheelMove() would also pick up horizontal trackpad swipes
        float zoom_movement = rl::GetMouseWheelMoveV().y;
        if (abs(zoom_movement) > 0.005f) {
            // Scrolling up (positive) zooms in, which moves the camera closer
            TargetZoom *= expf(-zoom_movement * scZoomSensitivity);
        }


        const int32 touch_count = GetActiveTouchCount();
        if (touch_count >= 2) {
            const float32 pinch_distance = rl::Vector2Distance(rl::GetTouchPosition(0), rl::GetTouchPosition(1));
            if (PrevPinchDistance > 0.0f && pinch_distance > 0.0f) {
                TargetZoom *= PrevPinchDistance / pinch_distance;
            }

            PrevPinchDistance = pinch_distance;
            bInTouchGesture = true;
            mouse_pressed = false;
        }
        else {
            PrevPinchDistance = 0.0f;

            if (touch_count == 0) {
                bInTouchGesture = false;
            }
        }

        // The press frame is skipped as a new touch moves the mouse position from wherever the last touch ended
        if (!bInTouchGesture && rl::IsMouseButtonDown(rl::MOUSE_LEFT_BUTTON) &&
            !rl::IsMouseButtonPressed(rl::MOUSE_LEFT_BUTTON)) {
            rl::Vector2 mouse_delta = rl::GetMouseDelta();

            float32 move_magnitude = abs(rl::Vector2Length(mouse_delta));

            if (move_magnitude > 0.005f) {
                AngleX += mouse_delta.x * scMouseSensitivity;
                AngleY += mouse_delta.y * scMouseSensitivity;
                mouse_pressed = false;
            }
        }

        if (!bInTouchGesture && rl::IsMouseButtonPressed(rl::MOUSE_LEFT_BUTTON)) {
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


    rl::EndDrawing();


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

#ifdef __EMSCRIPTEN__
    // The browser owns the main loop, so hand each frame off to it. This call does not return.
    emscripten_set_main_loop_arg(
        [](void* arg)
        {
            Simulation* sim = static_cast<Simulation*>(arg);
            sim->CheckControls();
            sim->Render();
        },
        &sim, 0, true);
#else
    while (!rl::WindowShouldClose()) {
        sim.CheckControls();
        sim.Render();
    }
#endif


    return 0;
}
