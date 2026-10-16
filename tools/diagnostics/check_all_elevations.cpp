#include <iostream>
#include <iomanip>
#include <cmath>
#include <map>
#include <vector>
#include "mysofa.h"

int main() {
    int err = 0;
    struct MYSOFA_HRTF* hrtf = mysofa_load("C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa", &err);
    if (!hrtf) return 1;

    // Group measurements by Elevation
    std::map<int, std::vector<int>> elevMap;
    for (int m = 0; m < (int)hrtf->M; m++) {
        float el = hrtf->SourcePosition.values[m * 3 + 1];
        int elInt = (int)std::round(el);
        elevMap[elInt].push_back(m);
    }

    std::cout << "Elevations in default_kemar.sofa:" << std::endl;
    std::cout << "Elev (deg) | Count | Azimuth 0 PeakL (amp) | Azimuth 0 PeakR (amp) | Delay (L, R)" << std::endl;
    for (const auto& pair : elevMap) {
        int el = pair.first;
        // Find Azimuth 0 (or closest)
        int bestM = -1;
        float minAzDiff = 999.0f;
        for (int m : pair.second) {
            float az = hrtf->SourcePosition.values[m * 3 + 0];
            float diff = std::min(std::abs(az), std::abs(az - 360.0f));
            if (diff < minAzDiff) {
                minAzDiff = diff;
                bestM = m;
            }
        }

        if (bestM >= 0) {
            float az = hrtf->SourcePosition.values[bestM * 3 + 0];
            float* irL = hrtf->DataIR.values + bestM * (hrtf->R * hrtf->N);
            float* irR = irL + hrtf->N;

            int peakL = 0, peakR = 0;
            float maxL = 0.0f, maxR = 0.0f;
            for (int i = 0; i < hrtf->N; i++) {
                if (std::abs(irL[i]) > maxL) { maxL = std::abs(irL[i]); peakL = i; }
                if (std::abs(irR[i]) > maxR) { maxR = std::abs(irR[i]); peakR = i; }
            }

            std::cout << std::setw(10) << el << " | "
                      << std::setw(5) << pair.second.size() << " | "
                      << "Az " << std::setw(3) << (int)az << ": PeakL=" << std::setw(2) << peakL << " (" << std::fixed << std::setprecision(2) << maxL << ") | "
                      << "PeakR=" << std::setw(2) << peakR << " (" << std::fixed << std::setprecision(2) << maxR << ") | "
                      << "Delay=(" << hrtf->DataDelay.values[bestM * 2 + 0] << ", " << hrtf->DataDelay.values[bestM * 2 + 1] << ")"
                      << std::endl;
        }
    }

    mysofa_free(hrtf);
    return 0;
}
