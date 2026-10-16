#include <iostream>
#include <vector>
#include <cmath>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &filterLen, &err);
    if (!easy) return 1;

    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;

    float elevations[] = {-57.0f, -30.0f, -15.0f, 0.0f, 15.0f, 30.0f, 45.0f, 60.0f, 75.0f};

    std::cout << "Elevation (deg) | Peak Sample L | Peak Sample R | Max Val L | Max Val R\n";
    std::cout << "--------------------------------------------------------------------\n";

    for (float elDeg : elevations) {
        float elRad = elDeg * 3.14159265f / 180.0f;
        float x = std::cos(elRad); // Front
        float y = 0.0f;
        float z = std::sin(elRad); // Up

        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &dL, &dR);

        int peakL = 0, peakR = 0;
        float maxL = 0, maxR = 0;
        for (int i = 0; i < filterLen; i++) {
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }

        std::cout << "  " << elDeg << " deg\t| " << peakL << "\t\t| " << peakR << "\t\t| " << maxL << "\t| " << maxR << "\n";
    }

    mysofa_close(easy);
    return 0;
}
