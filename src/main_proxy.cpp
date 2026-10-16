#include <windows.h>
#include "hrtf_dsp_interfaces.h"
#include "headphone_eq.h"
#include "spatial_logger.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <cstring>

static HMODULE g_hOrigDll = nullptr;

// Type definitions for original DLL exports
typedef HRESULT(__stdcall* PFN_CreateHrtfDspFactory)(UINT32 datasetType, IHrtfDspFactory** ppFactory);
typedef HRESULT(__stdcall* PFN_CreateHrtfDataHelper)(void** ppHelper);
typedef HRESULT(__stdcall* PFN_DllGetClassObject)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRESULT(__stdcall* PFN_CreateDspEngine)(void* pThis, const void* pConfig, uint32_t flags, uint32_t maxObjects, void** ppEngine);
typedef HRESULT(__stdcall* PFN_Process)(void* pThis, const HrtfSpatialObjectData* pObjects, float* pPcmBuffer, uint32_t frameCount, uint32_t flags);

static PFN_CreateHrtfDspFactory g_pfnOrigCreateFactory = nullptr;
static PFN_CreateHrtfDataHelper g_pfnOrigCreateHelper = nullptr;
static PFN_DllGetClassObject g_pfnOrigGetClassObject = nullptr;
static PFN_CreateDspEngine g_pfnOrigCreateDspEngine = nullptr;
static PFN_Process g_pfnOrigProcess = nullptr;

#include "sofa_loader.h"

static uint32_t g_maxObjects = 144;
static HeadphoneEQ g_headphoneEq;
static bool g_eqLoaded = false;
static SofaDatabase g_sofaDb;
static bool g_sofaLoaded = false;
static std::vector<float> g_tempL;
static std::vector<float> g_tempR;
static std::mutex g_dspMutex;

void LoadConfigForEq() {
    if (g_eqLoaded) return;
    g_eqLoaded = true;

    std::string path = "C:\\ProgramData\\SofaSpatialAudio\\config.ini";
    std::ifstream file(path);
    if (!file.is_open()) return;

    SpatialLogger::Get().LogFormat("[CONFIG] Reading settings from '%s'", path.c_str());
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

    // Load SOFA dataset into RAM
    if (!sofaPath.empty()) {
        if (g_sofaDb.LoadSofaFile(sofaPath)) {
            g_sofaLoaded = true;
            SpatialLogger::Get().LogFormat("[SOFA] System proxy loaded SOFA dataset: '%s' (%zu positions, %u taps)", 
                sofaPath.c_str(), g_sofaDb.GetMeasurementCount(), g_sofaDb.GetHrirLength());
        }
    }

    if (!wavPath.empty()) {
        if (g_headphoneEq.LoadWavImpulseResponse(wavPath)) {
            SpatialLogger::Get().LogFormat("[EQ] Loaded headphone convolution IR from '%s'", wavPath.c_str());
        }
    } else if (!autoeqPath.empty()) {
        if (g_headphoneEq.LoadAutoEqFile(autoeqPath)) {
            SpatialLogger::Get().LogFormat("[EQ] Loaded AutoEq parametric profile from '%s'", autoeqPath.c_str());
        }
    }
}

void LoadOriginalDll() {
    if (g_hOrigDll) return;

    // Load original renamed DLL from System32
    g_hOrigDll = LoadLibraryW(L"HrtfDspCpu_orig.dll");
    if (!g_hOrigDll) {
        g_hOrigDll = LoadLibraryW(L"C:\\Windows\\System32\\HrtfDspCpu_orig.dll");
    }

    if (g_hOrigDll) {
        g_pfnOrigCreateFactory = (PFN_CreateHrtfDspFactory)GetProcAddress(g_hOrigDll, "CreateHrtfDspFactory");
        g_pfnOrigCreateHelper = (PFN_CreateHrtfDataHelper)GetProcAddress(g_hOrigDll, "CreateHrtfDataHelper");
        g_pfnOrigGetClassObject = (PFN_DllGetClassObject)GetProcAddress(g_hOrigDll, "DllGetClassObject");
        SpatialLogger::Get().Log("[PROXY] Successfully chained original HrtfDspCpu_orig.dll");
    } else {
        SpatialLogger::Get().Log("[PROXY ERROR] Failed to load HrtfDspCpu_orig.dll!");
    }
}

struct ChannelConvolutionState {
    static const size_t RING_SIZE = 2048;
    static const size_t RING_MASK = RING_SIZE - 1;
    std::vector<float> history;
    size_t histPos = 0;
    std::vector<float> lastHrirL;
    std::vector<float> lastHrirR;
    bool hasLast = false;

