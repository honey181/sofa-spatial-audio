#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "mysofa.h"

void TestElevationTransition(const char* name, const char* path) {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(path, 48000.0f, &filterLen, &err);
    if (!easy) return;

    std::cout << "\n=======================================================\n";
    std::cout << " Elevation Transition Test: " << name << "\n";
    std::cout << "=======================================================\n";

    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;

    for (float elevDeg = -20.0f; elevDeg <= 20.0f; elevDeg += 2.0f) {
        float elRad = elevDeg * 3.14159265f / 180.0f;
        // Looking directly ahead in Front: x_sofa = cos(el), y_sofa = 0, z_sofa = sin(el)
        float x = std::cos(elRad);
        float y = 0.0f;
        float z = std::sin(elRad);

        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &dL, &dR);

        float energy = 0.0f;
        int peakIdx = 0;
        float maxVal = 0.0f;
        for (int i = 0; i < filterLen; i++) {
            energy += irL[i] * irL[i];
            if (std::abs(irL[i]) > maxVal) { maxVal = std::abs(irL[i]); peakIdx = i; }
        }

        std::cout << "Elev: " << std::setw(5) << std::fixed << std::setprecision(1) << elevDeg << " deg"
                  << " | Energy=" << std::setprecision(3) << energy
                  << " | Peak@" << std::setw(2) << peakIdx << " (val=" << std::setprecision(2) << maxVal << ")"
                  << " | Delay=" << dL
                  << std::endl;
    }

    mysofa_close(easy);
}

int main() {
    TestElevationTransition("MIT KEMAR", "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa");
    TestElevationTransition("USER INDIVIDUALIZED", "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa");
    return 0;
}
