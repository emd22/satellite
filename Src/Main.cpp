/*
 * Ethan MacDonald
 * 26/01/28
 * Visualization Project
 */

#include <cstdlib>

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
    void CreateFromEntry(char* line1, char* line2);

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

void Space::CreateFromEntry(char* line1, char* line2)
{
    memset(&SatRec, 0, sizeof(SatRec));
    SGP4Funcs::twoline2rv(line1, line2, 'c', 'e', 'i', wgs84, Mfe.Start, Mfe.Stop, Mfe.Delta, SatRec);
}

SpaceForce Space::GetForce(double time)
{
    SpaceForce force {};
    SGP4Funcs::sgp4(SatRec, time, force.Position.Values, force.Velocity.Values);

    return force;
}


int main()
{
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

    Vec3r sat_pos_scaled = force.Position * 0.1;
    sat_pos_scaled.Print();

    Vector3 sat_position = Vector3 { .x = float(sat_pos_scaled.X),
                                     .y = float(sat_pos_scaled.Y),
                                     .z = float(sat_pos_scaled.Z) };

    double counter = 0.0f;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        BeginMode3D(camera);

        DrawModel(model, Vector3 { 0.0f, 0.0f, 0.0f }, 0.05f, WHITE);

        camera.position.x = cosf(counter * 0.1) * 70.0f;
        camera.position.z = sinf(counter * 0.1) * 70.0f;

        DrawSphere(sat_position, 5.0f, RED);

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