    ChannelConvolutionState() {
        history.assign(RING_SIZE, 0.0f);
    }

    void Reset() {
        std::fill(history.begin(), history.end(), 0.0f);
        histPos = 0;
        hasLast = false;
    }
};

static std::vector<ChannelConvolutionState> g_objStates;

// Hooked Process Method (vtable[5] of IHrtfDspEngine)
HRESULT STDMETHODCALLTYPE HookedProcess(
    void* pThis,
    const HrtfSpatialObjectData* pObjects,
    float* pPcmBuffer,
    uint32_t frameCount,
    uint32_t flags
) {
    if (!pPcmBuffer || frameCount == 0) return S_OK;

    // Check if we have active spatial objects with valid input audio buffers
    uint32_t activeWithAudio = 0;
    uint32_t totalActive = 0;
    float primaryX = 0.0f, primaryY = 0.0f, primaryZ = 0.0f, primaryGain = 0.0f;
    const float* pPrimaryAudio = nullptr;
    uint32_t primaryAudioCount = 0;

    if (pObjects) {
        for (uint32_t i = 0; i < g_maxObjects; i++) {
            const char* pObj = ((const char*)pObjects) + i * 0x78;
            float gain = *(const float*)(pObj + 0x0C);
            uint32_t objFlags = *(const uint32_t*)(pObj + 0x60);

            if (gain > 1e-4f || objFlags != 0) {
                const float* pAudio = *(const float**)(pObj + 0x68);
                uint32_t count = *(const uint32_t*)(pObj + 0x70);

                if (totalActive == 0) {
                    primaryX = *(const float*)(pObj + 0x00);
                    primaryY = *(const float*)(pObj + 0x04);
                    primaryZ = *(const float*)(pObj + 0x08);
                    primaryGain = (gain > 0.0f) ? gain : 1.0f;
                    pPrimaryAudio = pAudio;
                    primaryAudioCount = count;
                }
                totalActive++;

                if (pAudio != nullptr && count >= frameCount) {
                    activeWithAudio++;
                }
            }
        }
    }

    if (totalActive > 0) {
        SpatialLogger::Get().LogObjectActivity(totalActive, primaryX, primaryY, primaryZ, primaryGain);
        if (pPrimaryAudio != nullptr) {
            static bool s_loggedAudioPtr = false;
            if (!s_loggedAudioPtr) {
                s_loggedAudioPtr = true;
                SpatialLogger::Get().LogFormat("[STREAM] Object 0 input buffer at %p (%u samples, activeWithAudio=%u)",
                    pPrimaryAudio, primaryAudioCount, activeWithAudio);
            }
        }
    }

    // If SOFA is loaded and spatial objects supply valid audio buffers, render directly with SOFA engine!
    if (g_sofaLoaded && activeWithAudio > 0) {
        std::lock_guard<std::mutex> lock(g_dspMutex);

        if (g_objStates.size() < g_maxObjects) {
            g_objStates.resize(g_maxObjects);
        }
        if (g_tempL.size() < frameCount) {
            g_tempL.resize(frameCount);
            g_tempR.resize(frameCount);
        }

        std::fill(g_tempL.begin(), g_tempL.begin() + frameCount, 0.0f);
        std::fill(g_tempR.begin(), g_tempR.begin() + frameCount, 0.0f);

        uint32_t hrirLen = g_sofaDb.GetHrirLength();
        if (hrirLen == 0) hrirLen = 256;
        if (hrirLen > 512) hrirLen = 512;

        for (uint32_t i = 0; i < g_maxObjects; i++) {
            const char* pObj = ((const char*)pObjects) + i * 0x78;
            float gain = *(const float*)(pObj + 0x0C);
            uint32_t objFlags = *(const uint32_t*)(pObj + 0x60);
            if (gain <= 1e-4f && objFlags == 0) continue;

            const float* pAudio = *(const float**)(pObj + 0x68);
            uint32_t count = *(const uint32_t*)(pObj + 0x70);
            if (pAudio == nullptr || count < frameCount) continue;

            float posX = *(const float*)(pObj + 0x00);
            float posY = *(const float*)(pObj + 0x04);
            float posZ = *(const float*)(pObj + 0x08);
            float objGain = (gain > 0.0f) ? gain : 1.0f;

            // Lookup HRIR for 3D coordinates (CartesianToSpherical in sofa_loader maps -posZ to Front)
            const std::vector<float>* pHrirL = nullptr;
            const std::vector<float>* pHrirR = nullptr;
            g_sofaDb.LookupHrir(posX, posY, posZ, &pHrirL, &pHrirR);
            if (!pHrirL || !pHrirR) continue;

            auto& state = g_objStates[i];
            const float* curL = pHrirL->data();
            const float* curR = pHrirR->data();
            if (!state.hasLast) {
                state.lastHrirL = *pHrirL;
                state.lastHrirR = *pHrirR;
                state.hasLast = true;
            }
            const float* prevL = state.lastHrirL.data();
            const float* prevR = state.lastHrirR.data();
            uint32_t prevLen = (uint32_t)state.lastHrirL.size();
            uint32_t curLen = (uint32_t)pHrirL->size();
            uint32_t effLen = std::min(std::min(curLen, prevLen), hrirLen);

            // Real-time FIR Convolution with block crossfading
            for (uint32_t f = 0; f < frameCount; f++) {
                state.history[state.histPos] = pAudio[f] * objGain;

                float sampleL_cur = 0.0f, sampleR_cur = 0.0f;
                float sampleL_prev = 0.0f, sampleR_prev = 0.0f;

                for (uint32_t tap = 0; tap < effLen; tap++) {
                    size_t readIdx = (state.histPos - tap) & ChannelConvolutionState::RING_MASK;
                    float sVal = state.history[readIdx];
                    sampleL_cur += sVal * curL[tap];
                    sampleR_cur += sVal * curR[tap];
                    sampleL_prev += sVal * prevL[tap];
                    sampleR_prev += sVal * prevR[tap];
                }

                float t = (float)f / (float)frameCount;
                g_tempL[f] += (1.0f - t) * sampleL_prev + t * sampleL_cur;
                g_tempR[f] += (1.0f - t) * sampleR_prev + t * sampleR_cur;

                state.histPos = (state.histPos + 1) & ChannelConvolutionState::RING_MASK;
            }

            state.lastHrirL = *pHrirL;
            state.lastHrirR = *pHrirR;
        }

        // Apply Headphone EQ Correction if loaded
        if (g_headphoneEq.HasAutoEq() || g_headphoneEq.HasConvolutionIR()) {
            g_headphoneEq.ProcessStereo(g_tempL.data(), g_tempR.data(), frameCount);
        }

        // Interleave Left & Right into final output buffer for DAC
        for (uint32_t f = 0; f < frameCount; f++) {
            pPcmBuffer[f * 2 + 0] = g_tempL[f];
            pPcmBuffer[f * 2 + 1] = g_tempR[f];
        }

        return S_OK;
    }

    // Fallback: Delegate to original Microsoft engine if no input audio buffers attached
    if (g_pfnOrigProcess) {
        HRESULT hr = g_pfnOrigProcess(pThis, pObjects, pPcmBuffer, frameCount, flags);
        if (FAILED(hr)) return hr;

        // Apply Headphone EQ Correction if configured
        std::lock_guard<std::mutex> lock(g_dspMutex);
        if (g_headphoneEq.HasAutoEq() || g_headphoneEq.HasConvolutionIR()) {
            if (g_tempL.size() < frameCount) {
                g_tempL.resize(frameCount);
                g_tempR.resize(frameCount);
            }
            for (uint32_t f = 0; f < frameCount; f++) {
                g_tempL[f] = pPcmBuffer[f * 2 + 0];
                g_tempR[f] = pPcmBuffer[f * 2 + 1];
            }
            g_headphoneEq.ProcessStereo(g_tempL.data(), g_tempR.data(), frameCount);
            for (uint32_t f = 0; f < frameCount; f++) {
                pPcmBuffer[f * 2 + 0] = g_tempL[f];
                pPcmBuffer[f * 2 + 1] = g_tempR[f];
            }
        }
        return S_OK;
    }

    return E_FAIL;
}

