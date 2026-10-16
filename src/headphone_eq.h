#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>

// Biquad Filter Types (Standard Audio EQ)
enum class BiquadType {
    Peaking,
    LowShelf,
    HighShelf,
    LowPass,
    HighPass
};

struct BiquadFilter {
    BiquadType type = BiquadType::Peaking;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.414f;

    // Filter coefficients
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;

    // State history per channel [0 = Left, 1 = Right]
    float x1[2] = {0}, x2[2] = {0};
    float y1[2] = {0}, y2[2] = {0};

    void CalculateCoefficients(float sampleRate = 48000.0f);
    float ProcessSample(float inSample, int channel);
    void Reset();
};

class HeadphoneEQ {
public:
    HeadphoneEQ();
    ~HeadphoneEQ();

    // 1. Parametric AutoEq Support
    bool LoadAutoEqFile(const std::string& filePath);
    void AddBiquad(BiquadType type, float freq, float gainDb, float q);
    void ClearBiquads();
    bool HasAutoEq() const { return !m_filters.empty(); }
    bool HasBiquads() const { return !m_filters.empty(); }

    // 2. Convolution IR WAV Support
    bool LoadWavImpulseResponse(const std::string& wavPath);
    bool HasConvolutionIR() const { return !m_irLeft.empty(); }
    const std::vector<float>& GetLeftIR() const { return m_irLeft; }
    const std::vector<float>& GetRightIR() const { return m_irRight; }

    // 3. Real-time Stereo Processing
    void ProcessStereo(float* leftChannel, float* rightChannel, uint32_t frameCount);
    void Reset();

    // 4. FIR Convolve helper (for pre-baking into SOFA HRIRs in memory)
    static std::vector<float> ConvolveSignals(const std::vector<float>& signalA, const std::vector<float>& signalB);

private:
    std::vector<BiquadFilter> m_filters;
    std::vector<float> m_irLeft;
    std::vector<float> m_irRight;

    // Convolution state (overlap-save / ring buffer)
    std::vector<float> m_convHistoryLeft;
    std::vector<float> m_convHistoryRight;
    uint32_t m_convPos = 0;
};
