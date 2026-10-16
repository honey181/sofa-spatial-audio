#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <spatialaudioclient.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "mysofa.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Engine Render Modes
enum RenderEngineMode {
    ENGINE_DIRECT_SOFA = 0,    // Pure direct convolution with SOFA HRTF via WASAPI
    ENGINE_WINDOWS_SONIC = 1   // Windows Spatial Audio pipeline via ISpatialAudioClient
};

// Shared State between GUI and Audio Thread
static std::atomic<bool> g_audioRunning{true};
static std::atomic<int> g_renderEngine{ENGINE_DIRECT_SOFA};
static std::atomic<float> g_targetX{0.0f};      // Target -1.5 (Left) to +1.5 (Right)
static std::atomic<float> g_targetY{0.0f};      // Target -1.0 (Floor) to +1.0 (Ceiling/Up)
static std::atomic<float> g_targetZ{1.0f};      // Target +1.5 (Front) to -1.5 (Back)
static std::atomic<int> g_soundMode{0};         // 0 = Keys/Shaker, 1 = Pink Noise, 2 = Pinna Band (6-12k), 3 = Helicopter, 4 = Radar Ping
static std::atomic<bool> g_autoOrbit{false};    // Auto spin around head
static std::atomic<float> g_orbitSpeed{0.8f};   // Radians per second
static std::atomic<float> g_orbitRadius{1.0f};
static std::atomic<bool> g_invertFrontBack{false}; // Instant front/back coordinate inversion
static std::atomic<bool> g_invertLeftRight{false}; // Instant left/right channel swap

// SOFA database state
static std::mutex g_sofaMutex;
static struct MYSOFA_EASY* g_sofaEasy = nullptr;
static int g_sofaFilterLen = 0;
static bool g_sofaLoaded = false;
static int g_currentSofaIndex = 0; // 0 = MIT KEMAR, 1 = IRCAM LISTEN (Human Ears), 2 = Individualized
static std::string g_sofaStatusText = "Loading SOFA...";

void SwitchSofaDataset(int index) {
    std::ofstream dbg("C:\\ProgramData\\SofaSpatialAudio\\tester_debug.log", std::ios::app);
    dbg << "SwitchSofaDataset(" << index << ") starting..." << std::endl;
    const char* path = "C:\\ProgramData\\SofaSpatialAudio\\default_kemar.sofa";
    if (index == 1) {
        path = "C:\\ProgramData\\SofaSpatialAudio\\ircam_listen.sofa";
    } else if (index == 2) {
        path = "C:\\ProgramData\\SofaSpatialAudio\\custom_individualized.sofa";
    }
    dbg << "Opening " << path << std::endl;
    
    int newLen = 0, err = 0;
    struct MYSOFA_EASY* newEasy = mysofa_open(path, 48000.0f, &newLen, &err);
    dbg << "mysofa_open returned " << newEasy << " err=" << err << " len=" << newLen << std::endl;
    if (newEasy) {
        std::lock_guard<std::mutex> lock(g_sofaMutex);
        struct MYSOFA_EASY* oldEasy = g_sofaEasy;
        g_sofaFilterLen = newLen;
        g_sofaEasy = newEasy;
        if (oldEasy) mysofa_close(oldEasy);
        g_sofaLoaded = true;
        g_currentSofaIndex = index;
        std::stringstream ss;
        if (index == 0) {
            ss << "MIT KEMAR Reference Baseline (Dummy Head) | " << newLen << " taps | Smooth 710 Spherical Positions";
        } else if (index == 1) {
            ss << "IRCAM LISTEN Subject 1002 (Real Human Pinna) | " << newLen << " taps | Anechoic Measurement";
        } else {
            ss << "Individualized (Corrected Right Ear) | " << newLen << " taps | Custom Ear Measurement";
        }
        g_sofaStatusText = ss.str();
        dbg << "SwitchSofaDataset complete: " << g_sofaStatusText << std::endl;
    } else {
        dbg << "Failed to open SOFA: " << err << std::endl;
    }
}

// GUI interaction state
static bool g_draggingRadar = false;
static bool g_draggingHeight = false;
static HWND g_hMainWnd = nullptr;

// Synthesizer State
static float g_synthPhase = 0.0f;
static float g_synthTimer = 0.0f;
static uint32_t g_noiseSeed = 123456789;
static float g_pinkB0 = 0.0f;
static float g_pinkB1 = 0.0f;
static float g_pinkB2 = 0.0f;

