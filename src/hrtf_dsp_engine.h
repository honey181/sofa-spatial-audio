#pragma once

#include "hrtf_dsp_interfaces.h"
#include "sofa_loader.h"
#include "headphone_eq.h"
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

class CustomHrtfDspEngine : public IHrtfDspEngine {
public:
    CustomHrtfDspEngine(uint32_t maxObjects);
    virtual ~CustomHrtfDspEngine();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;

    // IHrtfDspEngine
    virtual HRESULT STDMETHODCALLTYPE Initialize(const void* pConfig, uint32_t flags) override;
    virtual HRESULT STDMETHODCALLTYPE Reset() override;
    virtual HRESULT STDMETHODCALLTYPE Process(
        const HrtfSpatialObjectData* pObjects,
        float* pPcmBuffer,
        uint32_t frameCount,
        uint32_t flags
    ) override;
    virtual HRESULT STDMETHODCALLTYPE SetDirectivity(uint32_t objectIndex, const void* pParams) override;
    virtual HRESULT STDMETHODCALLTYPE GetMinFrameCount(uint32_t* pMinFrames) override;

    // Configuration
    void LoadConfiguration(const std::string& configPath = "");
    bool LoadSofa(const std::string& sofaPath);
    bool LoadHeadphoneWav(const std::string& wavPath);
    bool LoadAutoEq(const std::string& txtPath);

private:
    std::atomic<ULONG> m_refCount{1};
    uint32_t m_maxObjects = 128;
    std::mutex m_dspMutex;

    SofaDatabase m_sofaDb;
    HeadphoneEQ m_headphoneEq;

    // Per-object convolution ring buffers
    struct ObjectState {
        std::vector<float> history;
        size_t histPos = 0;
    };
    std::vector<ObjectState> m_objectStates;

    // Temporary accumulation stereo buffers
    std::vector<float> m_tempLeft;
    std::vector<float> m_tempRight;
};

class CustomHrtfDspFactory : public IHrtfDspFactory {
public:
    CustomHrtfDspFactory();
    virtual ~CustomHrtfDspFactory();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;

    // IHrtfDspFactory
    virtual HRESULT STDMETHODCALLTYPE CreateDspEngine(
        const void* pConfig,
        uint32_t flags,
        uint32_t maxObjects,
        IHrtfDspEngine** ppEngine
    ) override;

private:
    std::atomic<ULONG> m_refCount{1};
};
