#pragma once
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>

class CiAudio {
private:
    // Smart pointer for the COM multimedia device enumerator.
    std::shared_ptr<IMMDeviceEnumerator> m_pEnumerator;

    // Vector of smart pointers to manage COM endpoint objects.
    std::vector<std::unique_ptr<IMMDevice>> m_pEndpoints;

public:
    CiAudio() {
        // Initialize COM library using RAII.
        HRESULT hr = CoInitialize(nullptr);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to initialize COM library");
        }

        // Create a multimedia device enumerator and store it in a smart pointer.
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&m_pEnumerator);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create multimedia device enumerator");
        }

        // Enumerate the audio endpoints and store them in smart pointers.
        IMMDeviceCollection* pCollection = nullptr;
        hr = m_pEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &pCollection);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to enumerate audio endpoints");
        }

        UINT count;
        hr = pCollection->GetCount(&count);
        m_pEndpoints.reserve(count);

        for (UINT i = 0; i < count; i++) {
            IMMDevice* pEndpoint = nullptr;
            hr = pCollection->Item(i, &pEndpoint);
            if (SUCCEEDED(hr)) {
                m_pEndpoints.emplace_back(pEndpoint);
            }
        }

        // Release the endpoint collection.
        CoTaskMemFree(pCollection);
    }

    // Getter for the vector of endpoints with exception handling.
    const std::vector<std::unique_ptr<IMMDevice>>& getEndpoints() const {
        if (m_pEndpoints.empty()) {
            throw std::runtime_error("No audio endpoints available.");
        }
        return m_pEndpoints;
    }

    // Getter for an endpoint by index with exception handling.
    IMMDevice* getEndpointByIndex(size_t index) const {
        if (index < m_pEndpoints.size()) {
            return m_pEndpoints[index].get();
        }
        throw std::out_of_range("Invalid endpoint index.");
    }


    ~CiAudio() {
        // Uninitialize COM library using RAII.
        CoUninitialize();
    }

    std::wstring getAudioEndpointsInfo() {
        std::wstringstream info;

        for (const auto& pEndpoint : m_pEndpoints) {
            // Get the endpoint ID.
            LPWSTR pwszID = nullptr;
            HRESULT hr = pEndpoint->GetId(&pwszID);
            if (SUCCEEDED(hr)) {
                info << L"Endpoint ID: " << pwszID << L"\n";
                CoTaskMemFree(pwszID);
            }

            // Get the endpoint name.
            IPropertyStore* pProps = nullptr;
            hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
            if (SUCCEEDED(hr)) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
                if (SUCCEEDED(hr)) {
                    info << L"Endpoint Name: " << varName.pwszVal << L"\n";
                    PropVariantClear(&varName);
                }
                pProps->Release();
            }

            // Get the endpoint state.
            DWORD dwState;
            hr = pEndpoint->GetState(&dwState);
            if (SUCCEEDED(hr)) {
                info << L"Endpoint State: " << dwState << L"\n\n";
            }
        }

        return info.str();
    }
};
