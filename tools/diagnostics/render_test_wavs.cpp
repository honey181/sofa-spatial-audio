#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include "mysofa.h"

// Write simple 16-bit stereo WAV
void WriteWav(const char* filename, const float* left, const float* right, int numSamples, int sampleRate) {
    std::ofstream f(filename, std::ios::binary);
    int byteRate = sampleRate * 2 * 2;
    int blockAlign = 4;
    int dataBytes = numSamples * 2 * 2;
    int chunkBytes = 36 + dataBytes;

    f.write("RIFF", 4);
    f.write((char*)&chunkBytes, 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    int sub1 = 16;
    short format = 1, channels = 2, bits = 16;
    f.write((char*)&sub1, 4);
    f.write((char*)&format, 2);
    f.write((char*)&channels, 2);
    f.write((char*)&sampleRate, 4);
    f.write((char*)&byteRate, 4);
    f.write((char*)&blockAlign, 2);
    f.write((char*)&bits, 2);
    f.write("data", 4);
    f.write((char*)&dataBytes, 4);

    for (int i = 0; i < numSamples; i++) {
        short sl = (short)std::clamp(left[i] * 32767.0f, -32768.0f, 32767.0f);
        short sr = (short)std::clamp(right[i] * 32767.0f, -32768.0f, 32767.0f);
        f.write((char*)&sl, 2);
        f.write((char*)&sr, 2);
    }
}

uint32_t g_seed = 123456789;
float FastRand() {
    g_seed = (g_seed * 1664525u + 1013904223u);
    return ((float)(g_seed & 0x00FFFFFF) / (float)0x007FFFFF) - 1.0f;
}

void ConvolvePosition(const char* datasetPath, const char* outWav, float curX, float curY, float curZ) {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(datasetPath, 48000.0f, &filterLen, &err);
    if (!easy) {
        std::cout << "Failed to open " << datasetPath << std::endl;
        return;
    }

    // Convert User coords (+X=Right, +Y=Up, +Z=Front) to SOFA (+X=Front, +Y=Left, +Z=Up)
    float x_sofa = curZ;
    float y_sofa = -curX;
    float z_sofa = curY;

    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;
    mysofa_getfilter_float(easy, x_sofa, y_sofa, z_sofa, irL.data(), irR.data(), &dL, &dR);

    int totalSamples = 48000 * 2; // 2 seconds
    std::vector<float> dry(totalSamples);
    float dt = 1.0f / 48000.0f;
    float timer = 0.0f;
    for (int i = 0; i < totalSamples; i++) {
        timer += dt;
        float cycle = std::fmod(timer, 0.28f);
        float s = 0.0f;
        if (cycle < 0.10f) {
            float env = std::sin(cycle * (3.14159265f / 0.10f));
            s = FastRand() * env * 0.35f;
        }
        dry[i] = s;
    }

    std::vector<float> outL(totalSamples, 0.0f);
    std::vector<float> outR(totalSamples, 0.0f);

    uint32_t convLen = std::min((uint32_t)filterLen, 1024u);
    for (int i = 0; i < totalSamples; i++) {
        float sl = 0.0f, sr = 0.0f;
        for (uint32_t k = 0; k < convLen; k++) {
            if (i >= (int)k) {
                float val = dry[i - k];
                sl += val * irL[k];
                sr += val * irR[k];
            }
        }
        outL[i] = sl * 1.5f;
        outR[i] = sr * 1.5f;
    }

    WriteWav(outWav, outL.data(), outR.data(), totalSamples, 48000);
    std::cout << "Rendered: " << outWav << " (x_sofa=" << x_sofa << ", y_sofa=" << y_sofa << ", z_sofa=" << z_sofa << ")" << std::endl;

    mysofa_close(easy);
}

int main() {
    std::cout << "--- KEMAR ---" << std::endl;
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", "kemar_front.wav", 0.0f, 0.0f, 1.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", "kemar_back.wav", 0.0f, 0.0f, -1.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", "kemar_up.wav", 0.0f, 1.0f, 0.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", "kemar_down.wav", 0.0f, -1.0f, 0.0f);

    std::cout << "\n--- INDIVIDUALIZED ---" << std::endl;
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", "indiv_front.wav", 0.0f, 0.0f, 1.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", "indiv_back.wav", 0.0f, 0.0f, -1.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", "indiv_up.wav", 0.0f, 1.0f, 0.0f);
    ConvolvePosition("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", "indiv_down.wav", 0.0f, -1.0f, 0.0f);

    return 0;
}