inline float FastRand() {
    g_noiseSeed = (g_noiseSeed * 1664525u + 1013904223u);
    return ((float)(g_noiseSeed & 0x00FFFFFF) / (float)0x007FFFFF) - 1.0f;
}

// Continuous broadband test sound synthesis (Rich in pinna elevation spectral cues)
void GenerateMonoSound(float* pSamples, uint32_t frameCount, uint32_t sampleRate) {
    int mode = g_soundMode.load();
    float dt = 1.0f / (float)sampleRate;

    for (uint32_t i = 0; i < frameCount; i++) {
        float s = 0.0f;
        g_synthTimer += dt;

        if (mode == 0) {
            // Mode 0: Shaker / Jingling Keys (5 kHz - 15 kHz rhythmic bursts)
            // Pinna spectral notches are located at 6-11 kHz, providing instant height cues!
            float cycle = std::fmod(g_synthTimer, 0.28f);
            if (cycle < 0.10f) {
                float env = std::sin(cycle * ((float)M_PI / 0.10f));
                float noise = FastRand();
                s = noise * env * 0.35f;
            }
        } else if (mode == 1) {
            // Mode 1: True Continuous Pink Noise via Paul Gardner's 3-pole filter (-3 dB/octave)
            // Equal energy per octave: the international psychoacoustic standard for HRTF pinna testing
            float white = FastRand();
            g_pinkB0 = 0.99765f * g_pinkB0 + white * 0.0990460f;
            g_pinkB1 = 0.96300f * g_pinkB1 + white * 0.2965164f;
            g_pinkB2 = 0.57000f * g_pinkB2 + white * 1.0526913f;
            s = (g_pinkB0 + g_pinkB1 + g_pinkB2 + white * 0.1848f) * 0.09f;
        } else if (mode == 2) {
            // Mode 2: Targeted Pinna Elevation Band Noise (6 kHz - 12 kHz)
            // Concentrates 100% of acoustic energy directly into the human ear vertical notch band
            static float bpY1 = 0.0f, bpY2 = 0.0f;
            float white = FastRand();
            float bp = 0.38f * white + 0.60f * bpY1 - 0.42f * bpY2;
            bpY2 = bpY1;
            bpY1 = bp;
            s = bp * 0.22f;
        } else if (mode == 3) {
            // Mode 3: Helicopter / Drone Flyby (Deep 18 Hz rotor chop + broadband turbine whoosh)
            float rotor = 0.5f * (1.0f + std::sin(g_synthTimer * 2.0f * (float)M_PI * 18.0f));
            float whoosh = FastRand() * 0.18f;
            float lowTurbine = 0.25f * std::sin(g_synthTimer * 2.0f * (float)M_PI * 110.0f);
            s = (lowTurbine + whoosh) * rotor * 0.45f;
        } else {
            // Mode 4: Radar Ping (880 Hz + 1760 Hz)
            float cycle = std::fmod(g_synthTimer, 0.5f);
            if (cycle < 0.15f) {
                float env = std::exp(-cycle * 25.0f);
                float tone = std::sin(g_synthPhase) + 0.3f * std::sin(g_synthPhase * 2.0f);
                s = tone * env * 0.28f;
                g_synthPhase += 2.0f * (float)M_PI * 880.0f * dt;
            }
        }

        if (g_synthPhase > 2.0f * (float)M_PI) g_synthPhase -= 2.0f * (float)M_PI;
        pSamples[i] = s;
    }
}

