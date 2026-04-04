#include "Dataset.hpp"

#include "Config.hpp"
#include "String.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

#include "ThirdParty/SGP4.h"


class TleContext
{
public:
    void CreateFromEntry(const char* line1, const char* line2);

    Satellite::TimeStep GetTimeStepForTime(double time);

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

    SGP4Funcs::twoline2rv(line1_buf, line2_buf, 'c', 'e', 'i', wgs84, Mfe.Start, Mfe.Stop, Mfe.Delta, SatRec);

    free(line1_buf);
    free(line2_buf);
}


Satellite::TimeStep TleContext::GetTimeStepForTime(double time)
{
    Satellite::TimeStep force {};

    float64 pos_buffer[4];
    float64 vel_buffer[4];
    SGP4Funcs::sgp4(SatRec, time, pos_buffer, vel_buffer);

    // Convert the double precision results into our real number type in the vector
    force.Position = Vec3r(pos_buffer);

    return force;
}


struct BinHeader
{
    uint32 NumSatellites = 0;
    uint32 NumTimesteps = 0;
};


/**
 * @brief Finds the name of the dataset given a full path
 */
static String FindNameForPath(const String& path)
{
    const char* base = path.CStr();
    const char* end = base + path.GetLength();

    // Find end (eat the file extension)
    while ((*end) != '.' && end > base) {
        --end;
    }

    const char* idx_ptr = end;

    // Find start, read from back to front until slash is found
    while ((*idx_ptr) != '/' && end > base) {
        --idx_ptr;
    }

    // Remove leading slash
    ++idx_ptr;

    uint32 size = static_cast<uint32>(end - idx_ptr);
    String name(size);

    name = String(Slice<const char>(idx_ptr, size));

    return name;
}


void Dataset::LoadFromTLE(const String& tle_path)
{
    printf("Loading TLE at path %s\n", tle_path.CStr());

    std::ifstream fb(tle_path.CStr(), std::ios::in);

    Name = FindNameForPath(tle_path);

    printf("Name: %s\n", Name.CStr());

    std::string name_line;
    std::string line1;
    std::string line2;

    constexpr float cScaleMultiplier = 0.00035f;

    NumTimesteps = scNumTimeCaptures;

    // Read in all of the TLE information

    while (true) {
        if (!std::getline(fb, name_line)) {
            return;
        }

        std::getline(fb, line1);
        std::getline(fb, line2);

        printf("Saving %s\n", name_line.c_str());

        TleContext tle_ctx {};

        tle_ctx.CreateFromEntry(line1.c_str(), line2.c_str());

        Satellite sat;

        for (uint32 i = 0; i < scNumTimeCaptures; i++) {
            Satellite::TimeStep time_step = tle_ctx.GetTimeStepForTime(scStartTime + (scTimeJump * i));
            time_step.Position *= cScaleMultiplier;

            sat.AddTimeStep(time_step);
        }

        Satellites.push_back(sat);
    }
}


void Dataset::SaveToBin(const String& path)
{
    String dst_path = String::Fmt("{}/{}.bin", path, Name);

    std::ofstream output(dst_path.CStr(), std::ios::out | std::ios::binary);

    BinHeader header {};
    header.NumSatellites = Satellites.size();
    header.NumTimesteps = scNumTimeCaptures;

    // Write the header
    output.write(reinterpret_cast<char*>(&header), sizeof(header));

    for (uint32 sat_index = 0; sat_index < Satellites.size(); sat_index++) {
        const Satellite& sat = GetSatellite(sat_index);

        // Write each timestep
        for (uint32 ts_index = 0; ts_index < scNumTimeCaptures; ts_index++) {
            const Satellite::TimeStep& ts = sat.GetTimeStep(ts_index);
            output.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
        }
    }


    output.close();
}


void Dataset::LoadFromBin(const String& bin_path)
{
    std::ifstream fb(bin_path.CStr(), std::ios::in | std::ios::binary);

    Name = FindNameForPath(bin_path);

    // Read in the header
    BinHeader header;
    fb.read(reinterpret_cast<char*>(&header), sizeof(BinHeader));

    // Resize the vector to hold all time frames that were defined in the binary file
    Satellites.resize(header.NumSatellites);

    float64 buffer[4];

    NumTimesteps = header.NumTimesteps;

    for (uint32 sat_index = 0; sat_index < header.NumSatellites; sat_index++) {
        Satellite& sat = Satellites[sat_index];

        // Reserve the memory for the timesteps (faster inserts)
        sat.TimeSteps.reserve(scNumTimeCaptures);

        for (uint32 ts_index = 0; ts_index < NumTimesteps; ts_index++) {
            Satellite::TimeStep ts;
            fb.read(reinterpret_cast<char*>(&ts), sizeof(ts));

            sat.AddTimeStep(ts);
        }
    }
}
