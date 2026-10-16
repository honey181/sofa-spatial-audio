#include <iostream>
#include <iomanip>
#include <cmath>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", &err);
    if (!hrtf) return 1;

    std::cout << "Finding raw measurements near Azimuth 0, Elevation 0..." << std::endl;
    for (int m = 0; m < (int)hrtf->M; m++) {
        float az = hrtf->SourcePosition.values[m * 3 + 0];
        float el = hrtf->SourcePosition.values[m * 3 + 1];
        float dist = hrtf->SourcePosition.values[m * 3 + 2];

        if (std::abs(el) < 5.0f && (az < 20.0f || az > 340.0f)) {
            float* irL = hrtf->DataIR.values + m * (hrtf->R * hrtf->N);
            float* irR = irL + hrtf->N;

            int peakL = 0, peakR = 0;
            float maxL = 0.0f, maxR = 0.0f;
            for (int i = 0; i < hrtf->N; i++) {
                if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
                if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
            }

            std::cout << "Idx " << std::setw(3) << m << " | Az=" << std::setw(6) << az 
                      << " El=" << std::setw(5) << el 
                      << " | PeakL=" << std::setw(2) << peakL << " (" << std::fixed << std::setprecision(2) << maxL << ")"
                      << " PeakR=" << std::setw(2) << peakR << " (" << std::fixed << std::setprecision(2) << maxR << ")"
                      << " Delay=(" << hrtf->DataDelay.values[m * 2 + 0] << ", " << hrtf->DataDelay.values[m * 2 + 1] << ")"
                      << std::endl;
        }
    }

    mysofa_free(hrtf);
    return 0;
}
