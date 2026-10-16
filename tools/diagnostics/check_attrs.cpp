#include <iostream>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", &err);
    if (!hrtf) {
        std::cout << "custom err: " << err << std::endl;
        return 1;
    }

    std::cout << "custom DataDelay elements: " << hrtf->DataDelay.elements << std::endl;
    std::cout << "custom DataDelay values[0..9]: ";
    for (int i = 0; i < std::min((int)hrtf->DataDelay.elements, 10); i++) {
        std::cout << hrtf->DataDelay.values[i] << " ";
    }
    std::cout << std::endl;

    struct MYSOFA_HRTF* kemar = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", &err);
    if (!kemar) {
        std::cout << "kemar err: " << err << std::endl;
    } else {
        std::cout << "kemar DataDelay elements: " << kemar->DataDelay.elements << std::endl;
        std::cout << "kemar DataDelay values[0..9]: ";
        for (int i = 0; i < std::min((int)kemar->DataDelay.elements, 10); i++) {
            std::cout << kemar->DataDelay.values[i] << " ";
        }
        std::cout << std::endl;
        mysofa_free(kemar);
    }
    mysofa_free(hrtf);
    return 0;
}
