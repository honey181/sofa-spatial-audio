#include <windows.h>
#include <mmdeviceapi.h>
#include <spatialaudioclient.h>
#include <iostream>
#include <cmath>

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    std::cout << "CoCreateInstance MMDeviceEnumerator hr=0x" << std::hex << hr << std::endl;
    if (SUCCEEDED(hr)) {
        IMMDevice* pDevice = nullptr;
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        std::cout << "GetDefaultAudioEndpoint hr=0x" << std::hex << hr << std::endl;
        if (SUCCEEDED(hr)) {
            ISpatialAudioClient* pClient = nullptr;
            hr = pDevice->Activate(__uuidof(ISpatialAudioClient), CLSCTX_ALL, nullptr, (void**)&pClient);
            std::cout << "Activate ISpatialAudioClient hr=0x" << std::hex << hr << std::endl;
            if (SUCCEEDED(hr)) {
                IAudioFormatEnumerator* pEnumFormat = nullptr;
                hr = pClient->GetSupportedAudioObjectFormatEnumerator(&pEnumFormat);
                std::cout << "GetSupportedAudioObjectFormatEnumerator hr=0x" << std::hex << hr << std::endl;
                WAVEFORMATEX* pWfx = nullptr;
                if (SUCCEEDED(hr) && pEnumFormat) {
                    UINT32 count = 0;
                    pEnumFormat->GetCount(&count);
                    std::cout << "Supported formats count=" << std::dec << count << std::endl;
                    if (count > 0) {
                        pEnumFormat->GetFormat(0, &pWfx);
                        std::cout << "Format 0: rate=" << pWfx->nSamplesPerSec << " ch=" << pWfx->nChannels << " bits=" << pWfx->wBitsPerSample << " tag=0x" << std::hex << pWfx->wFormatTag << std::endl;
                    }
                }

                HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                SpatialAudioObjectRenderStreamActivationParams params = {};
                params.ObjectFormat = pWfx;
                params.StaticObjectTypeMask = AudioObjectType_None;
                params.MinDynamicObjectCount = 1;
                params.MaxDynamicObjectCount = 1;
                params.Category = AudioCategory_SoundEffects;
                params.EventHandle = hEvent;

                PROPVARIANT propVar;
                PropVariantInit(&propVar);
                propVar.vt = VT_BLOB;
                propVar.blob.cbSize = sizeof(params);
                propVar.blob.pBlobData = (BYTE*)&params;

                ISpatialAudioObjectRenderStream* pStream = nullptr;
                hr = pClient->ActivateSpatialAudioStream(&propVar, __uuidof(ISpatialAudioObjectRenderStream), (void**)&pStream);
                std::cout << "ActivateSpatialAudioStream hr=0x" << std::hex << hr << std::endl;
                if (SUCCEEDED(hr) && pStream) {
                    std::cout << "Stream activated successfully! Starting 2-second test sound..." << std::endl;
                    hr = pStream->Start();
                    std::cout << "Stream Start hr=0x" << std::hex << hr << std::endl;

                    ISpatialAudioObject* pObject = nullptr;
                    float phase = 0.0f;
                    float angle = 0.0f;

                    // Run for ~100 frames (~2 seconds at 48kHz / 1024 or 480 frames)
                    for (int i = 0; i < 150; i++) {
                        DWORD waitRes = WaitForSingleObject(hEvent, 500);
                        if (waitRes != WAIT_OBJECT_0) {
                            std::cout << "Wait timeout/error: " << waitRes << std::endl;
                            break;
                        }

                        UINT32 dynamicAvailable = 0, frameCount = 0;
                        hr = pStream->BeginUpdatingAudioObjects(&dynamicAvailable, &frameCount);
                        if (FAILED(hr)) {
                            std::cout << "BeginUpdatingAudioObjects failed: 0x" << std::hex << hr << std::endl;
                            break;
                        }

                        if (!pObject) {
                            hr = pStream->ActivateSpatialAudioObject(AudioObjectType_Dynamic, &pObject);
                            if (FAILED(hr)) {
                                std::cout << "ActivateSpatialAudioObject failed: 0x" << std::hex << hr << std::endl;
                            }
                        }

                        if (pObject) {
                            // Rotate around the head: radius = 1.0m, height = 0.0
                            float posX = std::sin(angle);
                            float posY = 0.0f;
                            float posZ = -std::cos(angle);
                            angle += 0.05f;

                            pObject->SetPosition(posX, posY, posZ);

                            BYTE* pBuffer = nullptr;
                            UINT32 bufferLen = 0;
                            hr = pObject->GetBuffer(&pBuffer, &bufferLen);
                            if (SUCCEEDED(hr) && pBuffer) {
                                float* pSamples = reinterpret_cast<float*>(pBuffer);
                                for (UINT32 s = 0; s < frameCount; s++) {
                                    // 440 Hz beep with 2 Hz pulsator
                                    float env = 0.5f * (1.0f + std::sin(phase * 0.02f));
                                    pSamples[s] = 0.2f * env * std::sin(phase);
                                    phase += 2.0f * 3.14159265f * 440.0f / 48000.0f;
                                    if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
                                }
                            }
                        }

                        pStream->EndUpdatingAudioObjects();
                    }

                    if (pObject) {
                        pObject->SetEndOfStream(0);
                        pObject->Release();
                    }
                    pStream->Stop();
                    pStream->Release();
                    std::cout << "Test completed successfully!" << std::endl;
                }
                CloseHandle(hEvent);
                if (pWfx) CoTaskMemFree(pWfx);
                pClient->Release();
            }
            pDevice->Release();
        }
        pEnum->Release();
    }
    CoUninitialize();
    return 0;
}
