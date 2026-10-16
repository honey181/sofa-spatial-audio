#include <iostream>
#include <iomanip>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", &err);
    if (!hrtf) return 1;

    // Index 216 is Az=0, El=0
    float* irL = hrtf->DataIR.values + 216 * (hrtf->R * hrtf->N);
    float* irR = irL + hrtf->N;

    std::cout << "Raw IR samples at Az=0, El=0 (sample 8 to 25):" << std::endl;
    std::cout << "Sample:  ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << i << " ";
    std::cout << "\nLeft:    ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irL[i] * 1000) << " ";
    std::cout << "\nRight:   ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irR[i] * 1000) << " ";
    std::cout << "\n";

    // Compare with Index 219 (Az=15, El=0)
    float* irL15 = hrtf->DataIR.values + 219 * (hrtf->R * hrtf->N);
    float* irR15 = irL15 + hrtf->N;
    std::cout << "\nRaw IR samples at Az=15, El=0 (sample 8 to 25):" << std::endl;
    std::cout << "Left:    ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irL15[i] * 1000) << " ";
    std::cout << "\nRight:   ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irR15[i] * 1000) << " ";
    std::cout << "\n";

    // Compare with Index 285 (Az=345, El=0)
    float* irL345 = hrtf->DataIR.values + 285 * (hrtf->R * hrtf->N);
    float* irR345 = irL345 + hrtf->N;
    std::cout << "\nRaw IR samples at Az=345, El=0 (sample 8 to 25):" << std::endl;
    std::cout << "Left:    ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irL345[i] * 1000) << " ";
    std::cout << "\nRight:   ";
    for (int i = 8; i <= 25; i++) std::cout << std::setw(6) << (int)(irR345[i] * 1000) << " ";
    std::cout << "\n";

    mysofa_free(hrtf);
    return 0;
}
