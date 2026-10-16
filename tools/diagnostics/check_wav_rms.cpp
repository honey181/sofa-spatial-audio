#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <iomanip>

int main() {
    auto readRms = [](const char* path) {
        std::ifstream f(path, std::ios::binary);
        f.seekg(44); // skip WAV header
        std::vector<short> samples(48000 * 2 * 2);
        f.read((char*)samples.data(), samples.size() * sizeof(short));
        size_t count = f.gcount() / 2;

        double sumL = 0, sumR = 0;
        for (size_t i = 0; i < count / 2; i++) {
            double l = samples[i * 2 + 0] / 32768.0;
            double r = samples[i * 2 + 1] / 32768.0;
            sumL += l * l;
            sumR += r * r;
        }
        double rmsL = std::sqrt(sumL / (count / 2));
        double rmsR = std::sqrt(sumR / (count / 2));
        std::cout << path << " -> RMS Left: " << std::fixed << std::setprecision(4) << rmsL 
                  << " | RMS Right: " << rmsR << std::endl;
    };

    readRms("kemar_front.wav");
    readRms("kemar_back.wav");
    readRms("kemar_up.wav");
    readRms("kemar_down.wav");
    readRms("indiv_front.wav");
    readRms("indiv_back.wav");
    readRms("indiv_up.wav");
    readRms("indiv_down.wav");
    return 0;
}
