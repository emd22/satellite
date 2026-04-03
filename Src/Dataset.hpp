#pragma once

#include "Satellite.hpp"

#include <iosfwd>
#include <vector>

class String;


class Dataset
{
public:
    void LoadFromTLE(const String& tle_path, const String& dst_path);
    void LoadFromBin(const String& bin_path);

    void SaveToBin(const String& dst_path);

    FORCE_INLINE uint32 Size() const { return Satellites.size(); }

    const Satellite& GetSatellite(uint32 index) { return Satellites[index]; }

private:
    static void ReadCacheEntry(std::ifstream& stream, Satellite& sat);

public:
    std::vector<Satellite> Satellites;

    uint32 NumTimesteps = 0;
};