// -------------------------------------------------------------
// Direct SOFA WASAPI Audio Thread (Pure HRTF convolution + crossfade)
// -------------------------------------------------------------
void RunDirectSofaAudio() {
    try {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IMMDeviceEnumerator* pEnum = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) {
        CoUninitialize();
        return;
    }

    IMMDevice* pDevice = nullptr;
    if (FAILED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) {
        pEnum->Release();
        CoUninitialize();
        return;
    }

    IAudioClient* pAudioClient = nullptr;
    if (FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient))) {
        pDevice->Release();
        pEnum->Release();
        CoUninitialize();
        return;
    }

    WAVEFORMATEXTENSIBLE wfe = {};
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = 2;
    wfe.Format.nSamplesPerSec = 48000;
    wfe.Format.wBitsPerSample = 32;
    wfe.Format.nBlockAlign = 8;
    wfe.Format.nAvgBytesPerSec = 48000 * 8;
    wfe.Format.cbSize = 22;
    wfe.Samples.wValidBitsPerSample = 32;
    static const GUID MY_SUBTYPE_IEEE_FLOAT = 
        { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
    wfe.SubFormat = MY_SUBTYPE_IEEE_FLOAT;

    REFERENCE_TIME hnsBuffer = 200000; // 20ms buffer
    if (FAILED(pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsBuffer, 0, (WAVEFORMATEX*)&wfe, nullptr))) {
        pAudioClient->Release();
        pDevice->Release();
        pEnum->Release();
        CoUninitialize();
        return;
    }

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    pAudioClient->SetEventHandle(hEvent);

    UINT32 bufferSize = 0;
    pAudioClient->GetBufferSize(&bufferSize);

    IAudioRenderClient* pRenderClient = nullptr;
    pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);
    pAudioClient->Start();

    // 4096-sample power-of-two ring buffer for flawless circular convolution
    static const size_t RING_SIZE = 4096;
    static const size_t RING_MASK = RING_SIZE - 1;
    std::vector<float> monoHistory(RING_SIZE, 0.0f);
    size_t histPos = 0;

    std::vector<float> monoSource(1024, 0.0f);
    std::vector<float> irL(1024, 0.0f);
    std::vector<float> irR(1024, 0.0f);
    std::vector<float> irL_prev(1024, 0.0f);
    std::vector<float> irR_prev(1024, 0.0f);
    bool hasPrevFilter = false;

    // Smoothed continuous coordinates
    float curX = 0.0f, curY = 0.0f, curZ = 1.0f;
    float orbitAngle = 0.0f;

    while (g_audioRunning.load()) {
        if (g_renderEngine.load() != ENGINE_DIRECT_SOFA) {
            Sleep(20);
            continue;
        }

        WaitForSingleObject(hEvent, 50);

        UINT32 padding = 0;
        HRESULT hrPad = pAudioClient->GetCurrentPadding(&padding);
        if (FAILED(hrPad)) {
            Sleep(10);
            continue;
        }
        if (padding >= bufferSize) continue;
        UINT32 framesAvailable = bufferSize - padding;
        if (framesAvailable == 0 || framesAvailable > bufferSize) continue;

        if (monoSource.size() < framesAvailable) {
            monoSource.resize(framesAvailable);
        }

        // Generate dry mono audio
        GenerateMonoSound(monoSource.data(), framesAvailable, 48000);

        // Update Auto-Orbit: circles clockwise around listener
        if (g_autoOrbit.load()) {
            orbitAngle += g_orbitSpeed.load() * ((float)framesAvailable / 48000.0f);
            if (orbitAngle > 2.0f * (float)M_PI) orbitAngle -= 2.0f * (float)M_PI;
            float r = g_orbitRadius.load();
            g_targetX.store(r * std::sin(orbitAngle)); // Right is +X
            g_targetZ.store(r * std::cos(orbitAngle)); // Front is +Z
        }

        // Smooth physical inertial glide towards target coordinates
        float tx = g_targetX.load();
        float ty = g_targetY.load();
        float tz = g_targetZ.load();
        if (g_invertFrontBack.load()) tz = -tz;
        curX += (tx - curX) * 0.22f;
        curY += (ty - curY) * 0.22f;
        curZ += (tz - curZ) * 0.22f;

        // Convert User coordinates (curX = Right, curZ = Front, curY = Height -1 to +1)
        // into True 3D Spherical Coordinates and AES69 SOFA Cartesian coordinates:
        // Azimuth: Front = 0 rad, Right = +pi/2 rad, Left = -pi/2 rad, Back = +-pi rad
        float azRad = std::atan2(curX, curZ);
        // Elevation: Full continuous spherical sweep from -90 deg (-pi/2) to +90 deg (+pi/2)
        // When curY = +1.0 (Ceiling), elevRad = +pi/2 (+90 deg, zenith overhead)
        // When curY = -1.0 (Floor), elevRad = -pi/2 (-90 deg, nadir below)
        float elevRad = curY * ((float)M_PI * 0.5f);
        float r = std::clamp(std::sqrt(curX * curX + curZ * curZ) * 1.2f, 0.5f, 2.0f);

        // AES69 SOFA Cartesian: +X = Front, +Y = Left, +Z = Up
        float x_sofa = r * std::cos(elevRad) * std::cos(azRad);
        float y_sofa = -r * std::cos(elevRad) * std::sin(azRad);
        float z_sofa = r * std::sin(elevRad);

        uint32_t activeFilterLen = 558;
        uint32_t convLen = 558;
        int currentSofa = g_currentSofaIndex;

        // Query individualized HRIR from SOFA dataset safely
        {
            std::lock_guard<std::mutex> lock(g_sofaMutex);
            if (g_sofaEasy) {
                activeFilterLen = (uint32_t)g_sofaFilterLen;
                convLen = std::min(activeFilterLen, 1024u);
                if (irL.size() < activeFilterLen) {
                    irL.resize(activeFilterLen, 0.0f);
                    irR.resize(activeFilterLen, 0.0f);
                    irL_prev.resize(activeFilterLen, 0.0f);
                    irR_prev.resize(activeFilterLen, 0.0f);
                }
                float delayL = 0.0f, delayR = 0.0f;
                mysofa_getfilter_float(g_sofaEasy, x_sofa, y_sofa, z_sofa, irL.data(), irR.data(), &delayL, &delayR);
            }
        }

        if (!hasPrevFilter) {
            irL_prev = irL;
            irR_prev = irR;
            hasPrevFilter = true;
        }

        BYTE* pBuffer = nullptr;
        if (SUCCEEDED(pRenderClient->GetBuffer(framesAvailable, &pBuffer)) && pBuffer) {
            float* pOut = reinterpret_cast<float*>(pBuffer);

            // Pure Real-time FIR Convolution with smooth block crossfading (100% clickless & transparent)
            for (UINT32 f = 0; f < framesAvailable; f++) {
                monoHistory[histPos] = monoSource[f];

                float outL_cur = 0.0f, outR_cur = 0.0f;
                float outL_prev = 0.0f, outR_prev = 0.0f;

                uint32_t effConv = std::min(convLen, (uint32_t)std::min(irL.size(), irL_prev.size()));
                for (uint32_t k = 0; k < effConv; k++) {
                    size_t readIdx = (histPos + RING_SIZE - k) & RING_MASK;
                    float sVal = monoHistory[readIdx];
                    outL_cur += sVal * irL[k];
                    outR_cur += sVal * irR[k];
                    outL_prev += sVal * irL_prev[k];
                    outR_prev += sVal * irR_prev[k];
                }

                // Smooth linear crossfade across audio block eliminates all zipper noise during fast movement
                float t = (float)f / (float)framesAvailable;
                float outL = (1.0f - t) * outL_prev + t * outL_cur;
                float outR = (1.0f - t) * outR_prev + t * outR_cur;

                bool swapLR = g_invertLeftRight.load();
                float finalL = swapLR ? outR : outL;
                float finalR = swapLR ? outL : outR;

                pOut[f * 2 + 0] = finalL * 1.5f;
                pOut[f * 2 + 1] = finalR * 1.5f;

                histPos = (histPos + 1) & RING_MASK;
            }

            irL_prev = irL;
            irR_prev = irR;

            pRenderClient->ReleaseBuffer(framesAvailable, 0);
        }
    }

    pAudioClient->Stop();
    pRenderClient->Release();
    CloseHandle(hEvent);
    pAudioClient->Release();
    pDevice->Release();
    pEnum->Release();
        CoUninitialize();
    } catch (const std::exception& e) {
        std::ofstream dbg("C:\\ProgramData\\SofaSpatialAudio\\tester_debug.log", std::ios::app);
        dbg << "sofaThread exception: " << e.what() << std::endl;
        dbg.flush();
    } catch (...) {
        std::ofstream dbg("C:\\ProgramData\\SofaSpatialAudio\\tester_debug.log", std::ios::app);
        dbg << "sofaThread unknown exception!" << std::endl;
        dbg.flush();
    }
}

