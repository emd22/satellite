#include "Dataset.hpp"

#include "Config.hpp"
#include "String.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "ThirdParty/SGP4.h"


class TleContext
{
public:
    void CreateFromEntry(const char* line1, const char* line2);

    /**
     * @brief Propagates to a Julian date. Returns false if SGP4 rejects the state.
     * @param earth_angle Rotation from the inertial frame into the Earth texture frame (radians)
     */
    bool GetTimeStepForTime(float64 jd, float64 earth_angle, Satellite::TimeStep& time_step);

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

    SGP4Funcs::twoline2rv(line1_buf, line2_buf, 'c', 'm', 'i', wgs84, Mfe.Start, Mfe.Stop, Mfe.Delta, SatRec);

    free(line1_buf);
    free(line2_buf);
}


/**
 * @brief Converts an SGP4 position (TEME, km) into scene space.
 */
static Vec3r TemeToScene(const float64 teme[3], float64 earth_angle)
{
    const float64 c = std::cos(earth_angle);
    const float64 s = std::sin(earth_angle);

    const float64 x = teme[0] * c + teme[1] * s;
    const float64 y = -teme[0] * s + teme[1] * c;
    const float64 z = teme[2];

    return Vec3r(float32(x), float32(z), float32(-y)) * scKmToUnits;
}

