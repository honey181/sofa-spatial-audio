#include <iostream>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", &err);
    if (!hrtf) {
        std::cout << "Failed to load SOFA: " << err << std::endl;
        return 1;
    }
    
    std::cout << "ListenerView elements: " << hrtf->ListenerView.elements << ": ";
    for (int i = 0; i < hrtf->ListenerView.elements; i++) {
        std::cout << hrtf->ListenerView.values[i] << " ";
    }
    std::cout << "\n";

    std::cout << "ListenerUp elements: " << hrtf->ListenerUp.elements << ": ";
    for (int i = 0; i < hrtf->ListenerUp.elements; i++) {
        std::cout << hrtf->ListenerUp.values[i] << " ";
    }
    std::cout << "\n";

    std::cout << "ReceiverPosition elements: " << hrtf->ReceiverPosition.elements << ":\n";
    for (int r = 0; r < hrtf->R; r++) {
        std::cout << "  Receiver " << r << ": (";
        for (int c = 0; c < hrtf->C; c++) {
            std::cout << hrtf->ReceiverPosition.values[r * hrtf->C + c] << (c < hrtf->C - 1 ? ", " : "");
        }
        std::cout << ")\n";
    }

    mysofa_free(hrtf);
    return 0;
}
