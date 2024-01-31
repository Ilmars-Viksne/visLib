// This C++ code defines a class for handling audio capture using 
// the Windows Core Audio API. This class is designed for capturing audio data, 
// managing endpoints, and providing information about audio devices and 
// formats. Error handling is included throughout the class to handle 
// potential failures in the audio capture process.
// Author: Ilmars Viksne
// Date: December 06, 2023

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

    /// <summary>
    /// Structure to hold audio frame data with two channels.
    /// </summary>
    struct AudioCH2F {
        float chA;
        float chB;
    };

    /// <summary>
    /// Structure to hold audio frame data with one channel.
    /// </summary>
    struct AudioCH1F {
        float chA;
    };

    /// <summary>
    /// Class for handling audio capture using the Windows Core Audio API.
    /// </summary>
    template <typename T>
    class CiAudio {
    private:
        // Smart pointer for the COM multimedia device enumerator.
        std::shared_ptr<IMMDeviceEnumerator> m_pEnumerator;

        // A collection of multimedia device resources.
        IMMDeviceCollection* m_pCollection = nullptr;

        // Pointer to the WAVEFORMATEX structure that specifies the stream format.
        WAVEFORMATEX* m_pFormat;

        // Pointer to the IMMDevice interface
        IMMDevice* m_pDevice;

        // Pointer to the IAudioClient interface
        IAudioClient* m_pAudioClient;

        // Index of the audio client
        size_t m_sizeAudioClientNo;

        // Pointer to the IAudioCaptureClient interface
        IAudioCaptureClient* m_pCaptureClient;

        // Queue to accumulate the read audio data
        std::vector<T> m_audioData;

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

        // Number of audio frame channels (default 2)
        int m_nNumberOfChannels;


    public:

        /// <summary>
        /// Message ID indicating that audio capture has started.
        /// </summary>
        const int AM_STARTED = 1;

        /// <summary>
        /// Message ID indicating the start of audio data capture.
        /// </summary>
        const int AM_DATASTART = 11;

        /// <summary>
        /// Message ID indicating the end of audio data capture.
        /// </summary>
        const int AM_DATAEND = 12;

        /// <summary>
        /// Default constructor for CiAudio class.
        /// Initializes COM library and creates a multimedia device enumerator.
        /// </summary>
        CiAudio() : m_pAudioClient(nullptr), m_pFormat(nullptr), m_pCaptureClient(nullptr), m_pCollection(nullptr), m_pDevice(nullptr),
            m_sizeAudioClientNo(-1), m_dwSamplesPerSec(0), m_nMessageID(1), m_sizeBatch(0), m_nNumberOfChannels(2) {
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
            hr = m_pEnumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, &m_pCollection);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to enumerate audio endpoints");
            }

        }

        /// <summary>
        /// Destructor for CiAudio class.
        /// Releases resources, including the device collection, and uninitializes COM.
        /// </summary>
        ~CiAudio() {
            if (m_pCollection != nullptr) m_pCollection->Release();
            // Uninitialize COM library using RAII.
            CoUninitialize();
        }

        /// <summary>
        /// Gets the captured audio data.
        /// </summary>
        /// <returns>A vector containing audio frame data with two channels.</returns>
        std::vector<AudioCH2F> getAudioData() const {
            return m_audioData;
        }

        /// <summary>
        /// Gets the size of the captured audio data.
        /// </summary>
        /// <returns>The number of audio frames in the captured data.</returns>
        size_t getAudioDataSize() const {
            return m_audioData.size();
        }

        /// <summary>
        /// Gets the sample rate of the audio endpoint.
        /// </summary>
        /// <returns>The sample rate in Hertz (Hz).</returns>
        DWORD getSamplesPerSec() const {
            return m_dwSamplesPerSec;
        }

        /// <summary>
        /// Gets the current message ID.
        /// </summary>
        /// <returns>The current message ID.</returns>
        int getMessageID() const {
            return m_nMessageID;
        }

        /// <summary>
        /// Gets the batch size of audio frames for further processing.
        /// </summary>
        /// <returns>The batch size of audio frames.</returns>
        size_t getBatchSize() const {
            return m_sizeBatch;
        }

        /// <summary>
        /// Sets the batch size of audio frames for further processing.
        /// </summary>
        /// <param name="sizeBatch">The new batch size.</param>
        void setBatchSize(const size_t sizeBatch) {
            m_sizeBatch = sizeBatch;
        }

        /// <summary>
        /// Gets the number of audio channels.
        /// </summary>
        /// <returns>The number of audio channels.</returns>
        int getNumberOfChannels() const {
            return m_nNumberOfChannels;
        }

        /// <summary>
        /// Sets the number of audio channels.
        /// </summary>
        /// <param name="nNumberOfChannels">The new number of audio channels.</param>
        void setNumberOfChannels(const int nNumberOfChannels) {
            m_nNumberOfChannels = nNumberOfChannels;
        }

        /// <summary>
        /// Gets and removes the first N frames of captured audio data.
        /// </summary>
        /// <param name="N">The number of frames to retrieve and remove.</param>
        /// <returns>A tuple containing vectors of audio data for channel A and channel B.</returns>
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

        /// <summary>
        /// The function retrieves and removes the first sample frames from the captured audio data of two channels.
        /// </summary>
        /// <returns>A tuple is returned that contains vectors of audio data for both channel A and channel B.</returns>
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

        /// <summary>
        /// The function retrieves and removes the first sample frames from the captured audio data of two channels.
        /// </summary>
        /// <returns>A tuple is returned that contains a vector of audio data for a single channel.</returns>
        std::tuple<std::vector<float>> moveFirstSampleCH1() {

            std::vector<float> chAData;
            {
                // Lock the mutex while modifying the queue
                std::lock_guard<std::mutex> lock(m_mtx);

                if (m_sizeBatch > m_audioData.size()) return { chAData };

                for (std::size_t i = 0; i < m_sizeBatch; ++i) {
                    chAData.push_back(m_audioData[i].chA);
                }

                m_audioData.erase(m_audioData.begin(), m_audioData.begin() + m_sizeBatch);  // Remove the N first frames
            }

            return { chAData };
        }


        /// <summary>
        /// Activates an audio endpoint by its index.
        /// </summary>
        /// <param name="index">The index of the audio endpoint to activate.</param>
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

            // Get the stream format
            hr = m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pAudioClient);
            if (SUCCEEDED(hr)) {
                hr = m_pAudioClient->GetMixFormat(&m_pFormat);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to get stream format.");
                }

                // Initialize the audio client
                hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, m_pFormat, NULL);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to initialize audio client.");
                }

                // Start the audio client
                hr = m_pAudioClient->Start();
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to start audio client.");
                }

                // Get the capture client
                hr = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pCaptureClient);
                if (FAILED(hr)) {
                    throw std::runtime_error("Failed to get capture client.");
                }
            }
        }

        /// <summary>
        /// Gets information about all available audio endpoints.
        /// </summary>
        /// <returns>A string containing information about audio endpoints.</returns>
        std::wstring getAudioEndpointsInfo() {
            std::wstringstream info;
            UINT count;

            // Get the number of endpoints in the collection.
            HRESULT hr = m_pCollection->GetCount(&count);
            if (FAILED(hr)) {
                // Handle error
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

        /// <summary>
        /// Gets information about the stream format of the audio endpoint.
        /// </summary>
        /// <returns>A string containing information about the audio stream format.</returns>
        std::wstring getStreamFormatInfo() {
            if (m_pFormat == nullptr) {
                throw std::runtime_error("Stream format is not initialized.");
            }

            std::wstringstream info;

            if (m_pFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                WAVEFORMATEXTENSIBLE* pFormatExt = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_pFormat);
                info << L"Waveform audio format: WAVE_FORMAT_EXTENSIBLE\n";
                m_nNumberOfChannels = static_cast<int>(pFormatExt->Format.nChannels);
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

        /// <summary>
        /// Reads audio data from the capture client for a specified duration.
        /// </summary>
        /// <param name="fpTime">The duration (in seconds) for which to read audio data.</param>
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
                        T* pAudioData = reinterpret_cast<T*>(pData);
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
