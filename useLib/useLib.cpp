#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include "goTest.hpp"
#include <iostream>
#include <conio.h>
#include <iomanip>

class CiAudioDft : public CiAudio {

private:

    int m_nIndexMinF;
    int m_nIndexMaxF;

public:

    // Constructor to initialize class variables
    CiAudioDft() : m_nIndexMinF(0), m_nIndexMaxF(0) {}

    // Setter for m_nIndexMinF and m_nIndexMaxF
    void setIndexRangeF(const int nIndexMinF, const int nIndexMaxF) {
        m_nIndexMinF = nIndexMinF; 
        m_nIndexMaxF = nIndexMaxF;
    }

    // Getter for m_nIndexMinF
    int getIndexMinF() const { return m_nIndexMinF; }

    // Getter for m_nIndexMaxF
    int getIndexMaxF() const { return m_nIndexMaxF; }

    void processAudioData(const int nSampleSize) {

        if (m_dwSamplesPerSec < 1) {
            throw std::runtime_error("Sample rate of the audio endpoint < 1.");
        }

        cl_int err{ 0 };

        // Create an instance of CiCLaDft
        CiCLaDft oDft;

        err = oDft.setOpenCL();
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to initialize OpenCL resources.");

        err = oDft.createOpenCLKernel(nSampleSize, oDft.P1SN);
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to create an OpenCL kernel.");

        std::vector<float> onesidePowerA(oDft.getOnesideSize());
        std::vector<float> onesidePowerB(oDft.getOnesideSize());

        size_t i = 1;

        double dbDt = nSampleSize / static_cast<double>(m_dwSamplesPerSec);

        do
        {
            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = moveFirstSample();


            if (nSampleSize > std::get<0>(audioData).size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            //if (nSampleSize > getAudioDataSize()) continue;

            err = oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

            err = oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

            //std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Move the cursor to the beginning of the console
            printf("\033[0;0H");
            printf("\n  Normalized One-Sided Power Spectrum after ");
            printf(" %10.6f seconds:\n", i * dbDt);
            printf("----------------------------------------------\n");
            printf(" Frequency | Index  |   Power A  |   Power B\n");
            printf("----------------------------------------------\n");

            for (int i = m_nIndexMinF; i <= m_nIndexMaxF; ++i) {
                float freq = i * static_cast<float>(m_dwSamplesPerSec) / nSampleSize;
                printf("%10.2f | %6d | %10.6f | %10.6f\n", freq, i, onesidePowerA.data()[i], onesidePowerB.data()[i]);
            }

            ++i;

        //} while (m_nMessageID == AM_DATASTART || nSampleSize >= getAudioDataSize());
        } while (m_nMessageID == AM_DATASTART);
        //} while (nSampleSize >= getAudioDataSize());


        oDft.releaseOpenCLResources();
    }
};


int main() {
    const int nSampleSize = 2048;

    try {
        // Create an instance of CiAudioDft
        CiAudioDft audio;
        audio.setIndexRangeF(0, 40);
        std::wcout << audio.getAudioEndpointsInfo();

        // Activate the first endpoint 1
        audio.activateEndpointByIndex(1);
        std::wcout << audio.getStreamFormatInfo();

        const float samplingFrequency = static_cast<float>(audio.getSamplesPerSec());
        std::cout << "\nSample Size: " << nSampleSize << "  Sampling Frequency: " << samplingFrequency << "\n\n";

        std::cout << "\nPress any key to continue . . .\n";
        int nR = _getch();  // Wait for any key press

        // Clear the console
        std::cout << "\033c";

        // Read audio data in a new thread
        std::thread t1(&CiAudioDft::readAudioData, &audio, 30.0f);
        //std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Process the audio data in a new thread
        std::thread t2(&CiAudioDft::processAudioData, &audio, nSampleSize);

        // Wait for both threads to finish
        t1.join();
        t2.join();
    }

    catch (const OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        return 1;
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