// Hooked CreateDspEngine Method (vtable[3] of IHrtfDspFactory)
HRESULT STDMETHODCALLTYPE HookedCreateDspEngine(
    void* pThis,
    const void* pConfig,
    uint32_t flags,
    uint32_t maxObjects,
    void** ppEngine
) {
    SpatialLogger::Get().LogFormat("[AUDIO ENGINE] Windows Sonic requested CreateDspEngine (maxObjects=%u, flags=0x%X)", maxObjects, flags);
    g_maxObjects = maxObjects ? maxObjects : 144;
    LoadConfigForEq();

    if (!g_pfnOrigCreateDspEngine) return E_FAIL;

    // Call original CreateDspEngine
    HRESULT hr = g_pfnOrigCreateDspEngine(pThis, pConfig, flags, maxObjects, ppEngine);
    if (FAILED(hr) || !ppEngine || !*ppEngine) {
        SpatialLogger::Get().LogFormat("[AUDIO ENGINE ERROR] Original CreateDspEngine failed: 0x%08X", hr);
        return hr;
    }

    // Hook vtable[5] (Process) on the created DSP Engine via shadow vtable
    void*** pEngineVtablePtr = (void***)*ppEngine;
    void** origVtable = *pEngineVtablePtr;

    if (!g_pfnOrigProcess) {
        g_pfnOrigProcess = (PFN_Process)origVtable[5];
    }

    void** shadowVtable = new void*[32];
    std::memcpy(shadowVtable, origVtable, sizeof(void*) * 32);
    shadowVtable[5] = (void*)&HookedProcess;
    *pEngineVtablePtr = shadowVtable;

    SpatialLogger::Get().Log("[AUDIO ENGINE] Process vtable hook installed on native DSP Engine");
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            SpatialLogger::Get().LogFormat("[PROXY] Attached to process PID: %lu", GetCurrentProcessId());
            LoadOriginalDll();
            break;
        case DLL_PROCESS_DETACH:
            SpatialLogger::Get().Log("[PROXY] Detached from process");
            if (g_hOrigDll) {
                FreeLibrary(g_hOrigDll);
                g_hOrigDll = nullptr;
            }
            break;
    }
    return TRUE;
}