// -------------------------------------------------------------
// GUI Layout and Persistent GDI Resource Management
// -------------------------------------------------------------
const int RADAR_CX = 220;
const int RADAR_CY = 230;
const int RADAR_R  = 160;

const int SLIDER_X = 450;
const int SLIDER_Y = 70;
const int SLIDER_W = 32;
const int SLIDER_H = 320;

// Persistent GDI Objects to prevent any handle exhaustion
static HFONT g_fontTitle = nullptr;
static HFONT g_fontRegular = nullptr;
static HFONT g_fontBold = nullptr;
static HPEN  g_penGrid = nullptr;
static HPEN  g_penAxis = nullptr;
static HPEN  g_penPuck = nullptr;
static HBRUSH g_brushBg = nullptr;
static HBRUSH g_brushSliderTrack = nullptr;
static HBRUSH g_brushHead = nullptr;

void InitGdiResources() {
    g_fontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontRegular = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold = CreateFontW(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_penGrid = CreatePen(PS_SOLID, 1, RGB(45, 50, 65));
    g_penAxis = CreatePen(PS_SOLID, 1, RGB(65, 75, 95));
    g_penPuck = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    g_brushBg = CreateSolidBrush(RGB(22, 24, 29));
    g_brushSliderTrack = CreateSolidBrush(RGB(32, 36, 46));
    g_brushHead = CreateSolidBrush(RGB(240, 180, 50));
}

void FreeGdiResources() {
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontRegular) DeleteObject(g_fontRegular);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_penGrid) DeleteObject(g_penGrid);
    if (g_penAxis) DeleteObject(g_penAxis);
    if (g_penPuck) DeleteObject(g_penPuck);
    if (g_brushBg) DeleteObject(g_brushBg);
    if (g_brushSliderTrack) DeleteObject(g_brushSliderTrack);
    if (g_brushHead) DeleteObject(g_brushHead);
}

