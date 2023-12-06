#pragma once
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <Audioclient.h>
#include <Endpointvolume.h>
#include <thread>
#include <queue>
#include <mutex>

namespace vi {

    // Define a new structure to hold the audio frrame data
    struct AudioCH2F {
        float chA;
        float chB;
    };

    class CiAudio {
    private:
        // Smart pointer for the COM multimedia device enumerator.
        std::shared_ptr<IMMDeviceEnumerator> m_pEnumerator;

        // A collection of multimedia device resources.
        IMMDeviceCollection* m_pCollection = nullptr;

        // Pointer to the WAVEFORMATEX structure that specifies the stream format.
        WAVEFORMATEX* m_pFormat;

        // Pointer to the IMMDevice interface.
        IMMDevice* m_pDevice;

        // Pointer to the IAudioClient interface.
        IAudioClient* m_pAudioClient;

        // Index of the audio client.
        size_t m_sizeAudioClientNo;

        // Pointer to the IAudioCaptureClient interface.
        IAudioCaptureClient* m_pCaptureClient;

        // Queue to accumulate the read audio data.
        std::vector<AudioCH2F> m_audioData;

    protected:
        // Sample rate (Hz)
        DWORD m_dwSamplesPerSec;

        // Mutex for synchronizing access to the queue
        std::mutex m_mtx;

        // Condition variable for signaling between threads
        std::condition_variable m_cv;

        // Message ID
        int m_nMessageID;

        // Audio frame batch size equal to sample size for further processing
        size_t m_sizeBatch;

    public:

        const int AM_STARTED = 1;
        const int AM_DATASTART = 11;
        const int AM_DATAEND = 12;

