# Windows Spatial Audio (Windows Sonic) & HrtfDspCpu Reverse Engineering Guide

This document summarizes our reverse engineering findings, architecture research, discovered binary interfaces, and technical notes on intercepting Windows Spatial Sound to enable custom HRTF (`.sofa`) convolution and headphone EQ.

---

## 1. System Architecture & The Audio Processing Chain

Windows Spatial Sound uses the `ISpatialAudioClient` COM API to stream uncompressed dynamic 3D audio objects (with continuous $X, Y, Z$ positions) and multichannel 7.1.4 bed channels.

```
┌────────────────────────────────────────────────────────┐
│ Audio Source (Games, Apple Music / Dolby Atmos, mpv)    │
└───────────────────────────┬────────────────────────────┘
                            │ ISpatialAudioClient / ISpatialAudioObjectRenderStream
                            ▼
┌────────────────────────────────────────────────────────┐
│ audiodg.exe (Windows Audio Device Graph Worker)         │
│                                                        │
│  1. HrtfApo.dll (Audio Processing Object)              │
│     - Implements IXAPOHrtfParameters                   │
│     - Converts geometric and acoustic parameters        │
│     - Loads HrtfDspCpu.dll via LoadLibraryExW          │
│                                                        │
│  2. HrtfDspCpu.dll (HRTF DSP Engine)                   │
│     - Exports CreateHrtfDspFactory & CreateHrtfDataHelper│
│     - Performs the actual spatial convolution onto DAC │
└────────────────────────────────────────────────────────┘
```

---

## 2. Reverse Engineered Binary Interfaces (`HrtfDspCpu.dll`)

### 2.1 Exported Functions

Disassembly of `C:\Windows\System32\HrtfDspCpu.dll` confirms only two exported entry points:

| Ordinal | Function Name | Signature | Description |
| :--- | :--- | :--- | :--- |
| **1** | `CreateHrtfDataHelper` | `HRESULT CreateHrtfDataHelper(void** ppHelper)` | Helper object for dataset conversion |
| **2** | `CreateHrtfDspFactory` | `HRESULT CreateHrtfDspFactory(UINT32 datasetType, IHrtfDspFactory** ppFactory)` | Creates the main DSP engine factory |

### 2.2 COM Interface Vtables

#### `IHrtfDspFactory` (COM Vtable)
```cpp
struct IHrtfDspFactoryVtbl {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* This, REFIID riid, void** ppvObject);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* This);
    ULONG   (STDMETHODCALLTYPE *Release)(void* This);

    // vtable[3]: Engine Instantiation
    HRESULT (STDMETHODCALLTYPE *CreateDspEngine)(
        void* pThis,
        const void* pConfig,
        uint32_t flags,
        uint32_t maxObjects,
        void** ppEngine
    );
};
```

#### `IHrtfDspEngine` (COM Vtable)
```cpp
struct IHrtfDspEngineVtbl {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* This, REFIID riid, void** ppvObject);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* This);
    ULONG   (STDMETHODCALLTYPE *Release)(void* This);

    // vtable[3..4]: Internal state & configuration
    HRESULT (STDMETHODCALLTYPE *Method3)(void* This, ...);
    HRESULT (STDMETHODCALLTYPE *Method4)(void* This, ...);

    // vtable[5]: Real-Time Audio Processing Loop
    HRESULT (STDMETHODCALLTYPE *Process)(
        void* pThis,
        const HrtfSpatialObjectData* pObjects,
        float* pPcmBuffer,
        uint32_t frameCount,
        uint32_t objectCount
    );
};
```

### 2.3 Spatial Object Data Structure (`HrtfSpatialObjectData`)

Each spatial object passed to `Process` occupies a stride of **120 bytes (`0x78`)**:

| Offset | Type | Field Name | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | `float` | `posX` | Right coordinate ($+X$ = Right, $-X$ = Left) |
| `0x04` | `float` | `posY` | Height coordinate ($+Y$ = Up / Ceiling, $-Y$ = Down / Floor) |
| `0x08` | `float` | `posZ` | Depth coordinate (DirectX right-handed: $-Z$ = Front, $+Z$ = Rear) |
| `0x0C` | `float` | `gain` | Linear object volume gain ($0.0$ to $1.0+$) |
| `0x60` | `uint32_t`| `flags` | State flags (active, directivity, environmental) |
| `0x68` | `float*` | `pAudioBuffer` | Pointer to raw float PCM input samples (dry audio) |
| `0x70` | `uint32_t`| `sampleCount` | Number of valid samples in `pAudioBuffer` |