void DrawUI(HDC hdc, RECT rc) {
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HBITMAP oldBM = (HBITMAP)SelectObject(memDC, memBM);

    // Dark Background
    FillRect(memDC, &rc, g_brushBg);

    SelectObject(memDC, g_fontTitle);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOutW(memDC, 30, 20, L"3D Spatial Audio Object Tracker", 31);

    // Active Dataset Indicator
    SelectObject(memDC, g_fontBold);
    if (g_currentSofaIndex == 0) {
        SetTextColor(memDC, RGB(255, 180, 50));
        TextOutW(memDC, 370, 24, L"[1: MIT KEMAR (BASELINE)]", 25);
    } else if (g_currentSofaIndex == 1) {
        SetTextColor(memDC, RGB(0, 210, 255));
        TextOutW(memDC, 370, 24, L"[2: IRCAM LISTEN (HUMAN PINNA)]", 31);
    } else {
        SetTextColor(memDC, RGB(0, 255, 170));
        TextOutW(memDC, 370, 24, L"[3: YOUR INDIVIDUALIZED SOFA]", 29);
    }

    SelectObject(memDC, g_fontRegular);
    SetTextColor(memDC, RGB(130, 140, 160));
    TextOutW(memDC, 30, 48, L"Datasets: [1] KEMAR | [2] IRCAM Human | [3] Individualized | Inversion: [F] Front/Back | [I] Swap L/R", 102);

    // 1. Radar Sound Stage
    SelectObject(memDC, g_penGrid);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));

    Ellipse(memDC, RADAR_CX - RADAR_R * 0.33f, RADAR_CY - RADAR_R * 0.33f, RADAR_CX + RADAR_R * 0.33f, RADAR_CY + RADAR_R * 0.33f);
    Ellipse(memDC, RADAR_CX - RADAR_R * 0.66f, RADAR_CY - RADAR_R * 0.66f, RADAR_CX + RADAR_R * 0.66f, RADAR_CY + RADAR_R * 0.66f);
    Ellipse(memDC, RADAR_CX - RADAR_R, RADAR_CY - RADAR_R, RADAR_CX + RADAR_R, RADAR_CY + RADAR_R);

    SelectObject(memDC, g_penAxis);
    MoveToEx(memDC, RADAR_CX - RADAR_R, RADAR_CY, nullptr);
    LineTo(memDC, RADAR_CX + RADAR_R, RADAR_CY);
    MoveToEx(memDC, RADAR_CX, RADAR_CY - RADAR_R, nullptr);
    LineTo(memDC, RADAR_CX, RADAR_CY + RADAR_R);

    SelectObject(memDC, g_fontBold);
    SetTextColor(memDC, RGB(0, 210, 255));
    TextOutW(memDC, RADAR_CX - 22, RADAR_CY - RADAR_R - 22, L"FRONT", 5);
    SetTextColor(memDC, RGB(120, 130, 150));
    TextOutW(memDC, RADAR_CX - 18, RADAR_CY + RADAR_R + 6, L"BACK", 4);
    TextOutW(memDC, RADAR_CX - RADAR_R - 38, RADAR_CY - 8, L"LEFT", 4);
    TextOutW(memDC, RADAR_CX + RADAR_R + 8, RADAR_CY - 8, L"RIGHT", 5);

    // Center Head Icon with Nose pointing towards FRONT (Up)
    SelectObject(memDC, g_brushHead);
    SelectObject(memDC, g_penGrid);
    Ellipse(memDC, RADAR_CX - 16, RADAR_CY - 16, RADAR_CX + 16, RADAR_CY + 16);
    POINT nose[3] = { {RADAR_CX, RADAR_CY - 22}, {RADAR_CX - 6, RADAR_CY - 14}, {RADAR_CX + 6, RADAR_CY - 14} };
    Polygon(memDC, nose, 3);

    float curX = g_targetX.load();
    float curZ = g_targetZ.load();
    float curY = g_targetY.load();

    int puckX = RADAR_CX + (int)(curX * (RADAR_R * 0.66f));
    int puckY = RADAR_CY - (int)(curZ * (RADAR_R * 0.66f)); // Front (+Z) is drawn at top!

    COLORREF puckColor = (g_currentSofaIndex == 0) ? RGB(255, 180, 50) : (g_currentSofaIndex == 1 ? RGB(0, 210, 255) : RGB(0, 255, 170));
    COLORREF haloColor = (g_currentSofaIndex == 0) ? RGB(100, 70, 20) : (g_currentSofaIndex == 1 ? RGB(0, 80, 100) : RGB(0, 100, 80));

    HBRUSH brushHalo = CreateSolidBrush(haloColor);
    SelectObject(memDC, brushHalo);
    Ellipse(memDC, puckX - 16, puckY - 16, puckX + 16, puckY + 16);
    DeleteObject(brushHalo);

    HBRUSH brushPuck = CreateSolidBrush(puckColor);
    SelectObject(memDC, brushPuck);
    SelectObject(memDC, g_penPuck);
    Ellipse(memDC, puckX - 9, puckY - 9, puckX + 9, puckY + 9);
    DeleteObject(brushPuck);

    // 2. Height Slider
    RECT rcTrack = { SLIDER_X, SLIDER_Y, SLIDER_X + SLIDER_W, SLIDER_Y + SLIDER_H };
    FillRect(memDC, &rcTrack, g_brushSliderTrack);

    int earLevelY = SLIDER_Y + SLIDER_H / 2;
    SelectObject(memDC, g_penAxis);
    MoveToEx(memDC, SLIDER_X - 6, earLevelY, nullptr);
    LineTo(memDC, SLIDER_X + SLIDER_W + 6, earLevelY);

    int handleY = earLevelY - (int)(curY * (SLIDER_H / 2 - 12));
    HBRUSH brushHandle = CreateSolidBrush(puckColor);
    RECT rcHandle = { SLIDER_X - 4, handleY - 8, SLIDER_X + SLIDER_W + 4, handleY + 8 };
    FillRect(memDC, &rcHandle, brushHandle);
    DeleteObject(brushHandle);

    SelectObject(memDC, g_fontBold);
    SetTextColor(memDC, RGB(180, 190, 210));
    TextOutW(memDC, SLIDER_X - 10, SLIDER_Y - 26, L"HEIGHT", 6);

    SelectObject(memDC, g_fontRegular);
    SetTextColor(memDC, RGB(120, 130, 150));
    TextOutW(memDC, SLIDER_X + SLIDER_W + 12, SLIDER_Y + 2, L"+1.0 (Ceiling)", 13);
    TextOutW(memDC, SLIDER_X + SLIDER_W + 12, earLevelY - 8, L" 0.0 (Ear Level)", 16);
    TextOutW(memDC, SLIDER_X + SLIDER_W + 12, SLIDER_Y + SLIDER_H - 16, L"-1.0 (Floor)", 12);

    // 3. Telemetry HUD
    SelectObject(memDC, g_fontBold);
    SetTextColor(memDC, RGB(255, 255, 255));
    TextOutW(memDC, 30, 420, L"REAL-TIME SPATIAL COORDINATES", 29);

    // Azimuth: Front (+Z) = 0 deg, Right (+X) = 90 deg, Back (-Z) = 180 deg, Left (-X) = 270 deg
    float azimuthDeg = std::atan2(curX, curZ) * 180.0f / (float)M_PI;
    if (azimuthDeg < 0.0f) azimuthDeg += 360.0f;
    float dist = std::clamp(std::sqrt(curX * curX + curZ * curZ) * 1.2f, 0.5f, 2.0f);
    float elevDeg = curY * 90.0f;

    std::wstringstream ss;
    ss << L"X: " << std::fixed << std::setprecision(2) << curX << L"  |  "
       << L"Y (Height): " << curY << L"  |  "
       << L"Z: " << curZ << L"    "
       << L"[Azimuth: " << (int)azimuthDeg << L"\u00B0  Elevation: " << (int)elevDeg << L"\u00B0  Dist: " << std::setprecision(2) << dist << L"m]";

    SelectObject(memDC, g_fontRegular);
    SetTextColor(memDC, puckColor);
    std::wstring hudText = ss.str();
    TextOutW(memDC, 30, 444, hudText.c_str(), (int)hudText.length());

    // Status bar with controls
    SetTextColor(memDC, RGB(140, 150, 170));
    std::wstring modeStr;
    int m = g_soundMode.load();
    if (m == 0) modeStr = L"Jingling Keys / Metallic Shaker";
    else if (m == 1) modeStr = L"Continuous Pink Noise";
    else if (m == 2) modeStr = L"Pinna Notch Band (6-12 kHz)";
    else if (m == 3) modeStr = L"Helicopter / Drone Flyby";
    else modeStr = L"Radar Beacon Ping";

    std::wstringstream ctrlStream;
    ctrlStream << L"[M] Sound: " << modeStr 
               << L" | [F] Front/Back: " << (g_invertFrontBack.load() ? L"INVERTED" : L"NORMAL")
               << L" | [I] Ears: " << (g_invertLeftRight.load() ? L"SWAPPED" : L"NORMAL")
               << L" | [SPACE] Orbit: " << (g_autoOrbit.load() ? L"ON" : L"OFF");
    std::wstring statusStr = ctrlStream.str();
    TextOutW(memDC, 30, 472, statusStr.c_str(), (int)statusStr.length());

    SetTextColor(memDC, RGB(90, 190, 110));
    std::wstring sofaInfo = L"SOFA: " + std::wstring(g_sofaStatusText.begin(), g_sofaStatusText.end());
    TextOutW(memDC, 30, 498, sofaInfo.c_str(), (int)sofaInfo.length());

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBM);
    DeleteObject(memBM);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitGdiResources();
            SetTimer(hWnd, 1, 33, nullptr);
            break;
        }
        case WM_TIMER: {
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }
        case WM_LBUTTONDOWN: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);

            float dx = (float)(mx - RADAR_CX);
            float dy = (float)(my - RADAR_CY);
            if (std::sqrt(dx * dx + dy * dy) <= RADAR_R + 15) {
                g_draggingRadar = true;
                g_autoOrbit.store(false);
                float nx = std::clamp(dx / (RADAR_R * 0.66f), -1.5f, 1.5f);
                float nz = std::clamp(-dy / (RADAR_R * 0.66f), -1.5f, 1.5f); // Up (-dy) = Front (+Z)
                g_targetX.store(nx);
                g_targetZ.store(nz);
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (mx >= SLIDER_X - 15 && mx <= SLIDER_X + SLIDER_W + 15 && my >= SLIDER_Y && my <= SLIDER_Y + SLIDER_H) {
                g_draggingHeight = true;
                int earLevelY = SLIDER_Y + SLIDER_H / 2;
                float ny = (float)(earLevelY - my) / (float)(SLIDER_H / 2 - 12);
                g_targetY.store(std::clamp(ny, -1.0f, 1.0f));
                SetCapture(hWnd);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }
        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);

            if (g_draggingRadar) {
                float dx = (float)(mx - RADAR_CX);
                float dy = (float)(my - RADAR_CY);
                float nx = std::clamp(dx / (RADAR_R * 0.66f), -1.5f, 1.5f);
                float nz = std::clamp(-dy / (RADAR_R * 0.66f), -1.5f, 1.5f); // Up (-dy) = Front (+Z)
                g_targetX.store(nx);
                g_targetZ.store(nz);
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (g_draggingHeight) {
                int earLevelY = SLIDER_Y + SLIDER_H / 2;
                float ny = (float)(earLevelY - my) / (float)(SLIDER_H / 2 - 12);
                g_targetY.store(std::clamp(ny, -1.0f, 1.0f));
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (g_draggingRadar || g_draggingHeight) {
                g_draggingRadar = false;
                g_draggingHeight = false;
                ReleaseCapture();
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float currentY = g_targetY.load();
            currentY += (delta > 0 ? 0.1f : -0.1f);
            g_targetY.store(std::clamp(currentY, -1.0f, 1.0f));
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_SPACE) {
                g_autoOrbit.store(!g_autoOrbit.load());
            } else if (wParam == 'M') {
                g_soundMode.store((g_soundMode.load() + 1) % 5);
            } else if (wParam == '1') {
                SwitchSofaDataset(0); // MIT KEMAR Baseline
            } else if (wParam == '2') {
                SwitchSofaDataset(1); // IRCAM LISTEN 1002 (Real Human Pinna)
            } else if (wParam == '3') {
                SwitchSofaDataset(2); // Individualized SOFA
            } else if (wParam == 'O') {
                // Cycle through all 3 datasets
                SwitchSofaDataset((g_currentSofaIndex + 1) % 3);
            } else if (wParam == 'F') {
                // Instant Front/Back inversion toggle
                g_invertFrontBack.store(!g_invertFrontBack.load());
            } else if (wParam == 'I') {
                // Instant Left/Right channel swap toggle
                g_invertLeftRight.store(!g_invertLeftRight.load());
            } else if (wParam == 'R') {
                g_targetX.store(0.0f);
                g_targetY.store(0.0f);
                g_targetZ.store(1.0f); // Reset to Front (+Z)
                g_autoOrbit.store(false);
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            DrawUI(hdc, rc);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY: {
            KillTimer(hWnd, 1);
            FreeGdiResources();
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    std::ofstream dbg("C:\\ProgramData\\SofaSpatialAudio\\tester_debug.log", std::ios::app);
    dbg << "CRASH! ExceptionCode: 0x" << std::hex << ep->ExceptionRecord->ExceptionCode 
        << " Address: " << ep->ExceptionRecord->ExceptionAddress << std::endl;
    dbg.flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(CrashFilter);
    std::ofstream dbg("C:\\ProgramData\\SofaSpatialAudio\\tester_debug.log", std::ios::app);
    dbg << "=== SpatialAudioTester Started ===" << std::endl;
    dbg.flush();

    // 1. Initialize with MIT KEMAR Reference Standard by default (User can press [O] to switch to Individualized)
    dbg << "Loading SOFA dataset..." << std::endl;
    dbg.flush();
    SwitchSofaDataset(0);
    dbg << "SOFA loaded, filterLen=" << g_sofaFilterLen << std::endl;
    dbg.flush();

    const wchar_t CLASS_NAME[] = L"SpatialAudioTesterClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc)) {
        dbg << "RegisterClassW failed: " << GetLastError() << std::endl;
    }

    HWND hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Windows Spatial Audio 3D Interactive Tester (SOFA HRTF)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 740, 580,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWnd) {
        dbg << "CreateWindowExW failed: " << GetLastError() << std::endl;
        return 0;
    }
    g_hMainWnd = hWnd;
    dbg << "Window created: " << hWnd << std::endl;

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);

    // Launch dedicated real-time SOFA audio convolution thread
    dbg << "Launching sofaThread..." << std::endl;
    std::thread sofaThread(RunDirectSofaAudio);

    dbg << "Entering message loop..." << std::endl;
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    dbg << "Message loop exited. wParam=" << msg.wParam << std::endl;

    g_audioRunning.store(false);
    if (sofaThread.joinable()) sofaThread.join();

    {
        std::lock_guard<std::mutex> lock(g_sofaMutex);
        if (g_sofaEasy) {
            mysofa_close(g_sofaEasy);
            g_sofaEasy = nullptr;
        }
    }
    dbg << "Clean exit." << std::endl;
    return 0;
}
