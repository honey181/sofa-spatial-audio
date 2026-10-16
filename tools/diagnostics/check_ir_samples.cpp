#include <iostream>
#include <vector>
#include <iomanip>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) return 1;

    // Find position with Azimuth = 90 deg, El = 0 deg
    int targetIdx = -1;
    for (unsigned int i = 0; i < hrtf->M; i++) {
        float az = hrtf->SourcePosition.values[i * 3 + 0];
        float el = hrtf->SourcePosition.values[i * 3 + 1];
        if (std::abs(az - 90.0f) < 0.1f && std::abs(el - 0.0f) < 0.1f) {
            targetIdx = i;
            break;
        }
    }

    if (targetIdx >= 0) {
        std::cout << "Found Az=90, El=0 at index " << targetIdx << std::endl;
        std::cout << "DataDelay: L=" << hrtf->DataDelay.values[targetIdx * 2]
                  << ", R=" << hrtf->DataDelay.values[targetIdx * 2 + 1] << std::endl;
        
        float* irL = hrtf->DataIR.values + targetIdx * hrtf->N * 2;
        float* irR = irL + hrtf->N;

        std::cout << "\nFirst 45 samples of IR:" << std::endl;
        for (int i = 0; i < 45; i++) {
            std::cout << "sample " << std::setw(2) << i 
                      << " | L: " << std::fixed << std::setprecision(5) << irL[i]
                      << " | R: " << irR[i] << std::endl;
        }
    } else {
        std::cout << "Position not found" << std::endl;
    }

    mysofa_free(hrtf);
    return 0;
}
