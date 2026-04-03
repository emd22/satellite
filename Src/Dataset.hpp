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

    const Satellite& GetSatellite(uint32 index) { return Satellites[index]; }

public:
    String Name = "Unknown";
    uint32 NumTimesteps = 0;

    std::vector<Satellite> Satellites;
};
