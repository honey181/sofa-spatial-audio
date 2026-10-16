#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>

int main() {
    CoInitialize(nullptr);
    IMMDeviceEnumerator* pEnum = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (!pEnum) return 1;

    IMMDevice* pDevice = nullptr;
    pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (!pDevice) return 1;

    IAudioClient* pClient = nullptr;
    pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pClient);
    if (!pClient) return 1;

    WAVEFORMATEX* pMixFormat = nullptr;
    pClient->GetMixFormat(&pMixFormat);
    if (pMixFormat) {
        std::cout << "Default Device Mix Format:" << std::endl;
        std::cout << "  Sample Rate: " << pMixFormat->nSamplesPerSec << " Hz" << std::endl;
        std::cout << "  Channels:    " << pMixFormat->nChannels << std::endl;
        std::cout << "  Bits/sample: " << pMixFormat->wBitsPerSample << std::endl;
        std::cout << "  Format tag:  " << pMixFormat->wFormatTag << std::endl;
        CoTaskMemFree(pMixFormat);
    }

    pClient->Release();
    pDevice->Release();
    pEnum->Release();
    CoUninitialize();
    return 0;
}
