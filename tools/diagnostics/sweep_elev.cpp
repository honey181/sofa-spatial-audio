#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa";
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(path, 48000.0f, &filterLen, &err);
    if (!easy) return 1;

    std::cout << "Inspecting Elevation Sweep at Azimuth 0 (Direct Front):" << std::endl;
    std::cout << "Elev (deg) | Nearest idx | Nearest Coord (Az, El, R) | Delays (L, R) | Peak L / R | RMS L / R" << std::endl;

    std::vector<float> irL(filterLen);
    std::vector<float> irR(filterLen);

    for (int el = -20; el <= 20; el += 2) {
        float elRad = el * 3.14159265f / 180.0f;
        // At Azimuth 0 (Front):
        // X = cos(el) * 1.0 (Front)
        // Y = 0 (Left/Right)
        // Z = sin(el) * 1.0 (Up)
        float x = std::cos(elRad) * 0.76f;
        float y = 0.0f;
        float z = std::sin(elRad) * 0.76f;

        float c[3] = {x, y, z};
        int nearest = mysofa_lookup(easy->lookup, c);

        float delayL = 0.0f, delayR = 0.0f;
        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &delayL, &delayR);

        float sumL = 0, sumR = 0;
        int peakL = 0, peakR = 0;
        float maxL = 0, maxR = 0;
        for (int i = 0; i < filterLen; i++) {
            sumL += irL[i] * irL[i];
            sumR += irR[i] * irR[i];
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }

        float nAz = easy->hrtf->SourcePosition.values[nearest * 3 + 0];
        float nEl = easy->hrtf->SourcePosition.values[nearest * 3 + 1];
        float nR  = easy->hrtf->SourcePosition.values[nearest * 3 + 2];

        std::cout << std::setw(10) << el 
                  << " | " << std::setw(11) << nearest
                  << " | (" << std::fixed << std::setprecision(1) << nAz << ", " << nEl << ", " << nR << ")"
                  << " | (" << std::setprecision(2) << delayL << ", " << delayR << ")"
                  << " | (" << peakL << ", " << peakR << ")"
                  << " | (" << std::setprecision(3) << std::sqrt(sumL/filterLen) << ", " << std::sqrt(sumR/filterLen) << ")"
                  << std::endl;
    }

    mysofa_close(easy);
    return 0;
}
