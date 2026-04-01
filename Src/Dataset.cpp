#include "Dataset.hpp"

#include "Config.hpp"
#include "String.hpp"

#include <fstream>
#include <iostream>

#include "ThirdParty/SGP4.h"


class TleContext
{
public:
    void CreateFromEntry(const char* line1, const char* line2);

    Force GetForce(double time);

public:
    struct
    {
        double Start;
        double Stop;
        double Delta;
    } Mfe;

    elsetrec SatRec;
};

void TleContext::CreateFromEntry(const char* line1, const char* line2)
{
    memset(&SatRec, 0, sizeof(SatRec));

    char* line1_buf = strdup(line1);
    char* line2_buf = strdup(line2);

    SGP4Funcs::twoline2rv(line1_buf, line2_buf, 'c', 'e', 'i', wgs72, Mfe.Start, Mfe.Stop, Mfe.Delta, SatRec);

    free(line1_buf);
    free(line2_buf);
}


Force TleContext::GetForce(double time)
{
    Force force {};
    SGP4Funcs::sgp4(SatRec, time, force.Position.Values, force.Velocity.Values);

    return force;
}


struct BinHeader
{
    uint32 TimeCaptures = scNumTimeCaptures;
};


void Dataset::SaveFromTLE(const String& tle_path, const String& dst_path)
{
    std::ifstream fb(tle_path.CStr(), std::ios::in);

    std::string name_line;
    std::string line1;
    std::string line2;

    std::ofstream output(dst_path.CStr(), std::ios::out | std::ios::binary);

    BinHeader header {};
    output.write(reinterpret_cast<char*>(&header), sizeof(BinHeader));

    while (true) {
        if (!std::getline(fb, name_line)) {
            return;
        }

        std::getline(fb, line1);
        std::getline(fb, line2);

        printf("Saving %s\n", name_line.c_str());

        TleContext space {};

        double unix_time = time(NULL);

        space.CreateFromEntry(line1.c_str(), line2.c_str());

        for (uint32 i = 0; i < scNumTimeCaptures; i++) {
            Force force = space.GetForce(scStartTime + (scTimeJump * i));
            output.write(reinterpret_cast<char*>(force.Position.Values), sizeof(force.Position.Values));
        }
    }

    output.close();
}


void Dataset::LoadFromBin(const String& bin_path)
{
    std::ifstream fb(bin_path.CStr(), std::ios::in | std::ios::binary);

    BinHeader header;
    fb.read(reinterpret_cast<char*>(&header), sizeof(BinHeader));

    // Resize the vector to hold all time frames that were defined in the binary file
    TimeFrames.resize(header.TimeCaptures);

    double buffer[4];

    constexpr float cScaleMultiplier = 0.005f;

    while (fb) {
        for (uint32 i = 0; i < header.TimeCaptures; i++) {
            TimeFrame& frame = TimeFrames[i];

            fb.read(reinterpret_cast<char*>(buffer), sizeof(buffer));

            frame.Positions.push_back(rl::Vector3 {
                .x = static_cast<float32>(buffer[0] * cScaleMultiplier),
                .y = static_cast<float32>(buffer[1] * cScaleMultiplier),
                .z = static_cast<float32>(buffer[2] * cScaleMultiplier),
            });
        }
    }
}
