#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "hrtf_dsp_engine.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// WAV File Writer helper
void WriteWavFile(const std::string& filename, const std::vector<float>& stereoPcm, uint32_t sampleRate) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return;

    uint32_t numSamples = (uint32_t)stereoPcm.size() / 2;
    uint32_t dataBytes = numSamples * 2 * sizeof(int16_t);

    // RIFF header
    file.write("RIFF", 4);
    uint32_t fileSize = 36 + dataBytes;
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    uint32_t fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t formatTag = 1; // PCM
    file.write(reinterpret_cast<const char*>(&formatTag), 2);
    uint16_t channels = 2; // Stereo
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    uint32_t byteRate = sampleRate * channels * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = channels * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataBytes), 4);

    // Convert float to 16-bit PCM
    std::vector<int16_t> intPcm(stereoPcm.size());
    for (size_t i = 0; i < stereoPcm.size(); i++) {
        float s = std::clamp(stereoPcm[i], -1.0f, 1.0f);
        intPcm[i] = (int16_t)(s * 32767.0f);
    }
    file.write(reinterpret_cast<const char*>(intPcm.data()), dataBytes);
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   SOFA Spatial Audio — Standalone 3D Sound Engine Test  \n";
    std::cout << "=========================================================\n\n";

    const uint32_t sampleRate = 48000;
    const uint32_t frameSize = 128; // Low latency 2.6ms buffer
    const float durationSeconds = 14.0f;
    const uint32_t totalFrames = (uint32_t)((durationSeconds * sampleRate) / frameSize);

    std::cout << "[1/3] Initializing CustomHrtfDspEngine...\n";
    CustomHrtfDspEngine engine(1); // 1 dynamic 3D object
    engine.Initialize(nullptr, 0);

    // Buffer allocations
    std::vector<float> monoInput(frameSize, 0.0f);
    std::vector<float> stereoOutputBuffer(frameSize * 2, 0.0f);
    std::vector<float> fullRecordedTrack;
    fullRecordedTrack.reserve(sampleRate * 2 * (size_t)durationSeconds);

    HrtfSpatialObjectData object;
    std::memset(&object, 0, sizeof(object));
    object.gain = 1.0f;
    object.flags = 1;
    *(const float**)((char*)&object + 0x68) = monoInput.data();
    *(uint32_t*)((char*)&object + 0x70) = frameSize;

    std::cout << "[2/3] Simulating 3D Trajectory in Space:\n";
    std::cout << "      • Seconds 0-6:  Orbiting 360 degrees around your ears\n";
    std::cout << "      • Seconds 6-11: Rising overhead into ceiling (Elevation +55 deg)\n";
    std::cout << "      • Seconds 11-14: Front overhead descent to front-center\n\n";

    float currentPhase = 0.0f;

    for (uint32_t frameIdx = 0; frameIdx < totalFrames; frameIdx++) {
        float timeSec = (float)(frameIdx * frameSize) / sampleRate;

        // 1. Generate acoustic test sound: rhythmic broadband clicks & warm synth pulses
        // (Broadband clicks are the easiest sound for the human brain to locate in 3D)
        for (uint32_t i = 0; i < frameSize; i++) {
            float sampleTime = timeSec + (float)i / sampleRate;
            float pulseInterval = std::fmod(sampleTime, 0.25f); // 4 pulses per second
            float envelope = (pulseInterval < 0.04f) ? std::sin(pulseInterval * ((float)M_PI / 0.04f)) : 0.0f;
            
            // Harmonic tone with white noise burst
            float tone = std::sin(2.0f * (float)M_PI * 550.0f * sampleTime) * 0.6f +
                         std::sin(2.0f * (float)M_PI * 1100.0f * sampleTime) * 0.3f;
            float noise = ((float)rand() / (float)RAND_MAX - 0.5f) * 0.2f;

            monoInput[i] = (tone + noise) * envelope * 0.6f;
        }

        // 2. Animate 3D Coordinates: (posX = Right, posY = Front, posZ = Up)
        float azDeg = 0.0f;
        float elDeg = 0.0f;
        float radius = 1.5f; // 1.5 meters away

        if (timeSec < 6.0f) {
            // Horizontal 360 circle
            float progress = timeSec / 6.0f;
            azDeg = progress * 360.0f; // 0 to 360
            elDeg = 0.0f;
        } else if (timeSec < 11.0f) {
            // Overhead spiral
            float progress = (timeSec - 6.0f) / 5.0f;
            azDeg = progress * 360.0f;
            elDeg = 55.0f; // Overhead!
        } else {
            // Descent from top to front
            float progress = (timeSec - 11.0f) / 3.0f;
            azDeg = 0.0f; // Directly in front
            elDeg = 55.0f * (1.0f - progress);
        }

        // Convert Azimuth & Elevation to Cartesian (X, Y, Z)
        float azRad = azDeg * ((float)M_PI / 180.0f);
        float elRad = elDeg * ((float)M_PI / 180.0f);

        object.posX = radius * std::sin(azRad) * std::cos(elRad);  // Right (+X) / Left (-X)
        object.posY = radius * std::sin(elRad);                    // Up (+Y) / Down (-Y)
        object.posZ = -radius * std::cos(azRad) * std::cos(elRad); // Front (-Z) / Back (+Z) in Windows Spatial Audio

        // 3. Process frame through our custom engine!
        // Copy mono input into buffer
        std::memcpy(stereoOutputBuffer.data(), monoInput.data(), frameSize * sizeof(float));

        engine.Process(&object, stereoOutputBuffer.data(), frameSize, 0);

        // Record audio
        fullRecordedTrack.insert(fullRecordedTrack.end(), stereoOutputBuffer.begin(), stereoOutputBuffer.end());

        // Console visualizer (every ~0.5 seconds)
        if (frameIdx % (sampleRate / (frameSize * 2)) == 0) {
            std::cout << "\r[Time " << std::fixed << std::setprecision(1) << timeSec << "s] "
                      << "Pos: Azimuth " << std::setw(5) << (int)azDeg << " deg | "
                      << "Elevation " << std::setw(3) << (int)elDeg << " deg ("
                      << (elDeg > 20.0f ? "OVERHEAD" : "EAR-LEVEL") << ")   " << std::flush;
        }
    }

    std::cout << "\n\n[3/3] Saving binaural audio file...\n";
    std::string outPath = "output_3d_test.wav";
    WriteWavFile(outPath, fullRecordedTrack, sampleRate);

    std::cout << "SUCCESS! Generated: " << outPath << " (" << durationSeconds << " seconds)\n";
    std::cout << "\nPlaying 3D sound through your headphones right now...\n";
    
    // Play sound directly to default audio device
    PlaySoundA(outPath.c_str(), NULL, SND_FILENAME | SND_SYNC);

    std::cout << "\nDone! You can also listen to output_3d_test.wav anytime with mpv or any player.\n";
    return 0;
}
