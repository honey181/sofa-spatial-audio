#pragma once

#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)
#  include <windows.h>
#  include <objbase.h>
#  include <unknwn.h>
#endif

// Fallbacks for IDE Language Servers (IntelliSense/Clangd) if Windows SDK is unindexed
#ifndef _GUID_DEFINED
#define _GUID_DEFINED
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;
#endif
typedef GUID IID;
typedef GUID CLSID;
#ifndef REFIID
#define REFIID const GUID&
#endif
#endif

#ifndef _HRESULT_DEFINED
#define _HRESULT_DEFINED
typedef long HRESULT;
#endif

#ifndef STDMETHODCALLTYPE
#  if defined(_WIN32) && !defined(_M_AMD64) && !defined(__x86_64__)
#    define STDMETHODCALLTYPE __stdcall
#  else
#    define STDMETHODCALLTYPE
#  endif
#endif

#ifndef ULONG
typedef unsigned long ULONG;
#endif

#ifndef UINT32
typedef uint32_t UINT32;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
#ifndef IUnknown
struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
    virtual ULONG STDMETHODCALLTYPE Release() = 0;
};
#endif
#endif

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif
#ifndef E_POINTER
#define E_POINTER ((HRESULT)0x80004003L)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef E_UNEXPECTED
#define E_UNEXPECTED ((HRESULT)0x8000FFFFL)
#endif

// GUIDs reverse-engineered from HrtfDspCpu.dll & HrtfApo.dll
// IID_IHrtfDspFactory: {B91F3DB7-302C-4AE3-83DE-7735F24EF462}
static const IID IID_IHrtfDspFactory = 
    { 0xB91F3DB7, 0x302C, 0x4AE3, { 0x83, 0xDE, 0x77, 0x35, 0xF2, 0x4E, 0xF4, 0x62 } };

// IID_IHrtfDataHelper: {82E34892-4F34-418A-A947-AE4EDF483D57}
static const IID IID_IHrtfDataHelper = 
    { 0x82E34892, 0x4F34, 0x418A, { 0xA9, 0x47, 0xAE, 0x4E, 0xDF, 0x48, 0x3D, 0x57 } };

// Audio Object Parameters passed to the DSP Engine
// Matches the exact 120-byte (0x78) memory footprint verified in HrtfApo.dll (imul r15, r15, 0x78)
#pragma pack(push, 4)
struct HrtfSpatialObjectData {
    // 0x00 - 0x0C: 3D Position in Cartesian Coordinates (-1.0 to +1.0 or meters)
    float posX;
    float posY;
    float posZ;
    float reserved0;

    // 0x10 - 0x1C: Velocity / Orientation Vector
    float velX;
    float velY;
    float velZ;
    float reserved1;

    // 0x20 - 0x2C: Volume Gains & Directivity
    float gain;
    float directivityInnerAngle;
    float directivityOuterAngle;
    float directivityOuterGain;

    // 0x30 - 0x3C: Distance Attenuation
    float minDistance;
    float maxDistance;
    float distanceRollOff;
    float reserved2;

    // 0x40 - 0x6C: Acoustic Space / Room Reflection Parameters
    float roomFilterCoeffs[12];

    // 0x70 - 0x77: Status flags & timestamps
    uint32_t flags;
    uint32_t objectId;
};
#pragma pack(pop)

static_assert(sizeof(HrtfSpatialObjectData) == 0x78, "HrtfSpatialObjectData must be exactly 120 bytes (0x78)");

// Forward declaration of DSP Engine interface
struct IHrtfDspEngine : public IUnknown {
    // vtable[0..2] = QueryInterface, AddRef, Release
    // vtable[3]: Initialize engine state
    virtual HRESULT STDMETHODCALLTYPE Initialize(const void* pConfig, uint32_t flags) = 0;
    
    // vtable[4]: Reset buffers and state
    virtual HRESULT STDMETHODCALLTYPE Reset() = 0;

    // vtable[5]: Process Audio Frame (Verified at 0xB607 in HrtfApo.dll)
    // rdx = pObjects, r8 = pPcmBuffer, r9d = frameCount, [rsp+0x20] = flags
    virtual HRESULT STDMETHODCALLTYPE Process(
        const HrtfSpatialObjectData* pObjects,
        float* pPcmBuffer,
        uint32_t frameCount,
        uint32_t flags
    ) = 0;

    // vtable[6]: Set directivity parameters
    virtual HRESULT STDMETHODCALLTYPE SetDirectivity(uint32_t objectIndex, const void* pParams) = 0;

    // vtable[7]: Get Minimum Processing Frame Count
    virtual HRESULT STDMETHODCALLTYPE GetMinFrameCount(uint32_t* pMinFrames) = 0;
};

// Interface for HrtfDspFactory (vtable[3] = CreateDspEngine)
struct IHrtfDspFactory : public IUnknown {
    // vtable[0..2] = QueryInterface, AddRef, Release
    // vtable[3]: CreateDspEngine (Verified at 0xBB8C - 0xBBDC in HrtfApo.dll)
    virtual HRESULT STDMETHODCALLTYPE CreateDspEngine(
        const void* pConfig,
        uint32_t flags,
        uint32_t maxObjects,
        IHrtfDspEngine** ppEngine
    ) = 0;
};
