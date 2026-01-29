/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "SGP4.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"


class Vec3r
{
public:
    static const Vec3r sZero;

public:
    Vec3r(double x, double y, double z) : X(x), Y(y), Z(z) {}

    void Print() { printf("{ %f, %f, %f }\n", X, Y, Z); }

    Vec3r operator*(float multiplier) const { return Vec3r(X * multiplier, Y * multiplier, Z * multiplier); }

public:
    union
    {
        struct
        {
            double X;
            double Y;
            double Z;
            double W;
        };

        double Values[4];
    };
};

const Vec3r Vec3r::sZero = Vec3r(0.0, 0.0, 0.0);

struct SpaceForce
{
    Vec3r Position = Vec3r::sZero;
    Vec3r Velocity = Vec3r::sZero;
};


class Space
{
public:
    void CreateFromEntry(const char* line1, const char* line2);

    SpaceForce GetForce(double time);


public:
    struct
    {
        double Start;
        double Stop;
        double Delta;
    } Mfe;

    elsetrec SatRec;
};

void Space::CreateFromEntry(const char* line1, const char* line2)
{
    memset(&SatRec, 0, sizeof(SatRec));

    char* line1_buf = strdup(line1);
    char* line2_buf = strdup(line2);

    SGP4Funcs::twoline2rv(line1_buf, line2_buf, 'c', 'e', 'i', wgs84, Mfe.Start, Mfe.Stop, Mfe.Delta, SatRec);

    free(line1_buf);
    free(line2_buf);
}

SpaceForce Space::GetForce(double time)
{
    SpaceForce force {};
    SGP4Funcs::sgp4(SatRec, time, force.Position.Values, force.Velocity.Values);

    return force;
}


void GeneratePositions()
{
    std::string filename = "../NORAD_TLE.txt";

    std::ifstream fb(filename, std::ios::in);

    std::string name_line;
    std::string line1;
    std::string line2;

    std::ofstream output("../Positions.bin", std::ios::out | std::ios::binary);


    while (true) {
        if (!std::getline(fb, name_line)) {
            return;
        }
        std::getline(fb, line1);
        std::getline(fb, line2);

        printf("Saving %s\n", name_line.c_str());

        Space space {};

        space.CreateFromEntry(line1.c_str(), line2.c_str());
        SpaceForce force = space.GetForce(0.0f);

        output.write(reinterpret_cast<char*>(force.Position.Values), sizeof(force.Position.Values));
    }

    output.close();
}

std::vector<Vector3> LoadPositions()
{
    std::ifstream fb("../Positions.bin", std::ios::in | std::ios::binary);

    std::vector<Vector3> positions;

    double buffer[4];

    while (fb) {
        fb.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        positions.push_back(Vector3 {
            .x = static_cast<float>(buffer[0] * 0.01),
            .y = static_cast<float>(buffer[1] * 0.01),
            .z = static_cast<float>(buffer[2] * 0.01),
        });
    }

    return positions;
}


int main()
{
    std::vector<Vector3> positions = LoadPositions();

    char line1[] = "1 41340U 16012D   26026.21998329  .00083066  00000+0  11410-2 0  9992";
    char line2[] = "2 41340  30.9947   3.4457 0004047 308.3989  51.6299 15.55929722548471 ";

    Space space {};
    space.CreateFromEntry(strdup(line1), strdup(line2));

    SpaceForce force = space.GetForce(0.0f);

    force.Position.Print();
    force.Velocity.Print();

    InitWindow(1024, 720, "Space");


    Mesh sphere = GenMeshSphere(0.1f, 16.0f, 16.0f);


    Camera camera {};
    camera.position = Vector3 { 100.0f, 0.0f, -100.0f };
    camera.target = Vector3 { 0.0f, 0.0f, 0.0f };
    camera.up = Vector3 { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    rlSetClipPlanes(0.05f, 2000.0f);


    Model model = LoadModel("../Earth.glb");

    Matrix* transforms = (Matrix*)RL_CALLOC(positions.size(), sizeof(Matrix));

    for (int i = 0; i < positions.size(); i++) {
        Vector3& pos = positions[i];
        // printf("Position: {%f, %f, %f}\n", pos.x, pos.y, pos.z);
        transforms[i] = MatrixTranslate(pos.x, pos.y, pos.z);
    }

    // Load lighting shader
    Shader shader = LoadShader("../Shaders/LightingInstanced.vs", "../Shaders/Lighting.fs");
    // Get shader locations
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader, "instanceTransform");

    // Set shader value: ambient light level
    int ambientLoc = GetShaderLocation(shader, "ambient");
    SetShaderValue(shader, ambientLoc, (float[4]) { 0.2f, 0.2f, 0.2f, 1.0f }, SHADER_UNIFORM_VEC4);

    // Create one light
    CreateLight(LIGHT_DIRECTIONAL, (Vector3) { 1000.0f, 100.0f, 0.0f }, Vector3Zero(), Color { 205, 200, 150 }, shader);

    // NOTE: We are assigning the intancing shader to material.shader
    // to be used on mesh drawing with DrawMeshInstanced()
    Material matInstances = LoadMaterialDefault();
    matInstances.shader = shader;
    matInstances.maps[MATERIAL_MAP_DIFFUSE].color = RAYWHITE;


    double counter = 0.0f;

    SetTargetFPS(60);

    float current_zoom = 200.0f;


    while (!WindowShouldClose()) {
        camera.position.x = cosf(counter) * current_zoom;
        camera.position.z = sinf(counter) * current_zoom;

        // UpdateCamera(&camera, CAMERA_ORBITAL);

        // Update the light shader with the camera view position
        float cameraPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);
        //----------------------------------------------------------------------------------

        float zoom_movement = GetMouseWheelMove();
        if (abs(zoom_movement) > 0.005f) {
            current_zoom += zoom_movement * 0.5;
        }

        BeginDrawing();

        ClearBackground(BLACK);

        BeginMode3D(camera);


        DrawModel(model, Vector3 { 0.0f, 0.0f, 0.0f }, 0.12f, WHITE);
        DrawMeshInstanced(sphere, matInstances, transforms, positions.size());


        // for (int i = 0; i < positions.size(); i++) {
        //     DrawSphere(positions[i], 4.0f, RED);
        // }
        //

        EndMode3D();


        // rlPopMatrix();

        EndDrawing();

        counter += 0.0005f;
    }

    RL_FREE(transforms);

    UnloadModel(model);

    CloseWindow();


    return 0;
}
