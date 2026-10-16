#include <iostream>
#include <vector>
#include <set>
#include <cmath>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) {
        std::cout << "Failed to open " << path << std::endl;
        return 1;
    }

    std::cout << "KEMAR Total positions (M): " << hrtf->M << std::endl;

    std::set<int> uniqueAzimuths;
    std::set<int> uniqueElevations;

    for (unsigned int i = 0; i < hrtf->M; i++) {
        float az = hrtf->SourcePosition.values[i * 3 + 0];
        float el = hrtf->SourcePosition.values[i * 3 + 1];
        uniqueAzimuths.insert((int)std::round(az));
        uniqueElevations.insert((int)std::round(el));
    }

    std::cout << "Unique Elevations (" << uniqueElevations.size() << "): ";
    for (int el : uniqueElevations) std::cout << el << " ";
    std::cout << std::endl;

    mysofa_free(hrtf);
    return 0;
}
