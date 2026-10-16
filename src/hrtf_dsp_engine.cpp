#include "hrtf_dsp_engine.h"
#include "spatial_logger.h"
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>

CustomHrtfDspEngine::CustomHrtfDspEngine(uint32_t maxObjects) 
    : m_maxObjects(maxObjects ? maxObjects : 128) {
    m_objectStates.resize(m_maxObjects);
    for (auto& st : m_objectStates) {
        st.history.assign(512, 0.0f);
        st.histPos = 0;
    }
    m_tempLeft.resize(1024, 0.0f);
    m_tempRight.resize(1024, 0.0f);

    LoadConfiguration();
}

CustomHrtfDspEngine::~CustomHrtfDspEngine() {}

void CustomHrtfDspEngine::LoadConfiguration(const std::string& configPath) {
    std::string path = configPath;
    if (path.empty()) {
        if (std::ifstream("C:\\ProgramData\\SofaSpatialAudio\\config.ini").good()) {
            path = "C:\\ProgramData\\SofaSpatialAudio\\config.ini";
        } else if (std::ifstream("config/config.ini").good()) {
            path = "config/config.ini";
        }
    }

    if (path.empty() || !std::ifstream(path).good()) {
        SpatialLogger::Get().Log("[CONFIG] No config.ini found, using baseline spherical HRTF.");
        return;
    }

    SpatialLogger::Get().LogFormat("[CONFIG] Reading settings from '%s'", path.c_str());
    std::ifstream file(path);
    std::string line;
    std::string sofaPath;
    std::string wavPath;
    std::string autoeqPath;

    while (std::getline(file, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';') continue;
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = line.substr(first, eqPos - first);
        key.erase(key.find_last_not_of(" \t") + 1);
        std::string val = line.substr(eqPos + 1);
        size_t valFirst = val.find_first_not_of(" \t\r\n");
        if (valFirst != std::string::npos) {
            val = val.substr(valFirst);
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
        } else {
            val.clear();
        }

        if (key == "sofa_file") sofaPath = val;
        else if (key == "ir_wav_file") wavPath = val;
        else if (key == "autoeq_txt_file") autoeqPath = val;
    }

    // 1. Load Headphone EQ first if configured
    if (!wavPath.empty()) {
        LoadHeadphoneWav(wavPath);
    } else if (!autoeqPath.empty()) {
        LoadAutoEq(autoeqPath);
    }

    // 2. Load SOFA file
    if (!sofaPath.empty()) {
        LoadSofa(sofaPath);
    }
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CustomHrtfDspEngine::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE CustomHrtfDspEngine::Release() {
    ULONG count = --m_refCount;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::Initialize(const void* pConfig, uint32_t flags) {
    std::lock_guard<std::mutex> lock(m_dspMutex);
    Reset();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::Reset() {
    for (auto& st : m_objectStates) {
        std::fill(st.history.begin(), st.history.end(), 0.0f);
        st.histPos = 0;
    }
    m_headphoneEq.Reset();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::SetDirectivity(uint32_t objectIndex, const void* pParams) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::GetMinFrameCount(uint32_t* pMinFrames) {
    if (!pMinFrames) return E_POINTER;
    *pMinFrames = 64; // Low-latency 64-sample period (1.33ms)
    return S_OK;
}

bool CustomHrtfDspEngine::LoadSofa(const std::string& sofaPath) {
    std::lock_guard<std::mutex> lock(m_dspMutex);
    bool ok = m_sofaDb.LoadSofaFile(sofaPath);
    if (ok && m_headphoneEq.HasConvolutionIR()) {
        // Pre-bake headphone EQ directly into the SOFA database in RAM
        m_sofaDb.PrebakeHeadphoneEQ(m_headphoneEq.GetLeftIR(), m_headphoneEq.GetRightIR());
    }
    return ok;
}

bool CustomHrtfDspEngine::LoadHeadphoneWav(const std::string& wavPath) {
    std::lock_guard<std::mutex> lock(m_dspMutex);
    bool ok = m_headphoneEq.LoadWavImpulseResponse(wavPath);
    if (ok && m_sofaDb.IsLoaded()) {
        m_sofaDb.PrebakeHeadphoneEQ(m_headphoneEq.GetLeftIR(), m_headphoneEq.GetRightIR());
    }
    return ok;
}

bool CustomHrtfDspEngine::LoadAutoEq(const std::string& txtPath) {
    std::lock_guard<std::mutex> lock(m_dspMutex);
    return m_headphoneEq.LoadAutoEqFile(txtPath);
}

// Real-Time Frame Processor
// Called continuously by HrtfApo.dll inside audiodg.exe
HRESULT STDMETHODCALLTYPE CustomHrtfDspEngine::Process(
    const HrtfSpatialObjectData* pObjects,
    float* pPcmBuffer,
    uint32_t frameCount,
    uint32_t flags
) {
    if (!pPcmBuffer || frameCount == 0) return S_OK;

    std::lock_guard<std::mutex> lock(m_dspMutex);

    if (m_tempLeft.size() < frameCount) {
        m_tempLeft.resize(frameCount);
        m_tempRight.resize(frameCount);
    }

    std::fill(m_tempLeft.begin(), m_tempLeft.begin() + frameCount, 0.0f);
    std::fill(m_tempRight.begin(), m_tempRight.begin() + frameCount, 0.0f);

    uint32_t hrirLen = m_sofaDb.GetHrirLength();
    uint32_t activeCount = 0;
    float primaryX = 0, primaryY = 0, primaryZ = 0, primaryGain = 0;

    // Iterate through dynamic objects
    for (uint32_t objIdx = 0; objIdx < m_maxObjects; objIdx++) {
        const auto& obj = pObjects[objIdx];
        
        // Skip disabled/silent objects
        if (obj.gain <= 1e-5f && (obj.flags == 0)) continue;

        if (activeCount == 0) {
            primaryX = obj.posX;
            primaryY = obj.posY;
            primaryZ = obj.posZ;
            primaryGain = (obj.gain > 0.0f) ? obj.gain : 1.0f;
        }
        activeCount++;

        // Lookup HRIR filter pair based on 3D Cartesian coordinates (posX, posY, posZ)
        const std::vector<float>* pHrirL = nullptr;
        const std::vector<float>* pHrirR = nullptr;
        m_sofaDb.LookupHrir(obj.posX, obj.posY, obj.posZ, &pHrirL, &pHrirR);

        if (!pHrirL || !pHrirR) continue;

        auto& state = m_objectStates[objIdx];
        if (state.history.size() < hrirLen) {
            state.history.resize(hrirLen, 0.0f);
        }

        // Pointer to this object's mono input PCM buffer
        const float* pAudio = *(const float**)((const char*)&obj + 0x68);
        const float* objInput = (pAudio != nullptr) ? pAudio : (pPcmBuffer + (objIdx * frameCount));
        float objGain = (obj.gain > 0.0f) ? obj.gain : 1.0f;

        // Convolve object audio with directional HRIRs
        for (uint32_t f = 0; f < frameCount; f++) {
            state.history[state.histPos] = objInput[f] * objGain;

            float sampleL = 0.0f;
            float sampleR = 0.0f;
            size_t idx = state.histPos;

            for (uint32_t tap = 0; tap < hrirLen; tap++) {
                float histVal = state.history[idx];
                sampleL += histVal * (*pHrirL)[tap];
                sampleR += histVal * (*pHrirR)[tap];

                if (idx == 0) idx = hrirLen - 1;
                else idx--;
            }

            m_tempLeft[f] += sampleL;
            m_tempRight[f] += sampleR;

            state.histPos = (state.histPos + 1) % hrirLen;
        }
    }

    if (activeCount > 0) {
        SpatialLogger::Get().LogObjectActivity(activeCount, primaryX, primaryY, primaryZ, primaryGain);
    }

    // Apply Headphone EQ Correction (Parametric biquads or post-mix FIR)
    m_headphoneEq.ProcessStereo(m_tempLeft.data(), m_tempRight.data(), frameCount);

    // Interleave Left & Right into final output buffer for DAC
    for (uint32_t f = 0; f < frameCount; f++) {
        pPcmBuffer[f * 2 + 0] = m_tempLeft[f];
        pPcmBuffer[f * 2 + 1] = m_tempRight[f];
    }

    return S_OK;
}

// CustomHrtfDspFactory Implementation
CustomHrtfDspFactory::CustomHrtfDspFactory() {}
CustomHrtfDspFactory::~CustomHrtfDspFactory() {}

HRESULT STDMETHODCALLTYPE CustomHrtfDspFactory::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IHrtfDspFactory) {
        *ppvObject = static_cast<IHrtfDspFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CustomHrtfDspFactory::AddRef() {
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE CustomHrtfDspFactory::Release() {
    ULONG count = --m_refCount;
    if (count == 0) {
        delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE CustomHrtfDspFactory::CreateDspEngine(
    const void* pConfig,
    uint32_t flags,
    uint32_t maxObjects,
    IHrtfDspEngine** ppEngine
) {
    if (!ppEngine) return E_POINTER;
    SpatialLogger::Get().LogFormat("[AUDIO ENGINE] Windows Sonic created CustomHrtfDspEngine (maxObjects=%u)", maxObjects);
    auto* pCustomEngine = new CustomHrtfDspEngine(maxObjects);
    *ppEngine = pCustomEngine;
    return S_OK;
}
