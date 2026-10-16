#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &filterLen, &err);
    if (!easy) return 1;

    std::vector<float> irL(filterLen), irR(filterLen);
    float delayL = 0.0f, delayR = 0.0f;

    std::cout << "Azimuth (deg) | PeakL (amp) | PeakR (amp) | ITD (samples = L - R)" << std::endl;
    for (int deg = 0; deg < 360; deg += 15) {
        float rad = deg * (float)M_PI / 180.0f;
        // In SOFA: +X = Front, +Y = Left, -Y = Right, -X = Back
        // deg: 0 = Front, 90 = Left, 180 = Back, 270 = Right
        float x = std::cos(rad);
        float y = std::sin(rad);
        float z = 0.0f;

        mysofa_getfilter_float(easy, x, y, z, irL.data(), irR.data(), &delayL, &delayR);
        int peakL = 0, peakR = 0;
        float maxL = 0.0f, maxR = 0.0f;
        for (int i = 0; i < filterLen; i++) {
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }
        std::cout << std::setw(13) << deg << " | " 
                  << std::setw(5) << peakL << " (" << std::fixed << std::setprecision(2) << maxL << ") | "
                  << std::setw(5) << peakR << " (" << std::fixed << std::setprecision(2) << maxR << ") | "
                  << (peakL - peakR) << std::endl;
    }

    mysofa_close(easy);
    return 0;
}
