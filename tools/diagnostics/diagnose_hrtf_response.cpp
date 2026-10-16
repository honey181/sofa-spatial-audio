#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include "mysofa.h"

void Analyze(const char* name, const char* path) {
    std::cout << "\n=======================================================\n";
    std::cout << "Testing Dataset: " << name << "\n";
    std::cout << "Path: " << path << "\n";
    std::cout << "=======================================================\n";

    int filterLen = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open(path, 48000.0f, &filterLen, &err);
    if (!easy) {
        std::cout << "Failed to open " << path << " (err=" << err << ")\n";
        return;
    }

    std::cout << "mysofa_open OK: filterLen = " << filterLen 
              << ", Source positions M = " << easy->hrtf->M 
              << ", Coordinates C = " << easy->hrtf->C << "\n";

    // Print coordinate bounds
    std::cout << "Radius bounds: [" << easy->lookup->radius_min << ", " << easy->lookup->radius_max << "]\n";
    std::cout << "Phi (Azimuth) bounds: [" << easy->lookup->phi_min << ", " << easy->lookup->phi_max << "]\n";
    std::cout << "Theta (Elevation) bounds: [" << easy->lookup->theta_min << ", " << easy->lookup->theta_max << "]\n";

    struct TestPoint {
        const char* label;
        float x_user; // Right
        float y_user; // Up
        float z_user; // Front
    };

    std::vector<TestPoint> points = {
        {"FRONT (Z=1)", 0.0f, 0.0f, 1.0f},
        {"BACK  (Z=-1)", 0.0f, 0.0f, -1.0f},
        {"LEFT  (X=-1)", -1.0f, 0.0f, 0.0f},
        {"RIGHT (X=1)", 1.0f, 0.0f, 0.0f},
        {"UP    (Y=+1, elev=90)", 0.0f, 1.0f, 0.0f},
        {"UP-FRONT (elev=+45)", 0.0f, 1.0f, 1.0f},
        {"DOWN-FRONT (elev=-45)", 0.0f, -1.0f, 1.0f},
        {"DOWN  (Y=-1, elev=-90)", 0.0f, -1.0f, 0.0f},
        {"HEIGHT +0.1", 0.0f, 0.1f, 1.0f},
        {"HEIGHT +0.01", 0.0f, 0.01f, 1.0f},
        {"HEIGHT -0.01", 0.0f, -0.01f, 1.0f},
        {"HEIGHT -0.1", 0.0f, -0.1f, 1.0f},
    };

    std::vector<float> irL(filterLen);
    std::vector<float> irR(filterLen);

    for (const auto& pt : points) {
        // According to our mapping:
        // x_sofa = z_user (Front is +X in SOFA)
        // y_sofa = -x_user (Left is +Y in SOFA)
        // z_sofa = y_user (Up is +Z in SOFA)
        float xs = pt.z_user;
        float ys = -pt.x_user;
        float zs = pt.y_user;

        float delayL = 0.0f, delayR = 0.0f;
        mysofa_getfilter_float(easy, xs, ys, zs, irL.data(), irR.data(), &delayL, &delayR);

        // Find peaks, energy, and ITD
        float sumL = 0, sumR = 0;
        int peakIdxL = 0, peakIdxR = 0;
        float peakValL = 0, peakValR = 0;
        for (int i = 0; i < filterLen; i++) {
            sumL += irL[i] * irL[i];
            sumR += irR[i] * irR[i];
            if (std::abs(irL[i]) > peakValL) { peakValL = std::abs(irL[i]); peakIdxL = i; }
            if (std::abs(irR[i]) > peakValR) { peakValR = std::abs(irR[i]); peakIdxR = i; }
        }
        float rmsL = std::sqrt(sumL / filterLen);
        float rmsR = std::sqrt(sumR / filterLen);

        std::cout << std::left << std::setw(25) << pt.label
                  << " | Peak L: " << std::setw(3) << peakIdxL << " (" << std::fixed << std::setprecision(3) << peakValL << ")"
                  << " | Peak R: " << std::setw(3) << peakIdxR << " (" << std::setw(5) << peakValR << ")"
                  << " | RMS L/R: " << rmsL << " / " << rmsR
                  << " | Delays: " << delayL << " / " << delayR << "\n";
    }

    mysofa_close(easy);
}

int main() {
    Analyze("MIT KEMAR Reference", "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa");
    Analyze("Individualized SOFA", "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa");
    return 0;
}
