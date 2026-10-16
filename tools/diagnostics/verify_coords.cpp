#include <iostream>
#include <vector>
#include <cmath>
#include "mysofa.h"

int main() {
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", 48000.0f, &filterLen, &err);
    if (!easy) {
        std::cout << "Open failed" << std::endl;
        return 1;
    }

    std::vector<float> irL(filterLen), irR(filterLen);
    float dL=0, dR=0;

    // Test 1: Sound on RIGHT
    mysofa_getfilter_float(easy, 0.0f, -1.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    float eL_R = 0, eR_R = 0;
    for (int i=0; i<filterLen; i++) {
        eL_R += irL[i]*irL[i];
        eR_R += irR[i]*irR[i];
    }
    std::cout << "Target RIGHT (x=0, y=-1): Energy L=" << eL_R << " R=" << eR_R << std::endl;

    // Test 2: Sound on LEFT
    mysofa_getfilter_float(easy, 0.0f, 1.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    float eL_L = 0, eR_L = 0;
    for (int i=0; i<filterLen; i++) {
        eL_L += irL[i]*irL[i];
        eR_L += irR[i]*irR[i];
    }
    std::cout << "Target LEFT  (x=0, y=+1): Energy L=" << eL_L << " R=" << eR_L << std::endl;

    // Test 3: Sound in FRONT vs BACK
    mysofa_getfilter_float(easy, 1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    float eFront = 0;
    for (int i=0; i<filterLen; i++) eFront += irL[i]*irL[i];

    mysofa_getfilter_float(easy, -1.0f, 0.0f, 0.0f, irL.data(), irR.data(), &dL, &dR);
    float eBack = 0;
    for (int i=0; i<filterLen; i++) eBack += irL[i]*irL[i];

    std::cout << "FRONT (+X=1): Energy=" << eFront << std::endl;
    std::cout << "BACK  (-X=1): Energy=" << eBack << std::endl;

    mysofa_close(easy);
    return 0;
}
