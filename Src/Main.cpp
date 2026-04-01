/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include "Dataset.hpp"

#include <cassert>
#include <cstdlib>
#include <vector>

#include "ThirdParty/raylib.h"
#include "ThirdParty/raymath.h"
#include "ThirdParty/rlgl.h"

#define RLIGHTS_IMPLEMENTATION
#include "Config.hpp"
#include "Types.hpp"
#include "Vec3.hpp"

#include "ThirdParty/rlights.h"

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

    const std::vector<Vector3>& GetPositionsForTimeFrame();

    ~Simulation();


public:
    Dataset TmpDataset;

    uint32 TimeFrameIndex = 0;

    Model Earth;

    Shader SatLit;
    Shader EarthLit;
    Material SatMaterial;
    Material EarthMaterial;

    Matrix* TransformBuffer;

    float Zoom = cDefaultZoom;

    double AngleX = 0.01;
    double AngleY = 0.01;

    Camera Camera {};
    Mesh SatModel;
    Model Skybox;

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
    InitWindow(1024, 720, "Space");


    auto& positions = GetPositionsForTimeFrame();

    TransformBuffer = (Matrix*)RL_CALLOC(positions.size(), sizeof(Matrix));

    for (int i = 0; i < positions.size(); i++) {
        const Vector3& pos = positions[i];
        TransformBuffer[i] = MatrixTranslate(pos.x, pos.y, pos.z);
    }

    Earth = LoadModel("../Earth.glb");
    Skybox = LoadModel("../Skybox.glb");
    SatModel = GenMeshSphere(0.10f, 6.0f, 6.0f);


    { // Load lighting shader
        SatLit = LoadShader("../Shaders/LightingInstanced.vs", "../Shaders/Lighting.fs");
        // Get shader locations
        SatLit.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(SatLit, "mvp");
        SatLit.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(SatLit, "viewPos");
        SatLit.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(SatLit, "instanceTransform");

        // Set shader value: ambient light level
        int ambientLoc = GetShaderLocation(SatLit, "ambient");
        SetShaderValue(SatLit, ambientLoc, (float[4]) { 0.5f, 0.5f, 0.5f, 1.0f }, SHADER_UNIFORM_VEC4);

        // Create one light
        CreateLight(LIGHT_DIRECTIONAL, (Vector3) { 1000.0f, 100.0f, 0.0f }, Vector3Zero(), Color { 230, 200, 150 },
                    SatLit);
    }


    { // Load lighting shader
        EarthLit = LoadShader("../Shaders/LightingDefault.vs", "../Shaders/Lighting.fs");
        // Get shader locations
        EarthLit.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(EarthLit, "mvp");
        EarthLit.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(EarthLit, "viewPos");
        EarthLit.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(EarthLit, "matModel");

        // Set shader value: ambient light level
        int ambientLoc = GetShaderLocation(EarthLit, "ambient");
        SetShaderValue(EarthLit, ambientLoc, (float[4]) { 0.5f, 0.5f, 0.5f, 1.0f }, SHADER_UNIFORM_VEC4);

        // Create one light
        CreateLight(LIGHT_DIRECTIONAL, (Vector3) { 1000.0f, 100.0f, 0.0f }, Vector3Zero(), Color { 230, 200, 150 },
                    EarthLit);
    }

    // NOTE: We are assigning the intancing shader to material.shader
    // to be used on mesh drawing with DrawMeshInstanced()
    SatMaterial = LoadMaterialDefault();
    SatMaterial.shader = SatLit;
    SatMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    Texture2D texture = Earth.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture;

    EarthMaterial = LoadMaterialDefault();
    EarthMaterial.shader = EarthLit;
    EarthMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = texture;

    Camera.position = Vector3 { 100.0f, 0.0f, -100.0f };
    Camera.target = Vector3 { 0.0f, 0.0f, 0.0f };
    Camera.up = Vector3 { 0.0f, 1.0f, 0.0f };
    Camera.fovy = 65.0f;
    Camera.projection = CAMERA_PERSPECTIVE;
    rlSetClipPlanes(0.05f, 2000.0f);

    // Earth.materials[0].shader = EarthLit;
    // Earth.materials[1] = LoadMaterialDefault();
    // Earth.materials[1].shader = EarthLit;
    // Earth.materials[1].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;


    SetTargetFPS(60);
}


