#include "sofa_loader.h"
#include "headphone_eq.h"
#include "spatial_logger.h"
#include "mysofa.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SofaDatabase::SofaDatabase() {
    InitializeDefaultDatabase();
}

SofaDatabase::~SofaDatabase() {}

void SofaDatabase::CartesianToSpherical(float x, float y, float z, float& azDeg, float& elDeg, float& dist) {
    dist = std::sqrt(x * x + y * y + z * z);
    if (dist < 1e-6f) {
        azDeg = 0.0f;
        elDeg = 0.0f;
        dist = 0.0f;
        return;
    }

    // Input coordinates from Windows Spatial Audio (DirectX Right-Handed):
    // x = Right (+X), -x = Left (-X)
    // y = Height Up (+Y), -y = Down (-Y)
    // -z = Depth Front (-Z), +z = Back (+Z)
    float forward = -z;
    float right = x;
    float up = y;
    float horizDist = std::sqrt(right * right + forward * forward);

    // Azimuth in horizontal plane: 0 = Front, +90 = Right, -90 = Left, 180 = Rear
    azDeg = std::atan2(right, forward) * (180.0f / (float)M_PI);

    // Elevation: angle above/below horizontal plane (-90 to +90)
    elDeg = std::atan2(up, horizDist) * (180.0f / (float)M_PI);
}

// Generate analytical spherical head model (Woodworth formula ITD + head-shadowing ILD)
void SofaDatabase::InitializeDefaultDatabase() {
    m_entries.clear();
    m_hrirLength = 128; // 128 samples @ 48kHz = 2.66ms

    const float sampleRate = 48000.0f;
    const float headRadius = 0.0875f; // 8.75 cm human head radius
    const float speedOfSound = 343.0f; // m/s

    // Generate grid: Azimuth from -180 to +180 in 10 deg steps, Elevation from -40 to +60 in 15 deg steps
    for (int el = -40; el <= 60; el += 15) {
        float elRad = el * ((float)M_PI / 180.0f);
        for (int az = -180; az < 180; az += 10) {
            float azRad = az * ((float)M_PI / 180.0f);

            HrirEntry entry;
            entry.pos.azimuthDeg = (float)az;
            entry.pos.elevationDeg = (float)el;
            entry.pos.distance = 1.0f;

            entry.leftHrir.assign(m_hrirLength, 0.0f);
            entry.rightHrir.assign(m_hrirLength, 0.0f);

            // Woodworth ITD calculation:
            float thetaL = azRad + (float)M_PI * 0.5f; // Angle from left ear
            float thetaR = azRad - (float)M_PI * 0.5f; // Angle from right ear

            float delayL = (headRadius / speedOfSound) * (1.0f - std::sin(thetaL));
            float delayR = (headRadius / speedOfSound) * (1.0f - std::sin(thetaR));

            int tapL = std::clamp((int)(delayL * sampleRate), 0, (int)m_hrirLength - 4);
            int tapR = std::clamp((int)(delayR * sampleRate), 0, (int)m_hrirLength - 4);

            // Head shadow attenuation (ILD)
            float gainL = 0.5f * (1.0f - 0.7f * std::sin(azRad)) * std::cos(elRad);
            float gainR = 0.5f * (1.0f + 0.7f * std::sin(azRad)) * std::cos(elRad);

            // Pinna spectral notch simulation for height perception (elevation cue)
            float heightCrest = 1.0f + 0.3f * std::sin(elRad);

            entry.leftHrir[tapL] = gainL * heightCrest;
            entry.leftHrir[tapL + 1] = gainL * 0.4f;
            entry.rightHrir[tapR] = gainR * heightCrest;
            entry.rightHrir[tapR + 1] = gainR * 0.4f;

            m_entries.push_back(std::move(entry));
        }
    }
}

void SofaDatabase::LookupHrir(float x, float y, float z, const std::vector<float>** ppLeft, const std::vector<float>** ppRight) const {
    if (m_entries.empty()) {
        *ppLeft = nullptr;
        *ppRight = nullptr;
        return;
    }

    float targetAz, targetEl, dist;
    CartesianToSpherical(x, y, z, targetAz, targetEl, dist);

    float bestDistance = 1e9f;
    const HrirEntry* bestMatch = &m_entries[0];

    float targetAzRad = targetAz * ((float)M_PI / 180.0f);
    float targetElRad = targetEl * ((float)M_PI / 180.0f);

    for (const auto& entry : m_entries) {
        float entryAzRad = entry.pos.azimuthDeg * ((float)M_PI / 180.0f);
        float entryElRad = entry.pos.elevationDeg * ((float)M_PI / 180.0f);

        // Great circle distance on unit sphere
        float cosDist = std::sin(targetElRad) * std::sin(entryElRad) +
                        std::cos(targetElRad) * std::cos(entryElRad) * std::cos(targetAzRad - entryAzRad);
        cosDist = std::clamp(cosDist, -1.0f, 1.0f);
        float angularDist = std::acos(cosDist);

        if (angularDist < bestDistance) {
            bestDistance = angularDist;
            bestMatch = &entry;
        }
    }

    *ppLeft = &bestMatch->leftHrir;
    *ppRight = &bestMatch->rightHrir;
}

