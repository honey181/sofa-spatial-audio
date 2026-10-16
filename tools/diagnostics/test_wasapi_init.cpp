#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <iostream>

int main() {
    CoInitialize(nullptr);
    IMMDeviceEnumerator* pEnum = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    IMMDevice* pDevice = nullptr;
    pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    IAudioClient* pAudioClient = nullptr;
    pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);

    WAVEFORMATEXTENSIBLE wfe = {};
    wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfe.Format.nChannels = 2;
    wfe.Format.nSamplesPerSec = 48000;
    wfe.Format.wBitsPerSample = 32;
    wfe.Format.nBlockAlign = 8;
    wfe.Format.nAvgBytesPerSec = 48000 * 8;
    wfe.Format.cbSize = 22;
    wfe.Samples.wValidBitsPerSample = 32;
    wfe.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    static const GUID GUID_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
    wfe.SubFormat = GUID_IEEE_FLOAT;

    WAVEFORMATEX* pClosest = nullptr;
    HRESULT hrMatch = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wfe, &pClosest);
    std::cout << "IsFormatSupported: hr=0x" << std::hex << hrMatch << std::endl;

    REFERENCE_TIME hnsBuffer = 200000;
    HRESULT hrInit = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsBuffer, 0, (WAVEFORMATEX*)&wfe, nullptr);
    std::cout << "Initialize: hr=0x" << std::hex << hrInit << std::endl;

    pAudioClient->Release();
    pDevice->Release();
    pEnum->Release();
    CoUninitialize();
    return 0;
}
