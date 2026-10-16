#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <thread>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include "mysofa.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// User 3D Position State (-1.0 to +1.0)
static std::atomic<float> g_posX{0.0f};  // -1.0 (Left) to +1.0 (Right)
static std::atomic<float> g_posZ{1.0f};  // +1.0 (Front) to -1.0 (Back)
static std::atomic<float> g_posY{0.0f};  // -1.0 (Floor) to +1.0 (Ceiling)
static std::atomic<bool>  g_running{true};

// SOFA Engine
static struct MYSOFA_EASY* g_sofa = nullptr;
static int g_filterLen = 0;

// Simple broadband test sound (rhythmic noise clicks rich in pinna cues)
static float g_soundTimer = 0.0f;
static uint32_t g_randSeed = 12345;
inline float RandFloat() {
    g_randSeed = g_randSeed * 1664525u + 1013904223u;
    return ((float)(g_randSeed & 0x00FFFFFF) / (float)0x007FFFFF) - 1.0f;
}

void GenerateAudio(float* pOut, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
        g_soundTimer += 1.0f / 48000.0f;
        float cycle = std::fmod(g_soundTimer, 0.25f); // 4 clicks per second
        float env = (cycle < 0.08f) ? std::sin(cycle * ((float)M_PI / 0.08f)) : 0.0f;
        pOut[i] = RandFloat() * env * 0.4f;
    }
}

// Audio Thread using WASAPI Stereo Shared Mode
void AudioThread() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* pEnum = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) return;

    IMMDevice* pDevice = nullptr;
    if (FAILED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) { pEnum->Release(); return; }

    IAudioClient* pAudioClient = nullptr;
    if (FAILED(pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient))) { pDevice->Release(); pEnum->Release(); return; }

    WAVEFORMATEXTENSIBLE wfe = {};
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = 2;
    wfe.Format.nSamplesPerSec = 48000;
    wfe.Format.wBitsPerSample = 32;
    wfe.Format.nBlockAlign = 8;
    wfe.Format.nAvgBytesPerSec = 48000 * 8;
    wfe.Format.cbSize = 22;
    wfe.Samples.wValidBitsPerSample = 32;
    static const GUID GUID_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
    wfe.SubFormat = GUID_FLOAT;

    if (FAILED(pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 200000, 0, (WAVEFORMATEX*)&wfe, nullptr))) return;

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    pAudioClient->SetEventHandle(hEvent);

    UINT32 bufferFrames = 0;
    pAudioClient->GetBufferSize(&bufferFrames);

    IAudioRenderClient* pRender = nullptr;
    pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRender);
    pAudioClient->Start();

    // Convolution history buffer
    std::vector<float> monoDry(bufferFrames, 0.0f);
    std::vector<float> ringHistory(2048, 0.0f);
    size_t ringPos = 0;

    std::vector<float> irL(g_filterLen, 0.0f);
    std::vector<float> irR(g_filterLen, 0.0f);

    while (g_running.load()) {
        WaitForSingleObject(hEvent, 50);

        UINT32 pad = 0;
        pAudioClient->GetCurrentPadding(&pad);
        UINT32 available = bufferFrames - pad;
        if (available == 0) continue;

        if (monoDry.size() < available) monoDry.resize(available);
        GenerateAudio(monoDry.data(), available);

        // Read current 3D position
        float ux = g_posX.load(); // Right (+X)
        float uz = g_posZ.load(); // Front (+Z)
        float uy = g_posY.load(); // Height (+Y: -1 to +1)

        // Convert to spherical & SOFA coordinates
        float az = std::atan2(ux, uz);
        float el = uy * ((float)M_PI * 0.5f); // Full -90 deg to +90 deg
        float r = 1.0f;

        float x_sofa = r * std::cos(el) * std::cos(az); // Front
        float y_sofa = -r * std::cos(el) * std::sin(az); // Left (+Y) / Right (-Y)
        float z_sofa = r * std::sin(el);                // Height

        // Get HRIR filter directly from SOFA file
        float delL = 0.0f, delR = 0.0f;
        mysofa_getfilter_float(g_sofa, x_sofa, y_sofa, z_sofa, irL.data(), irR.data(), &delL, &delR);

        BYTE* pData = nullptr;
        if (SUCCEEDED(pRender->GetBuffer(available, &pData)) && pData) {
            float* pOut = reinterpret_cast<float*>(pData);

            // Direct FIR Convolution
            uint32_t taps = std::min((uint32_t)g_filterLen, 512u);
            for (UINT32 f = 0; f < available; f++) {
                ringHistory[ringPos] = monoDry[f];

                float outL = 0.0f, outR = 0.0f;
                for (uint32_t k = 0; k < taps; k++) {
                    size_t idx = (ringPos + 2048 - k) & 2047;
                    float s = ringHistory[idx];
                    outL += s * irL[k];
                    outR += s * irR[k];
                }

                pOut[f * 2 + 0] = outL * 1.5f;
                pOut[f * 2 + 1] = outR * 1.5f;
                ringPos = (ringPos + 1) & 2047;
            }

            pRender->ReleaseBuffer(available, 0);
        }
    }

    pAudioClient->Stop();
    pRender->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnum->Release();
    CloseHandle(hEvent);
    CoUninitialize();
}