// Export 1: CreateHrtfDspFactory (Called by HrtfApo.dll at 0xBB3F)
extern "C" __declspec(dllexport) HRESULT __stdcall CreateHrtfDspFactory(
    UINT32 datasetType,
    IHrtfDspFactory** ppFactory
) {
    if (!ppFactory) return E_POINTER;
    SpatialLogger::Get().LogFormat("[PROXY] CreateHrtfDspFactory called (datasetType=%u)", datasetType);

    LoadOriginalDll();
    if (!g_pfnOrigCreateFactory) {
        SpatialLogger::Get().Log("[PROXY ERROR] g_pfnOrigCreateFactory unavailable!");
        return E_FAIL;
    }

    HRESULT hr = g_pfnOrigCreateFactory(datasetType, ppFactory);
    if (FAILED(hr) || !*ppFactory) {
        SpatialLogger::Get().LogFormat("[PROXY ERROR] Original CreateHrtfDspFactory failed: 0x%08X", hr);
        return hr;
    }

    // Hook vtable[3] (CreateDspEngine) on the returned factory via shadow vtable
    void*** pFactoryVtablePtr = (void***)*ppFactory;
    void** origFactoryVtable = *pFactoryVtablePtr;

    if (!g_pfnOrigCreateDspEngine) {
        g_pfnOrigCreateDspEngine = (PFN_CreateDspEngine)origFactoryVtable[3];
    }

    void** shadowFactoryVtable = new void*[16];
    std::memcpy(shadowFactoryVtable, origFactoryVtable, sizeof(void*) * 16);
    shadowFactoryVtable[3] = (void*)&HookedCreateDspEngine;
    *pFactoryVtablePtr = shadowFactoryVtable;

    SpatialLogger::Get().Log("[PROXY] Factory vtable hook installed successfully");
    return S_OK;
}

// Export 2: CreateHrtfDataHelper (Called by HrtfApo.dll at 0xA088)
extern "C" __declspec(dllexport) HRESULT __stdcall CreateHrtfDataHelper(
    void** ppHelper
) {
    if (!ppHelper) return E_POINTER;
    LoadOriginalDll();
    if (g_pfnOrigCreateHelper) {
        return g_pfnOrigCreateHelper(ppHelper);
    }
    *ppHelper = nullptr;
    return S_OK;
}

// Standard COM exports
extern "C" HRESULT __stdcall DllCanUnloadNow() {
    return S_FALSE; // Keep resident inside audiodg.exe
}

extern "C" HRESULT __stdcall DllGetActivationFactory(void* activatableClassId, void** factory) {
    if (!factory) return E_POINTER;
    *factory = nullptr;
    return E_NOINTERFACE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    LoadOriginalDll();
    if (g_pfnOrigGetClassObject) {
        return g_pfnOrigGetClassObject(rclsid, riid, ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}
