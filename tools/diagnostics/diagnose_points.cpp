#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "mysofa.h"

void test_sofa(const char* name, const char* path) {
    int len = 0, err = 0;
    auto easy = mysofa_open(path, 48000.0f, &len, &err);
    if (!easy) { std::cout << "Failed: " << path << std::endl; return; }
    std::cout << "\n=== " << name << " (filterLen=" << len << ") ===" << std::endl;
    
    auto check_pt = [&](const char* label, float x, float y, float z) {
        std::vector<float> L(len), R(len);
        float dL=0, dR=0;
        mysofa_getfilter_float(easy, x, y, z, L.data(), R.data(), &dL, &dR);
        float sumL=0, sumR=0;
        int maxL=0, maxR=0;
        float pL=0, pR=0;
        for (int i=0; i<len; i++) {
            sumL += L[i]*L[i]; sumR += R[i]*R[i];
            if (std::abs(L[i]) > pL) { pL = std::abs(L[i]); maxL = i; }
            if (std::abs(R[i]) > pR) { pR = std::abs(R[i]); maxR = i; }
        }
        std::cout << std::setw(18) << label << ": Peak L at " << std::setw(4) << maxL 
                  << " (val=" << std::fixed << std::setprecision(3) << pL << "), Peak R at " 
                  << std::setw(4) << maxR << " (val=" << pR << "), RMS L=" 
                  << std::sqrt(sumL/len) << ", R=" << std::sqrt(sumR/len) << std::endl;
    };
    
    check_pt("FRONT (x=1)", 1, 0, 0);
    check_pt("BACK (x=-1)", -1, 0, 0);
    check_pt("LEFT (y=1)", 0, 1, 0);
    check_pt("RIGHT (y=-1)", 0, -1, 0);
    check_pt("UP (+90 deg)", 0, 0, 1);
    check_pt("UP (+45 deg)", 0.707f, 0, 0.707f);
    check_pt("DOWN (-45 deg)", 0.707f, 0, -0.707f);
    check_pt("DOWN (-90 deg)", 0, 0, -1);
    check_pt("FRONT +5 deg", 0.996f, 0, 0.087f);
    check_pt("FRONT -5 deg", 0.996f, 0, -0.087f);
    mysofa_close(easy);
}

int main() {
    test_sofa("MIT KEMAR", "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa");
    test_sofa("INDIVIDUALIZED", "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa");
    return 0;
}
