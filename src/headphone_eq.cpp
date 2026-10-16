#include "headphone_eq.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void BiquadFilter::CalculateCoefficients(float sampleRate) {
    float w0 = 2.0f * (float)M_PI * frequency / sampleRate;
    float alpha = std::sin(w0) / (2.0f * q);
    float A = std::pow(10.0f, gainDb / 40.0f); // sqrt of 10^(gainDb/20)

    float a0 = 1.0f;

    switch (type) {
        case BiquadType::Peaking: {
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * std::cos(w0);
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * std::cos(w0);
            a2 = 1.0f - alpha / A;
            break;
        }
        case BiquadType::LowShelf: {
            float sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0f) - (A - 1.0f) * std::cos(w0) + 2.0f * sqrtA * alpha);
            b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * std::cos(w0));
            b2 = A * ((A + 1.0f) - (A - 1.0f) * std::cos(w0) - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) + (A - 1.0f) * std::cos(w0) + 2.0f * sqrtA * alpha;
            a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * std::cos(w0));
            a2 = (A + 1.0f) + (A - 1.0f) * std::cos(w0) - 2.0f * sqrtA * alpha;
            break;
        }
        case BiquadType::HighShelf: {
            float sqrtA = std::sqrt(A);
            b0 = A * ((A + 1.0f) + (A - 1.0f) * std::cos(w0) + 2.0f * sqrtA * alpha);
            b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * std::cos(w0));
            b2 = A * ((A + 1.0f) + (A - 1.0f) * std::cos(w0) - 2.0f * sqrtA * alpha);
            a0 = (A + 1.0f) - (A - 1.0f) * std::cos(w0) + 2.0f * sqrtA * alpha;
            a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * std::cos(w0));
            a2 = (A + 1.0f) - (A - 1.0f) * std::cos(w0) - 2.0f * sqrtA * alpha;
            break;
        }
        default:
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a0 = 1.0f; a1 = 0.0f; a2 = 0.0f;
            break;
    }

    // Normalize by a0
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

float BiquadFilter::ProcessSample(float inSample, int channel) {
    int ch = channel & 1;
    float outSample = b0 * inSample + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];

    x2[ch] = x1[ch];
    x1[ch] = inSample;
    y2[ch] = y1[ch];
    y1[ch] = outSample;

    return outSample;
}

void BiquadFilter::Reset() {
    for (int i = 0; i < 2; i++) {
        x1[i] = x2[i] = 0.0f;
        y1[i] = y2[i] = 0.0f;
    }
}

HeadphoneEQ::HeadphoneEQ() {}
HeadphoneEQ::~HeadphoneEQ() {}

void HeadphoneEQ::AddBiquad(BiquadType type, float freq, float gainDb, float q) {
    BiquadFilter f;
    f.type = type;
    f.frequency = freq;
    f.gainDb = gainDb;
    f.q = q;
    f.CalculateCoefficients(48000.0f);
    m_filters.push_back(f);
}

void HeadphoneEQ::ClearBiquads() {
    m_filters.clear();
}

bool HeadphoneEQ::LoadAutoEqFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    ClearBiquads();
    std::string line;
    // Parse lines like: "Filter 1: ON PK Fc 105 Hz Gain -3.2 dB Q 1.41"
    while (std::getline(file, line)) {
        if (line.find("Filter") == std::string::npos || line.find("ON") == std::string::npos) continue;

        std::istringstream iss(line);
        std::string token;
        BiquadType type = BiquadType::Peaking;
        float freq = 1000.0f, gain = 0.0f, q = 1.414f;

        while (iss >> token) {
            if (token == "PK") type = BiquadType::Peaking;
            else if (token == "LSC") type = BiquadType::LowShelf;
            else if (token == "HSC") type = BiquadType::HighShelf;
            else if (token == "Fc") iss >> freq;
            else if (token == "Gain") iss >> gain;
            else if (token == "Q") iss >> q;
        }

        AddBiquad(type, freq, gain, q);
    }
    return !m_filters.empty();
}

// WAV Header parsing for Impulse Response WAVs
#pragma pack(push, 1)
struct WavHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
#pragma pack(pop)