void SofaDatabase::PrebakeHeadphoneEQ(const std::vector<float>& leftEqIR, const std::vector<float>& rightEqIR) {
    if (leftEqIR.empty() || rightEqIR.empty()) return;

    SpatialLogger::Get().LogFormat("[SOFA] Pre-baking headphone EQ convolution into %zu HRIR directions in RAM...", m_entries.size());

    for (auto& entry : m_entries) {
        entry.leftHrir = HeadphoneEQ::ConvolveSignals(entry.leftHrir, leftEqIR);
        entry.rightHrir = HeadphoneEQ::ConvolveSignals(entry.rightHrir, rightEqIR);
        
        // Truncate to reasonable bounds (512 taps = 10.6ms) to keep frame processing real-time
        if (entry.leftHrir.size() > 512) {
            entry.leftHrir.resize(512);
            entry.rightHrir.resize(512);
        }
    }
    if (!m_entries.empty()) {
        m_hrirLength = (uint32_t)m_entries[0].leftHrir.size();
    }

    SpatialLogger::Get().LogFormat("[SOFA] Pre-bake complete. Final HRIR length: %u taps", m_hrirLength);
}

bool SofaDatabase::LoadSofaFile(const std::string& sofaPath) {
    if (sofaPath.empty()) {
        SpatialLogger::Get().Log("[SOFA] No SOFA file specified in config, using baseline high-precision spherical database.");
        return false;
    }

    int filterLength = 0;
    int err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(sofaPath.c_str(), 48000.0f, &filterLength, &err);
    if (!easy) {
        SpatialLogger::Get().LogFormat("[SOFA ERROR] Failed to open '%s' (mysofa error code: %d). Falling back to baseline.", sofaPath.c_str(), err);
        return false;
    }

    SpatialLogger::Get().LogFormat("[SOFA] Successfully opened '%s' (filter length: %d taps)", sofaPath.c_str(), filterLength);

    m_entries.clear();
    uint32_t targetTaps = (filterLength > 512) ? 512u : (uint32_t)filterLength;
    m_hrirLength = targetTaps;
    std::vector<float> leftBuf(filterLength);
    std::vector<float> rightBuf(filterLength);

    bool isCustom = (sofaPath.find("custom_individualized") != std::string::npos || sofaPath.find("Individualized") != std::string::npos);

    // Sample spatial sphere (10 degree azimuth, 10 degree elevation: 36 * 19 = 684 directional points)
    for (int el = -90; el <= 90; el += 10) {
        float elRad = el * ((float)M_PI / 180.0f);
        for (int az = -180; az < 180; az += 10) {
            float azRad = az * ((float)M_PI / 180.0f);

            // SOFA Cartesian (AES69): X=front, Y=left, Z=up
            float x_sofa = std::cos(elRad) * std::cos(azRad);
            float y_sofa = -std::cos(elRad) * std::sin(azRad);
            float z_sofa = std::sin(elRad);

            float delayL = 0.0f, delayR = 0.0f;
            mysofa_getfilter_float(easy, x_sofa, y_sofa, z_sofa, leftBuf.data(), rightBuf.data(), &delayL, &delayR);



            HrirEntry entry;
            entry.pos.azimuthDeg = (float)az;
            entry.pos.elevationDeg = (float)el;
            entry.pos.distance = 1.0f;

            entry.leftHrir.resize(targetTaps);
            entry.rightHrir.resize(targetTaps);
            for (uint32_t i = 0; i < targetTaps; i++) {
                float window = 1.0f;
                if (targetTaps < (uint32_t)filterLength && i >= targetTaps - 32) {
                    float fade = (float)(targetTaps - 1 - i) / 31.0f;
                    window = 0.5f * (1.0f - std::cos(fade * (float)M_PI));
                }
                entry.leftHrir[i] = leftBuf[i] * window;
                entry.rightHrir[i] = rightBuf[i] * window;
            }

            m_entries.push_back(std::move(entry));
        }
    }

    mysofa_close(easy);
    SpatialLogger::Get().LogFormat("[SOFA] Populated %zu spatial positions from SOFA dataset into RAM.", m_entries.size());
    return true;
}
