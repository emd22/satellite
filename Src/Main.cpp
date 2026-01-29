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
#include "rlgl.h"


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
        positions.emplace_back(Vector3 {
            .x = static_cast<float>(buffer[0] * 0.1),
            .y = static_cast<float>(buffer[1] * 0.1),
            .z = static_cast<float>(buffer[2] * 0.1),
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

    SetTargetFPS(60);

    Camera camera {};
    camera.position = Vector3 { 50.0f, 0.0f, -50.0f };
    camera.target = Vector3 { 0.0f, 0.0f, 0.0f };
    camera.up = Vector3 { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Model model = LoadModel("../earth.glb");

    // Vec3r sat_pos_scaled = force.Position * 0.1;
    // sat_pos_scaled.Print();

    // Vector3 sat_position = Vector3 { .x = float(sat_pos_scaled.X),
    //                                  .y = float(sat_pos_scaled.Y),
    //                                  .z = float(sat_pos_scaled.Z) };

    double counter = 0.0f;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        DrawModel(model, Vector3 { 0.0f, 0.0f, 0.0f }, 0.1f, WHITE);

        camera.position.x = cosf(counter * 0.1) * 200.0f;
        camera.position.z = sinf(counter * 0.1) * 200.0f;

        for (int i = 0; i < positions.size(); i++) {
            DrawSphere(positions[i], 4.0f, RED);
        }

        EndMode3D();

        // rlPushMatrix();


        // rlPopMatrix();

        EndDrawing();

        counter += 0.1f;
    }

    UnloadModel(model);

    CloseWindow();


    return 0;

    return 0;
}