bool HeadphoneEQ::LoadWavImpulseResponse(const std::string& wavPath) {
    std::ifstream file(wavPath, std::ios::binary);
    if (!file.is_open()) return false;

    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));

    if (std::string(header.riff, 4) != "RIFF" || std::string(header.wave, 4) != "WAVE") {
        return false;
    }

    // Find the 'data' chunk
    char chunkId[4];
    uint32_t chunkSize = 0;
    while (file.read(chunkId, 4)) {
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (std::string(chunkId, 4) == "data") break;
        file.seekg(chunkSize, std::ios::cur);
    }

    if (chunkSize == 0) return false;

    uint32_t numSamples = chunkSize / (header.numChannels * (header.bitsPerSample / 8));
    m_irLeft.resize(numSamples);
    m_irRight.resize(numSamples);

    if (header.bitsPerSample == 16) {
        std::vector<int16_t> buffer(numSamples * header.numChannels);
        file.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
        for (uint32_t i = 0; i < numSamples; i++) {
            m_irLeft[i] = buffer[i * header.numChannels] / 32768.0f;
            m_irRight[i] = (header.numChannels > 1) ? (buffer[i * header.numChannels + 1] / 32768.0f) : m_irLeft[i];
        }
    } else if (header.bitsPerSample == 32 && header.audioFormat == 3) { // IEEE Float
        std::vector<float> buffer(numSamples * header.numChannels);
        file.read(reinterpret_cast<char*>(buffer.data()), chunkSize);
        for (uint32_t i = 0; i < numSamples; i++) {
            m_irLeft[i] = buffer[i * header.numChannels];
            m_irRight[i] = (header.numChannels > 1) ? buffer[i * header.numChannels + 1] : m_irLeft[i];
        }
    } else {
        return false; // Unsupported bit depth
    }

    // Allocate convolution history
    m_convHistoryLeft.assign(m_irLeft.size(), 0.0f);
    m_convHistoryRight.assign(m_irRight.size(), 0.0f);
    m_convPos = 0;

    return true;
}

void HeadphoneEQ::ProcessStereo(float* leftChannel, float* rightChannel, uint32_t frameCount) {
    // 1. Apply Parametric Biquads
    for (auto& f : m_filters) {
        for (uint32_t i = 0; i < frameCount; i++) {
            leftChannel[i] = f.ProcessSample(leftChannel[i], 0);
            rightChannel[i] = f.ProcessSample(rightChannel[i], 1);
        }
    }

    // 2. Apply Direct Convolution IR (if loaded)
    if (HasConvolutionIR()) {
        size_t irLen = m_irLeft.size();
        for (uint32_t i = 0; i < frameCount; i++) {
            m_convHistoryLeft[m_convPos] = leftChannel[i];
            m_convHistoryRight[m_convPos] = rightChannel[i];

            float sumL = 0.0f, sumR = 0.0f;
            size_t idx = m_convPos;
            for (size_t j = 0; j < irLen; j++) {
                sumL += m_convHistoryLeft[idx] * m_irLeft[j];
                sumR += m_convHistoryRight[idx] * m_irRight[j];
                if (idx == 0) idx = irLen - 1;
                else idx--;
            }

            leftChannel[i] = sumL;
            rightChannel[i] = sumR;

            m_convPos = (m_convPos + 1) % irLen;
        }
    }
}

void HeadphoneEQ::Reset() {
    for (auto& f : m_filters) {
        f.Reset();
    }
    std::fill(m_convHistoryLeft.begin(), m_convHistoryLeft.end(), 0.0f);
    std::fill(m_convHistoryRight.begin(), m_convHistoryRight.end(), 0.0f);
    m_convPos = 0;
}

// Mathematical convolution of two signals: out[n] = sum(A[k] * B[n-k])
std::vector<float> HeadphoneEQ::ConvolveSignals(const std::vector<float>& A, const std::vector<float>& B) {
    if (A.empty() || B.empty()) return {};
    size_t outLen = A.size() + B.size() - 1;
    std::vector<float> out(outLen, 0.0f);

    for (size_t i = 0; i < A.size(); i++) {
        for (size_t j = 0; j < B.size(); j++) {
            out[i + j] += A[i] * B[j];
        }
    }
    return out;
}
