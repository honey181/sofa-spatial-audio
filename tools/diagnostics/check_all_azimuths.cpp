#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &filterLen, &err);
    if (!easy) return 1;

    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;

    std::cout << "Azimuth (deg) | Energy Left | Energy Right | Ratio (L/R) | Delay L | Delay R\n";
    std::cout << "----------------------------------------------------------------------------\n";

    for (int az = 0; az < 360; az += 15) {
        float azRad = az * 3.14159265f / 180.0f;
        float x = std::cos(azRad);
        float y = std::sin(azRad); // Left in SOFA (+Y)
        float z = 0.0f;

        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &dL, &dR);

        float eL = 0, eR = 0;
        for (int i = 0; i < filterLen; i++) {
            eL += irL[i] * irL[i];
            eR += irR[i] * irR[i];
        }

        std::cout << std::setw(13) << az << " | "
                  << std::setw(11) << std::fixed << std::setprecision(3) << eL << " | "
                  << std::setw(12) << eR << " | "
                  << std::setw(11) << (eR > 1e-4f ? (eL / eR) : 999.0f) << " | "
                  << std::setw(7) << std::setprecision(1) << dL << " | "
                  << std::setw(7) << dR << "\n";
    }

    mysofa_close(easy);
    return 0;
}
