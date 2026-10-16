#include <windows.h>
#include <iostream>
#include <iomanip>
#include "hrtf_dsp_interfaces.h"

typedef HRESULT(__stdcall* PFN_CreateHrtfDataHelper)(void** ppHelper);

int main() {
    HMODULE hDll = LoadLibraryA("C:\\Windows\\System32\\HrtfDspCpu_orig.dll");
    if (!hDll) return 1;

    auto pfnCreateHelper = (PFN_CreateHrtfDataHelper)GetProcAddress(hDll, "CreateHrtfDataHelper");
    if (!pfnCreateHelper) return 1;

    void* pHelper = nullptr;
    HRESULT hr = pfnCreateHelper(&pHelper);
    if (FAILED(hr) || !pHelper) {
        std::cout << "CreateHrtfDataHelper failed: 0x" << std::hex << hr << std::endl;
        return 1;
    }

    void** helperVtable = *(void***)pHelper;
    uintptr_t baseAddr = (uintptr_t)hDll;

    std::cout << "\n--- IHrtfDataHelper VTable ---" << std::endl;
    for (int i = 0; i < 12; i++) {
        uintptr_t fn = (uintptr_t)helperVtable[i];
        std::cout << "  vtable[" << i << "] = 0x" << std::hex << fn 
                  << " (RVA: +0x" << (fn - baseAddr) << ")" << std::endl;
    }

    return 0;
}