const std::vector<Vector3>& Simulation::GetPositionsForTimeFrame()
{
    assert(TimeFrameIndex < TmpDataset.Size());
    return TmpDataset.GetPositionsForIndex(TimeFrameIndex);
}

Simulation::~Simulation()
{
    RL_FREE(TransformBuffer);

    UnloadModel(Earth);
    UnloadModel(Skybox);

    CloseWindow();
}

void Simulation::StepNext()
{
    TimeFrameIndex = ((TimeFrameIndex + 1) % scNumTimeCaptures);

    auto& positions = GetPositionsForTimeFrame();

    for (int i = 0; i < positions.size(); i++) {
        const Vector3& pos = positions[i];
        TransformBuffer[i] = MatrixTranslate(pos.x, pos.y, pos.z);
    }
}

void Simulation::CheckControls()
{
    if (IsKeyPressed(KEY_SPACE)) {
        StepNext();
    }

    if (IsKeyPressed(KEY_P)) {
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

    AngleY = Clamp(AngleY, -M_PI + cPoleLimitOffset, -cPoleLimitOffset);

    Camera.position.x = Zoom * cosf(AngleX) * sinf(AngleY);
    Camera.position.y = Zoom * cosf(AngleY);
    Camera.position.z = Zoom * sinf(AngleY) * sinf(AngleX);

    {
        float camera_pos[3] = { Camera.position.x, Camera.position.y, Camera.position.z };
        SetShaderValue(SatLit, SatLit.locs[SHADER_LOC_VECTOR_VIEW], camera_pos, SHADER_UNIFORM_VEC3);
        SetShaderValue(EarthLit, EarthLit.locs[SHADER_LOC_VECTOR_VIEW], camera_pos, SHADER_UNIFORM_VEC3);
    }


    float zoom_movement = GetMouseWheelMove();
    if (abs(zoom_movement) > 0.005f) {
        Zoom += zoom_movement * 0.5;
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse_delta = GetMouseDelta();

        if (Vector2LengthSqr(mouse_delta) > 0.05f) {
            AngleX += mouse_delta.x * scMouseSensitivity;
            AngleY += mouse_delta.y * scMouseSensitivity;
        }
    }

    BeginDrawing();

    ClearBackground(BLACK);

    BeginMode3D(Camera);


    DrawModel(Skybox, Vector3 { 0.0f, 0.0f, 0.0f }, 1.0 + (Zoom / cDefaultZoom), WHITE);
    // DrawModel(Earth, Vector3 { 0.0f, 0.0f, 0.0f }, 0.065f, WHITE);
    //
    Matrix newmat = MatrixScale(0.065, 0.065, 0.065);
    DrawMeshInstanced(Earth.meshes[0], EarthMaterial, &newmat, 1);
    DrawMeshInstanced(SatModel, SatMaterial, TransformBuffer, GetPositionsForTimeFrame().size());

    EndMode3D();

    DrawText(TextFormat("aframe:   %02i/%02i", TimeFrameIndex + 1, TmpDataset.Size()), 10, 10, 20, WHITE);
    DrawText("dataset: default", 10, 30, 14, WHITE);
    DrawText(TextFormat("sats:    %04i", TmpDataset.GetPositionsForIndex(0).size()), 10, 45, 14, WHITE);

    EndDrawing();

    ++FrameCount;
}


int main(int argc, char* argv[])
{
    if (argc > 1 && !strcmp(argv[1], "rebuild")) {
        // GeneratePositions();
        Dataset::SaveFromTLE("../NORAD_TLE.txt", "../Positions.bin");
    }

    Simulation sim {};

    while (!WindowShouldClose()) {
        sim.CheckControls();
        sim.Render();
    }


    return 0;
}
