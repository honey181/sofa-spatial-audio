#include <iostream>
#include <iomanip>
#include <cmath>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", &err);
    if (!hrtf) return 1;

    std::cout << "M=" << hrtf->M << " R=" << hrtf->R << " N=" << hrtf->N << std::endl;
    std::cout << "SamplingRate=" << hrtf->DataSamplingRate.values[0] << std::endl;

    std::cout << "Checking elements..." << std::endl;

    std::cout << "DataIR elements: " << hrtf->DataIR.elements << std::endl;
    std::cout << "DataDelay elements: " << hrtf->DataDelay.elements << std::endl;
    std::cout << "SourcePosition elements: " << hrtf->SourcePosition.elements << std::endl;
    if (!hrtf->DataIR.values || !hrtf->DataDelay.values || !hrtf->SourcePosition.values) {
        std::cout << "Null values!" << std::endl;
        return 1;
    }
    for (int m = 0; m < std::min(5, (int)hrtf->M); m++) {
        float x = hrtf->SourcePosition.values[m * 3 + 0];
        float y = hrtf->SourcePosition.values[m * 3 + 1];
        float z = hrtf->SourcePosition.values[m * 3 + 2];
        float dL = hrtf->DataDelay.values[m * 2 + 0];
        float dR = hrtf->DataDelay.values[m * 2 + 1];

        // Find peak sample in DataIR for Left and Right
        float* irL = hrtf->DataIR.values + m * (hrtf->R * hrtf->N);
        float* irR = irL + hrtf->N;

        int peakL = 0, peakR = 0;
        float maxL = 0.0f, maxR = 0.0f;
        for (int i = 0; i < hrtf->N; i++) {
            if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
            if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
        }

        std::cout << "Pos " << m << ": (" << std::fixed << std::setprecision(2) 
                  << x << ", " << y << ", " << z << ") | Delay=(" << dL << ", " << dR 
                  << ") | PeakL=" << peakL << " (amp=" << maxL << "), PeakR=" << peakR << " (amp=" << maxR << ")" << std::endl;
    }

    mysofa_free(hrtf);
    return 0;
}
