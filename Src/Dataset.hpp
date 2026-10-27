#pragma once

#include "Satellite.hpp"
#include "String.hpp"

#include <iosfwd>
#include <vector>

class String;


class Dataset
{
public:
    void LoadFromTLE(const String& tle_path);
    void LoadFromBin(const String& bin_path);

    void SaveToBin(const String& dst_path);

    FORCE_INLINE uint32 Size() const { return Satellites.size(); }

    const Satellite& GetSatellite(uint32 index) const { return Satellites[index]; }
    Satellite& GetSatellite(uint32 index) { return Satellites[index]; }

    void PopulateFromCatalog();

public:
    String Name = "Unknown";
    uint32 NumTimesteps = 0;

    /// Julian date (UT) that every satellite is propagated to. Time step 0 is this instant plus scStartTime.
    float64 ReferenceJd = 0.0;

    std::vector<Satellite> Satellites;
};
