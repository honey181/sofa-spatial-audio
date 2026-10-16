#include <iostream>
#include <vector>
#include <algorithm>
#include "mysofa.h"

int main() {
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa";
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load(path, &err);
    if (!hrtf) {
        std::cout << "Failed to load SOFA" << std::endl;
        return 1;
    }

    std::cout << "Measurements (M): " << hrtf->M << std::endl;
    std::cout << "Coordinates type: " << (hrtf->SourcePosition.attributes ? "has attributes" : "none") << std::endl;

    // Convert to spherical if in cartesian
    mysofa_tocartesian(hrtf);

    float minEl = 999.0f, maxEl = -999.0f;
    float minAz = 999.0f, maxAz = -999.0f;
    float minR = 999.0f, maxR = -999.0f;

    for (unsigned int i = 0; i < hrtf->M; i++) {
        float x = hrtf->SourcePosition.values[i * 3 + 0];
        float y = hrtf->SourcePosition.values[i * 3 + 1];
        float z = hrtf->SourcePosition.values[i * 3 + 2];

        float c[3] = {x, y, z};
        mysofa_c2s(c); // spherical: c[0]=azimuth, c[1]=elevation, c[2]=radius

        if (c[1] < minEl) minEl = c[1];
        if (c[1] > maxEl) maxEl = c[1];
        if (c[0] < minAz) minAz = c[0];
        if (c[0] > maxAz) maxAz = c[0];
        if (c[2] < minR) minR = c[2];
        if (c[2] > maxR) maxR = c[2];
    }

    std::cout << "Elevation range: [" << minEl << " deg, " << maxEl << " deg]" << std::endl;
    std::cout << "Azimuth range:   [" << minAz << " deg, " << maxAz << " deg]" << std::endl;
    std::cout << "Radius range:    [" << minR << " m, " << maxR << " m]" << std::endl;

    mysofa_free(hrtf);
    return 0;
}
