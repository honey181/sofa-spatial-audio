#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <iostream>

int main() {
    CoInitialize(nullptr);
    IMMDeviceEnumerator* pEnum = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    IMMDevice* pDev = nullptr;
    pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDev);
    IPropertyStore* pStore = nullptr;
    pDev->OpenPropertyStore(STGM_READ, &pStore);
    PROPVARIANT var;
    PropVariantInit(&var);
    pStore->GetValue(PKEY_Device_FriendlyName, &var);
    std::wcout << L"Default Render Device: " << var.pwszVal << std::endl;
    return 0;
}
