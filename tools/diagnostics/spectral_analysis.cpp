#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "mysofa.h"

// Discrete Fourier Transform magnitude for frequency bands
void AnalyzeSpectrum(const char* label, const float* ir, int len, float sampleRate) {
    // Frequency bands to check:
    // Low: 250 - 1000 Hz
    // Mid: 1000 - 3000 Hz
    // Pinna Front Cue: 3000 - 6000 Hz (concha boost)
    // Pinna Notch / Height Cue: 6000 - 10000 Hz
    // Air Absorption / Shadow: 10000 - 16000 Hz

    auto bandEnergy = [&](float fLow, float fHigh) {
        float sum = 0.0f;
        int count = 0;
        for (float f = fLow; f <= fHigh; f += 50.0f) {
            float re = 0.0f, im = 0.0f;
            float omega = 2.0f * 3.14159265f * f / sampleRate;
            for (int n = 0; n < len; n++) {
                re += ir[n] * std::cos(omega * n);
                im -= ir[n] * std::sin(omega * n);
            }
            sum += (re * re + im * im);
            count++;
        }
        return count > 0 ? std::sqrt(sum / count) : 0.0f;
    };

    float eLow = bandEnergy(250, 1000);
    float eMid = bandEnergy(1000, 3000);
    float eConcha = bandEnergy(3000, 6000);
    float eNotch = bandEnergy(6000, 10000);
    float eHigh = bandEnergy(10000, 16000);

    std::cout << std::left << std::setw(20) << label
              << " | Low(0.2-1k): " << std::fixed << std::setprecision(2) << eLow
              << " | Mid(1-3k): " << eMid
              << " | Concha(3-6k): " << eConcha
              << " | Pinna(6-10k): " << eNotch
              << " | High(10-16k): " << eHigh
              << std::endl;
}

void TestSpectrum(const char* name, const char* path) {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(path, 48000.0f, &filterLen, &err);
    if (!easy) return;

    std::cout << "\n--- Frequency Spectrum Analysis: " << name << " ---\n";
    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;

    mysofa_getfilter_float(easy, 1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    AnalyzeSpectrum("FRONT (+X)", irL.data(), filterLen, 48000.0f);

    mysofa_getfilter_float(easy, -1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    AnalyzeSpectrum("BACK  (-X)", irL.data(), filterLen, 48000.0f);

    mysofa_getfilter_float(easy, 0.707f, 0.0f, 0.707f, irL.data(), irR.data(), &dL, &dR);
    AnalyzeSpectrum("UP (+45 deg)", irL.data(), filterLen, 48000.0f);

    mysofa_getfilter_float(easy, 0.707f, 0.0f, -0.707f, irL.data(), irR.data(), &dL, &dR);
    AnalyzeSpectrum("DOWN (-45 deg)", irL.data(), filterLen, 48000.0f);

    mysofa_close(easy);
}

int main() {
    TestSpectrum("MIT KEMAR", "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa");
    TestSpectrum("USER INDIVIDUALIZED", "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa");
    return 0;
}
