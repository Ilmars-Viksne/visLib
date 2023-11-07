#pragma once
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <Audioclient.h>
#include <Endpointvolume.h>


class CiAudio {
private:
    // Smart pointer for the COM multimedia device enumerator.
    std::shared_ptr<IMMDeviceEnumerator> m_pEnumerator;

    // Vector of smart pointers to manage COM endpoint objects.
    std::vector<std::unique_ptr<IMMDevice>> m_pEndpoints;

    // Pointer to the WAVEFORMATEX structure that specifies the stream format.
    WAVEFORMATEX* m_pFormat;

    // Pointer to the IMMDevice interface.
    IMMDevice* m_pDevice;

    // Pointer to the IAudioClient interface.
    IAudioClient* m_pAudioClient;

    // Index of the audio client.
    size_t m_sizeAudioClientNo;

public:
    CiAudio(): m_pAudioClient(nullptr), m_pFormat(nullptr), m_sizeAudioClientNo(-1) {
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
            hr = pCollection->Item(i, &m_pDevice);
            if (SUCCEEDED(hr)) {
                m_pEndpoints.emplace_back(m_pDevice);
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

    // Activate an endpoint by index.
    void activateEndpointByIndex(size_t index) {
        if (index >= m_pEndpoints.size())
            throw std::out_of_range("Invalid endpoint index.");

        m_sizeAudioClientNo = index;

        m_pDevice = m_pEndpoints[m_sizeAudioClientNo].get();

        // Get the stream format.
        HRESULT hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pAudioClient);
        if (SUCCEEDED(hr)) {
            hr = m_pAudioClient->GetMixFormat(&m_pFormat);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to get stream format.");
            }
        }

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

    std::wstring getStreamFormatInfo() {
        if (m_pFormat == nullptr) {
            throw std::runtime_error("Stream format is not initialized.");
        }

        std::wstringstream info;

        if (m_pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            WAVEFORMATEXTENSIBLE* pFormatExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_pFormat);
            info << L"Format: WAVE_FORMAT_EXTENSIBLE\n";
            info << L"Channels: " << pFormatExt->Format.nChannels << L"\n";
            info << L"Sample Rate: " << pFormatExt->Format.nSamplesPerSec << L"\n";
            info << L"Average Bytes Per Second: " << pFormatExt->Format.nAvgBytesPerSec << L"\n";
            info << L"Block Align: " << pFormatExt->Format.nBlockAlign << L"\n";
            info << L"Bits Per Sample: " << pFormatExt->Format.wBitsPerSample << L"\n";
            info << L"Size: " << pFormatExt->Format.cbSize << L"\n";

            // Check if the SubFormat is KSDATAFORMAT_SUBTYPE_IEEE_FLOAT or KSDATAFORMAT_SUBTYPE_PCM.
            if (IsEqualGUID(pFormatExt->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
                info << L"SubFormat: KSDATAFORMAT_SUBTYPE_IEEE_FLOAT\n";
            }
            else if (IsEqualGUID(pFormatExt->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
                info << L"SubFormat: KSDATAFORMAT_SUBTYPE_PCM\n";
            }
            else {
                info << L"SubFormat: Other\n";
            }
        }
        else {
            info << L"Format: " << m_pFormat->wFormatTag << L"\n";
            info << L"Channels: " << m_pFormat->nChannels << L"\n";
            info << L"Sample Rate: " << m_pFormat->nSamplesPerSec << L"\n";
            info << L"Average Bytes Per Second: " << m_pFormat->nAvgBytesPerSec << L"\n";
            info << L"Block Align: " << m_pFormat->nBlockAlign << L"\n";
            info << L"Bits Per Sample: " << m_pFormat->wBitsPerSample << L"\n";
            info << L"Size: " << m_pFormat->cbSize << L"\n";
        }

        return info.str();
    }

};
