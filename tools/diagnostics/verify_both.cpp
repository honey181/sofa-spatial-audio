#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "mysofa.h"

void TestDataset(const char* name, const char* path) {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(path, 48000.0f, &filterLen, &err);
    if (!easy) {
        std::cout << "=== " << name << " FAILED TO OPEN (err=" << err << ") ===" << std::endl;
        return;
    }

    std::cout << "\n=======================================================\n";
    std::cout << " DATASET: " << name << " (" << filterLen << " taps)\n";
    std::cout << "=======================================================\n";

    auto probe = [&](const char* label, float x, float y, float z) {
        std::vector<float> irL(filterLen), irR(filterLen);
        float dL = 0, dR = 0;
        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &dL, &dR);
        
        float eL = 0, eR = 0;
        for (int i = 0; i < filterLen; i++) {
            eL += irL[i] * irL[i];
            eR += irR[i] * irR[i];
        }

        // Find peak sample index (arrival time / latency)
        int peakL = 0, peakR = 0;
        float maxL = 0, maxR = 0;
        for (int i = 0; i < filterLen; i++) {
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }

        std::cout << std::left << std::setw(18) << label
                  << " [x=" << std::setw(4) << x << ", y=" << std::setw(4) << y << ", z=" << std::setw(4) << z << "]"
                  << " | Energy L=" << std::fixed << std::setprecision(3) << eL
                  << ", R=" << std::fixed << std::setprecision(3) << eR
                  << " | Peak L@" << peakL << " (v=" << std::setprecision(2) << maxL << ")"
                  << ", R@" << peakR << " (v=" << std::setprecision(2) << maxR << ")"
                  << " | Delays L=" << std::setprecision(4) << dL << ", R=" << dR
                  << std::endl;
    };

    // In AES69 SOFA Cartesian:
    // +X = Front, -X = Back
    // +Y = Left,  -Y = Right
    // +Z = Up,    -Z = Down
    probe("FRONT (+X)",      1.0f,  0.0f,  0.0f);
    probe("BACK  (-X)",     -1.0f,  0.0f,  0.0f);
    probe("LEFT  (+Y)",      0.0f,  1.0f,  0.0f);
    probe("RIGHT (-Y)",      0.0f, -1.0f,  0.0f);
    probe("UP (+45 deg)",    0.707f, 0.0f,  0.707f);
    probe("DOWN (-45 deg)",  0.707f, 0.0f, -0.707f);
    probe("FRONT-UP (+Z)",   0.0f,  0.0f,  1.0f);
    probe("FRONT-DOWN (-Z)", 0.0f,  0.0f, -1.0f);

    mysofa_close(easy);
}

int main() {
    TestDataset("MIT KEMAR (default_kemar.sofa)", "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa");
    TestDataset("USER INDIVIDUALIZED (custom_individualized.sofa)", "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa");
    return 0;
}
