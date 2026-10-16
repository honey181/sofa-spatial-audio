# SOFA Spatial Audio — Windows Native 3D Audio & Custom HRTF / EQ Engine

A native system-wide spatial audio engine for Windows 10 & 11 that intercepts uncompressed 3D dynamic audio objects and 7.1.4 bed channels from games, movies, and music apps, rendering them through your **custom `.sofa` HRTF files** and **headphone correction EQ (FIR convolution or AutoEq)** with ultra-low latency.

---

## Architecture Overview

```
[ Games (Cyberpunk, Forza) / Streaming (Apple Music, Netflix) / mpv-omniphony ]
                             │
                             ▼ (ISpatialAudioClient: 128 dynamic objects + 7.1.4 beds)
               [ audiodg.exe (Windows Audio Engine) ]
                             │
                  HrtfApo.dll ──> Calls CreateHrtfDspFactory()
                             │
                             ▼
              [ Our Proxy: HrtfDspCpu.dll ]
        ├── 1. Intercepts (x, y, z) Cartesian vectors & raw float PCM
        ├── 2. Convolves with your custom .sofa file via SofaDatabase
        ├── 3. Applies your Headphone EQ (AutoEq / FIR WAV convolution)
        └── 4. Outputs low-latency binaural stereo directly to DAC
```

---

## Features

- **True 3D Dynamic Object Support**: Processes up to 128 dynamic sound objects with continuous 3D coordinates $(x, y, z)$.
- **Full Height Channel Support**: Renders ceiling/height beds ($+30^\circ$ to $+50^\circ$ elevation) in Apple Music, Netflix, and 4K Blu-ray remuxes.
- **Custom `.sofa` HRTF Support**: Drop your personalized ear-measurement SOFA file into `config/`.
- **Headphone EQ via Convolution or AutoEq**:
  - **FIR WAV Convolution**: Load measured 16/24/32-bit headphone impulse responses (`headphone_eq.wav`).
  - **AutoEq Parametric EQ**: Load standard 10-band or 20-band AutoEq parametric `.txt` profiles.
  - **Zero-Cost Pre-Baking**: Pre-convolves the headphone impulse response directly into the SOFA dataset in RAM at startup for **0.00% extra CPU load** during real-time playback!
- **System-Wide & Anti-Cheat Safe**: Runs completely inside user-mode `audiodg.exe`. No kernel drivers, no `TestSigning` required, 100% safe with anti-cheat engines (Valorant Vanguard, Easy Anti-Cheat, BattlEye).

---

## Directory Structure

```
sofa-spatial-audio/
├── src/
│   ├── main_proxy.cpp         # Exports CreateHrtfDspFactory & CreateHrtfDataHelper
│   ├── hrtf_dsp_engine.h      # CustomHrtfDspEngine & CustomHrtfDspFactory
│   ├── hrtf_dsp_engine.cpp    # Real-time frame processing loop
│   ├── sofa_loader.h          # SOFA loader & spherical HRIR lookup
│   ├── sofa_loader.cpp
│   ├── headphone_eq.h         # Parametric Biquad & FIR Convolution EQ
│   ├── headphone_eq.cpp
│   └── test_spatial.cpp       # Standalone 3D trajectory test generator
├── include/
│   └── hrtf_dsp_interfaces.h  # Reverse-engineered COM vtables & 120-byte object struct
├── config/
│   └── config.ini             # Paths to .sofa file and headphone EQ files
├── bin/
│   ├── HrtfDspCpu.dll         # Compiled standalone proxy DLL (zero external dependencies)
│   └── test_spatial.exe       # 3D spatial audio trajectory test executable
├── build.ps1                  # Build script using Clang++ (MartinStorsjo.LLVM-MinGW)
├── deploy.ps1                 # Safe system deployment with automated backup
├── uninstall.ps1              # One-click restore for original Microsoft DLL
├── REVERSE_ENGINEERING.md     # In-depth architectural & reverse engineering findings
└── README.md
```

---

## Technical Context & Reverse Engineering

For full architectural details, COM vtable definitions, memory struct layouts (`0x78` stride), and engineering notes for continuing development, see **[`REVERSE_ENGINEERING.md`](REVERSE_ENGINEERING.md)**.

---

## Testing in Headphones (Standalone)

Run the test tool directly to verify the 3D positioning without touching any system files:

```powershell
.\bin\test_spatial.exe
```

Or play the generated [`output_3d_test.wav`](output_3d_test.wav) with `mpv`:

```powershell
mpv output_3d_test.wav
```

---

## System Deployment (Optional)

To enable the custom SOFA engine system-wide across all games and apps:

1. Right-click PowerShell and select **Run as Administrator**.
2. Run:
   ```powershell
   .\deploy.ps1
   ```
3. In Windows Sound Settings, ensure **Windows Sonic for Headphones** is selected.

To revert back to Microsoft's original file at any time, run:
```powershell
.\uninstall.ps1
```

---

## Building

To build the self-contained static DLL:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The output DLL will be placed in `bin\HrtfDspCpu.dll`.