bool TleContext::GetTimeStepForTime(float64 jd, float64 earth_angle, Satellite::TimeStep& time_step)
{
    // SGP4 takes minutes since the element epoch. The epoch is split into a whole and a fractional day.
    const float64 tsince = ((jd - SatRec.jdsatepoch) - SatRec.jdsatepochF) * 1440.0;

    float64 teme_pos[3] = {};
    float64 teme_vel[3] = {};

    if (!SGP4Funcs::sgp4(SatRec, tsince, teme_pos, teme_vel)) {
        return false;
    }

    time_step.Position = TemeToScene(teme_pos, earth_angle);
    return true;
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

static void SetNamesForSatellite(const std::string& name_line, Satellite& sat)
{
    uint32 i = 0;
    uint32 line_len = name_line.length();

    const char* line_cstr = name_line.c_str();

    for (; i < line_len; i++) {
        char ch = name_line[i];
        if (ch == '-') {
            sat.Series = String(line_cstr, i);
            ++i;
            break;
        }
    }

    const char* ident_start = line_cstr + i;

    while (i < line_len)
        i++;

    sat.Identifier = String(ident_start, i);
}

uint32 CharToNumber(char ch)
{
    if (ch < '0') {
        return 0;
    }

    return uint32(ch - '0');
}


/// One element set as read from the file, before it has been propagated
struct TleEntry
{
    std::string Name;
    std::string Line1;
    std::string Line2;
    bool HasName;
};

void Dataset::LoadFromTLE(const String& tle_path)
{
    printf("Loading TLE at path %s\n", tle_path.CStr());

    std::ifstream fb(tle_path.CStr(), std::ios::in);

    Name = FindNameForPath(tle_path);

    std::string name_line;
    std::string line1;
    std::string line2;

    NumTimesteps = scNumTimeCaptures;

    // Read in all of the TLE information

    bool no_name_lines = false;

    std::vector<TleEntry> entries;


    while (true) {
        if (!std::getline(fb, name_line)) {
            break;
        }

        // Historic TLEs are prefixed with 0, modern ones are letters (usually).
        // If this TLE does not contain the metadata line, load from catalog
        if (name_line[0] == '1') {
            no_name_lines = true;
        }


        if (no_name_lines) {
            line1 = name_line;
        }
        else {
            std::getline(fb, line1);
        }
        std::getline(fb, line2);

        entries.push_back({ no_name_lines ? std::string() : name_line, line1, line2, !no_name_lines });
    }

    std::vector<float64> epochs;
    epochs.reserve(entries.size());

    for (const TleEntry& entry : entries) {
        TleContext tle_ctx {};
        tle_ctx.CreateFromEntry(entry.Line1.c_str(), entry.Line2.c_str());

        epochs.push_back(tle_ctx.SatRec.jdsatepoch + tle_ctx.SatRec.jdsatepochF);
    }

    if (!epochs.empty()) {
        std::nth_element(epochs.begin(), epochs.begin() + epochs.size() / 2, epochs.end());
        ReferenceJd = epochs[epochs.size() / 2];
    }

    // The instant and the Earth's rotation at each time step are shared by every satellite
    float64 sample_jd[scNumTimeCaptures];
    float64 sample_angle[scNumTimeCaptures];

    for (uint32 i = 0; i < scNumTimeCaptures; i++) {
        const float64 minutes = scStartTime + (scTimeJump * i);

        sample_jd[i] = ReferenceJd + (minutes / 1440.0);
        sample_angle[i] = SGP4Funcs::gstime_SGP4(sample_jd[i]) + scGreenwichSceneAngle;
    }

    for (const TleEntry& entry : entries) {
        TleContext tle_ctx {};
        tle_ctx.CreateFromEntry(entry.Line1.c_str(), entry.Line2.c_str());

        Satellite sat;

        char norad_id[6];
        memcpy(norad_id, entry.Line1.c_str() + 2, 5);

        sat.NoradId = atoi(String(norad_id, 5).CStr());
        sat.LaunchYear = CharToNumber(entry.Line1[9]) * 10 + CharToNumber(entry.Line1[10]);

        bool propagated = true;

        for (uint32 i = 0; i < scNumTimeCaptures; i++) {
            Satellite::TimeStep time_step {};
            if (!tle_ctx.GetTimeStepForTime(sample_jd[i], sample_angle[i], time_step)) {
                propagated = false;
                break;
            }

            sat.AddTimeStep(time_step);
        }

        // SGP4 rejects decayed or invalid element sets. Leave them out rather than place them somewhere wrong.
        if (!propagated) {
            printf("Skipping NORAD %u: SGP4 could not propagate it\n", sat.NoradId);
            continue;
        }

        if (entry.HasName) {
            SetNamesForSatellite(entry.Name, sat);
        }

        Satellites.push_back(sat);
    }

    if (no_name_lines) {
        PopulateFromCatalog();
    }
}

struct CatalogEntry
{
    String Name;
    String Identifier;
};

void Dataset::PopulateFromCatalog()
{
    std::ifstream fb(ASSET_BASE_DIR "/Datasets/satcat.csv", std::ios::in);

    std::string line;

    // Skip first line
    if (!std::getline(fb, line)) {
        return;
    }

    std::unordered_map<uint32, CatalogEntry, Hash32Stl> Catalog;

    char name[64];
    char object_id[64];
    char norad_id[8];

    // Load catalog information in
    while (true) {
        if (!std::getline(fb, line)) {
            break;
        }

        CatalogEntry entry {};

        // Load the name into the buffer
        int index;
        for (index = 0; line[index] != ','; index++) {
            name[index] = line[index];
        }

        name[index] = '\0';
        entry.Name = String(name, index);

        // Skip comma
        ++index;

        int buffer_index = 0;

        // Load the object id in
        for (buffer_index = 0; line[index] != ','; index++) {
            object_id[buffer_index++] = line[index];
        }
        object_id[buffer_index] = '\0';
        entry.Identifier = String(object_id, buffer_index);

        // Skip comma
        ++index;


        // Load the object id in
        for (buffer_index = 0; line[index] != ','; index++) {
            norad_id[buffer_index++] = line[index];
        }
        norad_id[buffer_index] = '\0';
        uint32 norad_number = atoi(norad_id);

        // Skip comma
        ++index;

        Catalog[norad_number] = std::move(entry);
    }


    for (Satellite& sat : Satellites) {
        auto it = Catalog.find(sat.NoradId);

        if (it == Catalog.end()) {
            continue;
        }


        sat.Name = it->second.Name;
        sat.Identifier = it->second.Identifier;

        uint32 i = 0;
        uint32 line_len = sat.Name.Length;

        const char* line_cstr = sat.Name.CStr();

        if (line_len > 4 && !strncmp(line_cstr + line_len - 3, "DEB", 3)) {
            sat.Series = String("Debris");
        }
        else {
            for (; i < line_len; i++) {
                char ch = line_cstr[i];
                if (ch == '-') {
                    sat.Series = String(line_cstr, i);
                    break;
                }
            }
        }

        if (sat.Series.Length == 0) {
            sat.Series = sat.Name;
        }
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