        CiAudio() : m_pAudioClient(nullptr), m_pFormat(nullptr), m_pCaptureClient(nullptr), m_pCollection(nullptr), m_pDevice(nullptr),
            m_sizeAudioClientNo(-1), m_dwSamplesPerSec(0), m_nMessageID(1), m_sizeBatch(0) {
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
            //IMMDeviceCollection* pCollection = nullptr;
            hr = m_pEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &m_pCollection);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to enumerate audio endpoints");
            }

        }

        // Getter for the audio data
        std::vector<AudioCH2F> getAudioData() const {
            return m_audioData;
        }

        // Getter for the audio data size
        size_t getAudioDataSize() const {
            return m_audioData.size();
        }

        // Getter for sample rate of the audio endpoint
        DWORD getSamplesPerSec() const {
            return m_dwSamplesPerSec;
        }

        // Getter for message ID
        int getMessageID() const {
            return m_nMessageID;
        }

        // Getter for audio frame batch size
        size_t getBatchSize() const {
            return m_sizeBatch;
        }

        // Setter for audio frame batch size
        void setBatchSize(const size_t sizeBatch) {
            m_sizeBatch = sizeBatch;
        }

        // Method to get and remove the N first frames
        std::tuple<std::vector<float>, std::vector<float>> moveFirstFrames(std::size_t N) {

            std::vector<float> chAData;
            std::vector<float> chBData;

            {
                // Lock the mutex while modifying the queue
                std::lock_guard<std::mutex> lock(m_mtx);

                if (N > m_audioData.size()) {
                    N = m_audioData.size();  // Ensure we don't try to remove more frames than exist
                }

                for (std::size_t i = 0; i < N; ++i) {
                    chAData.push_back(m_audioData[i].chA);
                    chBData.push_back(m_audioData[i].chB);
                }

                m_audioData.erase(m_audioData.begin(), m_audioData.begin() + N);  // Remove the N first frames
            }

            return { chAData, chBData };
        }

        // Method to get and remove the first sample frames
        std::tuple<std::vector<float>, std::vector<float>> moveFirstSample() {

            std::vector<float> chAData;
            std::vector<float> chBData;

            {
                // Lock the mutex while modifying the queue
                std::lock_guard<std::mutex> lock(m_mtx);

                if (m_sizeBatch > m_audioData.size()) return { chAData, chBData };

                for (std::size_t i = 0; i < m_sizeBatch; ++i) {
                    chAData.push_back(m_audioData[i].chA);
                    chBData.push_back(m_audioData[i].chB);
                }

                m_audioData.erase(m_audioData.begin(), m_audioData.begin() + m_sizeBatch);  // Remove the N first frames
            }

            return { chAData, chBData };
        }

        // Activate an endpoint by index.
        void activateEndpointByIndex(size_t index) {

            HRESULT hr;

            UINT count;
            hr = m_pCollection->GetCount(&count);
            if (FAILED(hr))
                throw std::runtime_error("Failed to get a count of the devices in the device collection.");
            if (index >= count)
                throw std::out_of_range("Invalid endpoint index.");

            m_sizeAudioClientNo = index;

            hr = m_pCollection->Item(static_cast<unsigned int>(m_sizeAudioClientNo), &m_pDevice);

            // Get the stream format.
            hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pAudioClient);
            if (SUCCEEDED(hr)) {
                hr = m_pAudioClient->GetMixFormat(&m_pFormat);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to get stream format.");
                }

                // Initialize the audio client.
                hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, m_pFormat, NULL);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to initialize audio client.");
                }

                // Start the audio client.
                hr = m_pAudioClient->Start();
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to start audio client.");
                }

                // Get the capture client.
                hr = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pCaptureClient);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to get capture client.");
                }
            }
        }

        ~CiAudio() {

            if (m_pCollection != nullptr) m_pCollection->Release();
            // Uninitialize COM library using RAII.
            CoUninitialize();
        }

        std::wstring getAudioEndpointsInfo() {
            std::wstringstream info;
            UINT count;

            // Get the number of endpoints in the collection.
            HRESULT hr = m_pCollection->GetCount(&count);
            if (FAILED(hr)) {
                // Handle error.
            }

            for (UINT i = 0; i < count; ++i) {
                // Get the endpoint at the current index.
                IMMDevice* pEndpoint = nullptr;
                hr = m_pCollection->Item(i, &pEndpoint);
                if (SUCCEEDED(hr)) {
                    // Get the endpoint ID.
                    LPWSTR pwszID = nullptr;
                    hr = pEndpoint->GetId(&pwszID);
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

                    // Release the endpoint.
                    pEndpoint->Release();
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
                info << L"Waveform audio format: WAVE_FORMAT_EXTENSIBLE\n";
                info << L"Number of Channels: " << pFormatExt->Format.nChannels << L"\n";
                m_dwSamplesPerSec = pFormatExt->Format.nSamplesPerSec;
                info << L"Sample Rate: " << m_dwSamplesPerSec << L" (Hz)\n";
                info << L"Average Bytes Per Second: " << pFormatExt->Format.nAvgBytesPerSec << L" (B/s)\n";
                info << L"Block Align: " << pFormatExt->Format.nBlockAlign << L" (B)\n";
                info << L"Bits Per Sample: " << pFormatExt->Format.wBitsPerSample << L" (bit)\n";
                info << L"Size of Extra Information Appended to WAVEFORMATEX: " << pFormatExt->Format.cbSize << L" (bit)\n";

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
                m_dwSamplesPerSec = m_pFormat->nSamplesPerSec;
                info << L"Sample Rate: " << m_dwSamplesPerSec << L" (Hz)\n";
                info << L"Average Bytes Per Second: " << m_pFormat->nAvgBytesPerSec << L" (B/s)\n";
                info << L"Block Align: " << m_pFormat->nBlockAlign << L" (B)\n";
                info << L"Bits Per Sample: " << m_pFormat->wBitsPerSample << L" (bit)\n";
                info << L"Size of Extra Information Appended to WAVEFORMATEX: " << m_pFormat->cbSize << L" (bit)\n";
            }

            return info.str();
        }

        // Method to read audio data
        void readAudioData(const float fpTime = 0.1f) {

            if (m_pCaptureClient == nullptr) {
                throw std::runtime_error("Capture client is not initialized.");
            }

            if (m_dwSamplesPerSec < 1) {
                throw std::runtime_error("Sample rate of the audio endpoint < 1.");
            }

            // Target frames for time seconds
            const int targetFrames = static_cast<int> (fpTime * m_dwSamplesPerSec); // Target frames for 0.1 second
            int totalFramesRead = 0;

            m_nMessageID = AM_DATASTART;

            while (totalFramesRead <= targetFrames) {
                UINT32 packetLength = 0;
                BYTE* pData;
                UINT32 numFramesAvailable;
                DWORD flags;

                HRESULT hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to get next packet size.");
                }

                if (packetLength != 0) {
                    hr = m_pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
                    if (FAILED(hr)) {
                        throw std::runtime_error("Failed to get buffer.");
                    }

                    {
                        // Lock the mutex while modifying the queue
                        std::lock_guard<std::mutex> lock(m_mtx);

                        // Process audio data here...
                        AudioCH2F* pAudioData = reinterpret_cast<AudioCH2F*>(pData);
                        for (UINT32 i = 0; i < numFramesAvailable; ++i) {
                            m_audioData.push_back(pAudioData[i]);
                        }

                        if (m_audioData.size() >= m_sizeBatch) m_cv.notify_one();
                    }

                    hr = m_pCaptureClient->ReleaseBuffer(numFramesAvailable);
                    if (FAILED(hr)) {
                        throw std::runtime_error("Failed to release buffer.");
                    }

                    totalFramesRead += numFramesAvailable;
                }
            }

            m_nMessageID = AM_DATAEND;
            m_cv.notify_one();

        }

    };

}
