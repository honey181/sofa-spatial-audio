#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>

int main() {
    std::ifstream f("output_3d_test.wav", std::ios::binary);
    if (!f.is_open()) return 1;
    f.seekg(44); // skip WAV header

    // 14 seconds of 16-bit stereo at 48kHz
    std::vector<short> samples(48000 * 2 * 14);
    f.read((char*)samples.data(), samples.size() * sizeof(short));

    auto getRmsSlice = [&](float startSec, float durSec, double& rmsL, double& rmsR) {
        size_t startSample = (size_t)(startSec * 48000.0f);
        size_t numSamples = (size_t)(durSec * 48000.0f);
        double sumL = 0, sumR = 0;
        for (size_t i = 0; i < numSamples; i++) {
            size_t idx = (startSample + i) * 2;
            if (idx + 1 >= samples.size()) break;
            double l = samples[idx + 0] / 32768.0;
            double r = samples[idx + 1] / 32768.0;
            sumL += l * l;
            sumR += r * r;
        }
        rmsL = std::sqrt(sumL / numSamples);
        rmsR = std::sqrt(sumR / numSamples);
    };

    double rL, rR;
    std::cout << "Analysis of output_3d_test.wav across trajectory:" << std::endl;
    // 0.0s - 0.5s: Front
    getRmsSlice(0.0f, 0.5f, rL, rR);
    std::cout << "Front (0s):       RMS L=" << std::fixed << std::setprecision(4) << rL << "  R=" << rR << " (Balance: " << (rL/rR) << ")" << std::endl;

    // 1.5s: Right (Azimuth 90 deg)
    getRmsSlice(1.3f, 0.4f, rL, rR);
    std::cout << "Right (1.5s):     RMS L=" << rL << "  R=" << rR << " (Right is louder? " << (rR > rL ? "YES" : "NO") << ")" << std::endl;

    // 3.0s: Back (Azimuth 180 deg)
    getRmsSlice(2.8f, 0.4f, rL, rR);
    std::cout << "Back (3.0s):      RMS L=" << rL << "  R=" << rR << " (Balance: " << (rL/rR) << ")" << std::endl;

    // 4.5s: Left (Azimuth 270 deg)
    getRmsSlice(4.3f, 0.4f, rL, rR);
    std::cout << "Left (4.5s):      RMS L=" << rL << "  R=" << rR << " (Left is louder? " << (rL > rR ? "YES" : "NO") << ")" << std::endl;

    // 8.0s: Overhead (Elevation 55 deg)
    getRmsSlice(8.0f, 0.5f, rL, rR);
    std::cout << "Overhead (8.0s):  RMS L=" << rL << "  R=" << rR << std::endl;

    return 0;
}
