#include <iostream>
#include <vector>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) return 1;

    std::cout << "DataDelay elements: " << hrtf->DataDelay.elements << std::endl;
    for (int i = 0; i < std::min((int)hrtf->DataDelay.elements, 20); i++) {
        std::cout << hrtf->DataDelay.values[i] << " ";
    }
    std::cout << std::endl;

    mysofa_free(hrtf);
    return 0;
}
