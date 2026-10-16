#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &filterLen, &err);
    if (!easy) {
        std::cout << "easy open err: " << err << std::endl;
        return 1;
    }

    std::cout << "mysofa_open filterLen: " << filterLen << std::endl;

    std::vector<float> irL(filterLen), irR(filterLen);
    float delayL = 0.0f, delayR = 0.0f;

    // Check at Front (x=1, y=0, z=0), Left (x=0, y=1, z=0), Right (x=0, y=-1, z=0), Back (x=-1, y=0, z=0)
    struct Dir { const char* name; float x, y, z; };
    Dir dirs[] = {
        {"Front (0 az)", 1.0f, 0.0f, 0.0f},
        {"Left (+90 az)", 0.0f, 1.0f, 0.0f},
        {"Right (-90 az)", 0.0f, -1.0f, 0.0f},
        {"Back (180 az)", -1.0f, 0.0f, 0.0f},
        {"Up (+90 el)", 0.0f, 0.0f, 1.0f},
        {"Down (-90 el)", 0.0f, 0.0f, -1.0f}
    };

    for (const auto& d : dirs) {
        mysofa_getfilter_float(easy, d.x, d.y, d.z, irL.data(), irR.data(), &delayL, &delayR);
        int peakL = 0, peakR = 0;
        float maxL = 0.0f, maxR = 0.0f;
        for (int i = 0; i < filterLen; i++) {
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }
        std::cout << std::left << std::setw(18) << d.name 
                  << " | delayL=" << delayL << " delayR=" << delayR 
                  << " | PeakL=" << peakL << " (amp=" << maxL 
                  << "), PeakR=" << peakR << " (amp=" << maxR << ")" << std::endl;
    }

    mysofa_close(easy);
    return 0;
}
