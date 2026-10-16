#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &filterLen, &err);
    if (!easy) return 1;

    std::cout << "Custom Individualized Front/Back:" << std::endl;
    std::vector<float> irL(filterLen), irR(filterLen);
    float delayL = 0.0f, delayR = 0.0f;

    // Front: X=1, Y=0, Z=0
    mysofa_getfilter_float(easy, 1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &delayL, &delayR);
    std::cout << "KEMAR Front (X=1, Y=0, Z=0):" << std::endl;
    float energyFront = 0.0f;
    for (int i = 0; i < filterLen; i++) energyFront += irL[i] * irL[i];
    std::cout << "  Energy=" << energyFront << std::endl;

    // Back: X=-1, Y=0, Z=0
    mysofa_getfilter_float(easy, -1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &delayL, &delayR);
    std::cout << "KEMAR Back (X=-1, Y=0, Z=0):" << std::endl;
    float energyBack = 0.0f;
    for (int i = 0; i < filterLen; i++) energyBack += irL[i] * irL[i];
    std::cout << "  Energy=" << energyBack << std::endl;

    // Check frequency response: Front vs Back (ratio of high freq energy > 5kHz)
    // In human ears, pinna blocks high frequencies from behind (Back has MUCH LESS high frequency energy than Front)
    mysofa_close(easy);
    return 0;
}
