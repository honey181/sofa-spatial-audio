#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <complex>
#include "mysofa.h"

// Simple DFT for spectral magnitude in dB
std::vector<float> ComputeMagnitudeDB(const float* ir, int N, int numBins) {
    std::vector<float> dbs(numBins);
    for (int k = 0; k < numBins; k++) {
        float omega = (float)M_PI * (float)k / (float)numBins;
        std::complex<float> sum(0.0f, 0.0f);
        for (int n = 0; n < N; n++) {
            float angle = omega * n;
            sum += ir[n] * std::complex<float>(std::cos(angle), -std::sin(angle));
        }
        float mag = std::abs(sum);
        dbs[k] = 20.0f * std::log10(std::max(mag, 1e-6f));
    }
    return dbs;
}

int main() {
    int len = 0, err = 0;
    struct MYSOFA_EASY* easy = mysofa_open("C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa", 48000.0f, &len, &err);
    if (!easy) return 1;

    std::vector<float> frontL(len), frontR(len);
    std::vector<float> backL(len), backR(len);
    std::vector<float> upL(len), upR(len);
    std::vector<float> downL(len), downR(len);

    float dL, dR;
    mysofa_getfilter_float(easy, 1.0f, 0.0f, 0.0f, frontL.data(), frontR.data(), &dL, &dR);
    mysofa_getfilter_float(easy, -1.0f, 0.0f, 0.0f, backL.data(), backR.data(), &dL, &dR);
    mysofa_getfilter_float(easy, 0.0f, 0.0f, 1.0f, upL.data(), upR.data(), &dL, &dR);
    mysofa_getfilter_float(easy, 0.0f, 0.0f, -0.6f, downL.data(), downR.data(), &dL, &dR);

    int numBins = 240; // up to 24 kHz in 100 Hz steps
    auto specFront = ComputeMagnitudeDB(frontL.data(), len, numBins);
    auto specBack = ComputeMagnitudeDB(backL.data(), len, numBins);
    auto specUp = ComputeMagnitudeDB(upL.data(), len, numBins);
    auto specDown = ComputeMagnitudeDB(downL.data(), len, numBins);

    std::cout << "Frequency (Hz) | FRONT (dB) | BACK (dB) | UP (dB) | DOWN (dB)" << std::endl;
    int freqs[] = {500, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 12000, 14000, 16000};
    for (int f : freqs) {
        int bin = f / 100;
        std::cout << std::setw(14) << f << " | "
                  << std::fixed << std::setprecision(1)
                  << std::setw(10) << specFront[bin] << " | "
                  << std::setw(9) << specBack[bin] << " | "
                  << std::setw(7) << specUp[bin] << " | "
                  << std::setw(9) << specDown[bin] << std::endl;
    }

    mysofa_close(easy);
    return 0;
}
