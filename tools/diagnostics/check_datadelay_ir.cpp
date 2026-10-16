#include <iostream>
#include <vector>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) return 1;

    std::cout << "DataDelay.elements: " << hrtf->DataDelay.elements << std::endl;
    std::cout << "First 5 Delay pairs (Left, Right):" << std::endl;
    for (int i = 0; i < 5; i++) {
        std::cout << "  Pos " << i << ": L=" << hrtf->DataDelay.values[i * 2] << " R=" << hrtf->DataDelay.values[i * 2 + 1] << std::endl;
    }

    // Now check where the peak or energy is in DataIR for pos 0
    int N = hrtf->N;
    float* irL = hrtf->DataIR.values;
    float* irR = hrtf->DataIR.values + N;
    
    int maxIdxL = 0, maxIdxR = 0;
    float maxValL = 0, maxValR = 0;
    for (int i = 0; i < N; i++) {
        if (std::abs(irL[i]) > maxValL) { maxValL = std::abs(irL[i]); maxIdxL = i; }
        if (std::abs(irR[i]) > maxValR) { maxValR = std::abs(irR[i]); maxIdxR = i; }
    }
    std::cout << "Pos 0 IR peak: L at sample " << maxIdxL << " (val=" << maxValL << "), R at sample " << maxIdxR << " (val=" << maxValR << ")" << std::endl;

    // Check pos 10 (which might be to the side)
    int pos10 = 10;
    float* irL10 = hrtf->DataIR.values + pos10 * 2 * N;
    float* irR10 = hrtf->DataIR.values + pos10 * 2 * N + N;
    maxIdxL = 0; maxIdxR = 0; maxValL = 0; maxValR = 0;
    for (int i = 0; i < N; i++) {
        if (std::abs(irL10[i]) > maxValL) { maxValL = std::abs(irL10[i]); maxIdxL = i; }
        if (std::abs(irR10[i]) > maxValR) { maxValR = std::abs(irR10[i]); maxIdxR = i; }
    }
    std::cout << "Pos 10 (Delays L=" << hrtf->DataDelay.values[pos10 * 2] << " R=" << hrtf->DataDelay.values[pos10 * 2 + 1] 
              << "): IR peak: L at " << maxIdxL << ", R at " << maxIdxR << std::endl;

    mysofa_free(hrtf);
    return 0;
}
