#include <iostream>
#include "mysofa.h"

int main() {
    std::cout << "Testing mysofa_open on default_kemar.sofa..." << std::endl;
    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", 48000.0f, &filterLen, &err);
    std::cout << "easy=" << easy << " err=" << err << " filterLen=" << filterLen << std::endl;
    if (easy) {
        mysofa_close(easy);
    }
    return 0;
}
