#include <windows.h>
#include <iostream>
#include <iomanip>
#include "hrtf_dsp_interfaces.h"

typedef HRESULT(__stdcall* PFN_CreateHrtfDspFactory)(UINT32 datasetType, IHrtfDspFactory** ppFactory);

int main() {
    HMODULE hDll = LoadLibraryA("C:\\Windows\\System32\\HrtfDspCpu_orig.dll");
    if (!hDll) {
        std::cout << "Failed to load HrtfDspCpu_orig.dll" << std::endl;
        return 1;
    }

    auto pfnCreateFactory = (PFN_CreateHrtfDspFactory)GetProcAddress(hDll, "CreateHrtfDspFactory");
    if (!pfnCreateFactory) {
        std::cout << "Failed to get CreateHrtfDspFactory" << std::endl;
        return 1;
    }

    IHrtfDspFactory* pFactory = nullptr;
    HRESULT hr = pfnCreateFactory(0, &pFactory);
    if (FAILED(hr) || !pFactory) {
        std::cout << "CreateHrtfDspFactory failed: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    void** factoryVtable = *(void***)pFactory;
    uintptr_t baseAddr = (uintptr_t)hDll;

    std::cout << "HrtfDspCpu_orig.dll Base: 0x" << std::hex << baseAddr << std::endl;
    std::cout << "\n--- IHrtfDspFactory VTable ---" << std::endl;
    for (int i = 0; i < 8; i++) {
        uintptr_t fn = (uintptr_t)factoryVtable[i];
        std::cout << "  vtable[" << i << "] = 0x" << std::hex << fn 
                  << " (RVA: +0x" << (fn - baseAddr) << ")" << std::endl;
    }

    IHrtfDspEngine* pEngine = nullptr;
    hr = pFactory->CreateDspEngine(nullptr, 0, 144, &pEngine);
    if (FAILED(hr) || !pEngine) {
        std::cout << "CreateDspEngine failed: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    void** engineVtable = *(void***)pEngine;
    std::cout << "\n--- IHrtfDspEngine VTable ---" << std::endl;
    for (int i = 0; i < 16; i++) {
        uintptr_t fn = (uintptr_t)engineVtable[i];
        std::cout << "  vtable[" << i << "] = 0x" << std::hex << fn 
                  << " (RVA: +0x" << (fn - baseAddr) << ")" << std::endl;
    }

    return 0;
}
