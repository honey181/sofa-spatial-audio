#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct HrirPosition {
    float azimuthDeg;   // -180.0 to +180.0
    float elevationDeg; // -90.0 to +90.0
    float distance;     // meters
};

struct HrirEntry {
    HrirPosition pos;
    std::vector<float> leftHrir;
    std::vector<float> rightHrir;
};

class SofaDatabase {
public:
    SofaDatabase();
    ~SofaDatabase();

    // Load SOFA file
    bool LoadSofaFile(const std::string& sofaPath);

    // Initialize with high-quality spherical synthetic/baseline HRIRs (fallback or default)
    void InitializeDefaultDatabase();

    // Look up HRIR pair closest to Cartesian (x, y, z)
    void LookupHrir(float x, float y, float z, const std::vector<float>** ppLeft, const std::vector<float>** ppRight) const;

    // Convert Cartesian (x, y, z) to Spherical (Azimuth, Elevation, Distance)
    static void CartesianToSpherical(float x, float y, float z, float& azDeg, float& elDeg, float& dist);

    // Pre-bake headphone correction convolution directly into every HRIR in RAM
    void PrebakeHeadphoneEQ(const std::vector<float>& leftEqIR, const std::vector<float>& rightEqIR);

    uint32_t GetHrirLength() const { return m_hrirLength; }
    size_t GetMeasurementCount() const { return m_entries.size(); }
    bool IsLoaded() const { return !m_entries.empty(); }

private:
    std::vector<HrirEntry> m_entries;
    uint32_t m_hrirLength = 128; // Default 128 taps (2.6ms at 48kHz)
};