// Win32 GUI
const int RADAR_X = 180, RADAR_Y = 190, RADAR_R = 120;
const int SLID_X = 350, SLID_Y = 70, SLID_W = 30, SLID_H = 240;

static bool g_dragRadar = false;
static bool g_dragSlider = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hWnd, 1, 33, nullptr);
            break;
        case WM_TIMER:
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case WM_LBUTTONDOWN: {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            float dx = (float)(mx - RADAR_X), dy = (float)(my - RADAR_Y);
            if (std::sqrt(dx*dx + dy*dy) <= RADAR_R + 10) {
                g_dragRadar = true;
                g_posX.store(std::clamp(dx / RADAR_R, -1.0f, 1.0f));
                g_posZ.store(std::clamp(-dy / RADAR_R, -1.0f, 1.0f)); // Up on screen is Front (+Z)
                SetCapture(hWnd);
            } else if (mx >= SLID_X - 10 && mx <= SLID_X + SLID_W + 10 && my >= SLID_Y && my <= SLID_Y + SLID_H) {
                g_dragSlider = true;
                int midY = SLID_Y + SLID_H / 2;
                g_posY.store(std::clamp((float)(midY - my) / (SLID_H / 2), -1.0f, 1.0f));
                SetCapture(hWnd);
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (g_dragRadar) {
                int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
                g_posX.store(std::clamp((float)(mx - RADAR_X) / RADAR_R, -1.0f, 1.0f));
                g_posZ.store(std::clamp(-(float)(my - RADAR_Y) / RADAR_R, -1.0f, 1.0f));
            } else if (g_dragSlider) {
                int my = GET_Y_LPARAM(lp);
                int midY = SLID_Y + SLID_H / 2;
                g_posY.store(std::clamp((float)(midY - my) / (SLID_H / 2), -1.0f, 1.0f));
            }
            break;
        }
        case WM_LBUTTONUP:
            g_dragRadar = g_dragSlider = false;
            ReleaseCapture();
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);

            HDC mdc = CreateCompatibleDC(hdc);
            HBITMAP mbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            SelectObject(mdc, mbm);

            HBRUSH bg = CreateSolidBrush(RGB(20, 22, 28));
            FillRect(mdc, &rc, bg);
            DeleteObject(bg);

            SetBkMode(mdc, TRANSPARENT);
            SetTextColor(mdc, RGB(255, 255, 255));
            TextOutW(mdc, 20, 15, L"Simple 3D SOFA Audio Tester", 27);

            // Radar Circle
            HPEN pGrid = CreatePen(PS_SOLID, 1, RGB(60, 65, 80));
            SelectObject(mdc, pGrid);
            SelectObject(mdc, GetStockObject(NULL_BRUSH));
            Ellipse(mdc, RADAR_X - RADAR_R, RADAR_Y - RADAR_R, RADAR_X + RADAR_R, RADAR_Y + RADAR_R);
            MoveToEx(mdc, RADAR_X - RADAR_R, RADAR_Y, nullptr); LineTo(mdc, RADAR_X + RADAR_R, RADAR_Y);
            MoveToEx(mdc, RADAR_X, RADAR_Y - RADAR_R, nullptr); LineTo(mdc, RADAR_X, RADAR_Y + RADAR_R);

            SetTextColor(mdc, RGB(0, 200, 255));
            TextOutW(mdc, RADAR_X - 18, RADAR_Y - RADAR_R - 18, L"FRONT", 5);
            SetTextColor(mdc, RGB(140, 140, 140));
            TextOutW(mdc, RADAR_X - 16, RADAR_Y + RADAR_R + 4, L"BACK", 4);
            TextOutW(mdc, RADAR_X - RADAR_R - 36, RADAR_Y - 8, L"LEFT", 4);
            TextOutW(mdc, RADAR_X + RADAR_R + 8, RADAR_Y - 8, L"RIGHT", 5);

            // Puck
            int px = RADAR_X + (int)(g_posX.load() * RADAR_R);
            int py = RADAR_Y - (int)(g_posZ.load() * RADAR_R);
            HBRUSH bPuck = CreateSolidBrush(RGB(0, 255, 180));
            SelectObject(mdc, bPuck);
            Ellipse(mdc, px - 8, py - 8, px + 8, py + 8);
            DeleteObject(bPuck);

            // Height Slider
            RECT rTrack = { SLID_X, SLID_Y, SLID_X + SLID_W, SLID_Y + SLID_H };
            HBRUSH bTrack = CreateSolidBrush(RGB(35, 40, 50));
            FillRect(mdc, &rTrack, bTrack);
            DeleteObject(bTrack);

            int midY = SLID_Y + SLID_H / 2;
            int hy = midY - (int)(g_posY.load() * (SLID_H / 2));
            HBRUSH bHandle = CreateSolidBrush(RGB(0, 255, 180));
            RECT rHand = { SLID_X - 4, hy - 6, SLID_X + SLID_W + 4, hy + 6 };
            FillRect(mdc, &rHand, bHandle);
            DeleteObject(bHandle);

            SetTextColor(mdc, RGB(180, 190, 210));
            TextOutW(mdc, SLID_X, SLID_Y - 22, L"HEIGHT", 6);
            SetTextColor(mdc, RGB(120, 120, 120));
            TextOutW(mdc, SLID_X + SLID_W + 10, SLID_Y, L"+1.0 (Top)", 10);
            TextOutW(mdc, SLID_X + SLID_W + 10, midY - 8, L" 0.0 (Ear)", 10);
            TextOutW(mdc, SLID_X + SLID_W + 10, SLID_Y + SLID_H - 16, L"-1.0 (Bottom)", 13);

            // Position Readout
            float curAz = std::atan2(g_posX.load(), g_posZ.load()) * 180.0f / (float)M_PI;
            if (curAz < 0) curAz += 360.0f;
            float curEl = g_posY.load() * 90.0f;

            wchar_t info[128];
            wsprintfW(info, L"Azimuth: %d deg  |  Elevation: %d deg", (int)curAz, (int)curEl);
            SetTextColor(mdc, RGB(0, 255, 180));
            TextOutW(mdc, 20, 335, info, lstrlenW(info));

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
            DeleteObject(pGrid);
            DeleteObject(mbm);
            DeleteDC(mdc);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_DESTROY:
            KillTimer(hWnd, 1);
            g_running.store(false);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hWnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // 1. Load SOFA file
    const char* sofaPath = "config/default_kemar.sofa";
    int err = 0;
    g_sofa = mysofa_open("custom_individualized.sofa", 48000.0f, &g_filterLen, &err);
    if (!g_sofa) {
        g_sofa = mysofa_open(sofaPath, 48000.0f, &g_filterLen, &err);
    }
    if (!g_sofa) {
        MessageBoxW(nullptr, L"Failed to load SOFA file!\nMake sure config/default_kemar.sofa or custom_individualized.sofa is present.", L"Error", MB_ICONERROR);
        return 1;
    }

    // 2. Start audio thread
    std::thread th(AudioThread);

    // 3. Simple window
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"SimpleSofaTesterClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Simple 3D SOFA Audio Tester",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 420,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_running.store(false);
    if (th.joinable()) th.join();
    if (g_sofa) mysofa_close(g_sofa);

    return 0;
}