### 2.4 PCM Output Buffer
- `float* pPcmBuffer` is an interleaved stereo float buffer: `[L0, R0, L1, R1, ...]`.
- Total buffer capacity is `frameCount * 2 * sizeof(float)`.
- Standard sample rate is 48,000 Hz.

---

## 3. What Has Been Achieved

1. **Self-Contained Compiler Toolchain**:
   - Compiles with LLVM-MinGW (`clang++`) under UCRT via [`build.ps1`](build.ps1).
   - Bundles `libmysofa` and `zlib` directly from source into static libraries—no external DLL dependencies or runtime installers needed.

2. **SOFA File Parser & Interpolator**:
   - Loads AES69 standard `.sofa` files using `mysofa_load` and `mysofa_open`.
   - Supports dense spherical grid sampling and Cartesian lookup.

3. **Headphone Correction EQ Engine**:
   - **AutoEq Support**: Parses standard parametric equalizer exports (Peaking, Low Shelf, High Shelf biquads).
   - **FIR WAV Convolution**: Convolves 16/24/32-bit measured headphone impulse responses.
   - **Zero-Cost RAM Pre-baking**: Pre-convolves the headphone EQ filter into all SOFA HRIR angles at startup, eliminating real-time EQ CPU overhead.

4. **System Deployment Scripts**:
   - [`deploy.ps1`](deploy.ps1): Automates taking ownership of `System32\HrtfDspCpu.dll`, creates a verified backup at `HrtfDspCpu_orig.dll`, and grants required execution rights to `NT AUTHORITY\LOCAL SERVICE`.
   - [`uninstall.ps1`](uninstall.ps1): Restores the original Microsoft binary.

---

## 4. Key Engineering Findings & Roadblocks for Successors

If you are picking up this project, pay close attention to these nuances:

### 1. Windows Audio Engine Security & `audiodg.exe`
- In Windows 10/11, `audiodg.exe` runs as a protected process. By default, unsigned or modified APO DLLs will fail to load or mute system audio.
- **Required Registry Setting**:
  ```reg
  [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio]
  "DisableProtectedAudioDG"=dword:00000001
  ```
- Modifying `C:\Windows\System32\HrtfDspCpu.dll` requires administrative privilege plus taking ownership from `TrustedInstaller` (handled in [`deploy.ps1`](deploy.ps1)).

### 2. Interaural Time Difference (ITD) in SOFA Files
- `mysofa_open` extracts time delays into `delayLeft` and `delayRight` if the SOFA dataset stores delay vectors separately in `Data.Delay`.
- If an implementation discards `delayLeft` and `delayRight`, both ears receive impulses starting at sample 0. This destroys horizontal localization cues and creates front/back confusion. Delay values must either be prepended to the FIR buffer or simulated via fractional ring-buffer read delays.

### 3. Filter Switching & Click Prevention
- Sound sources in games and apps update coordinates dynamically every frame. Snapping FIR filters instantly produces audible discontinuities (clicks/pops).
- Smooth time-domain crossfading (fading previous block out while fading new block in) or frequency-domain overlap-add (WOLA / partitioned convolution) is required when positions change.

### 4. Direct Audio Routing vs. Chaining
- In `IHrtfDspEngine::Process`, parameter 5 is `objectCount`.
- When forwarding to Microsoft's original engine (`HrtfDspCpu_orig.dll`), passing correct object counts is mandatory.
- If dynamic objects pass `pAudioBuffer == nullptr`, audio may be pre-mixed or routed through specific APO shared buffers.

### 5. Alternative Architecture: Virtual Audio Endpoint / Custom APO
- If modifying `System32\HrtfDspCpu.dll` proves inconvenient on specific Windows builds, an alternative approach is implementing a custom virtual surround endpoint (similar to HeSuVi / Equalizer APO) that receives 7.1.4 audio and convolves it with the SOFA dataset before feeding the physical DAC.

---

## 5. Building & Project Layout

```powershell
# Build everything from source:
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

- [`src/main_proxy.cpp`](src/main_proxy.cpp): DLL entry points & COM proxy hooks.
- [`src/sofa_loader.cpp`](src/sofa_loader.cpp): SOFA reading & coordinate mapping.
- [`src/headphone_eq.cpp`](src/headphone_eq.cpp): AutoEq parser & FIR convolver.
- [`src/hrtf_dsp_engine.cpp`](src/hrtf_dsp_engine.cpp): Audio rendering loop.
- [`include/hrtf_dsp_interfaces.h`](include/hrtf_dsp_interfaces.h): COM interface definitions.
- [`tools/diagnostics/`](tools/diagnostics): Standalone inspection & verification scripts.
