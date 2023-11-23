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

    void processAudioData() {

        if (m_dwSamplesPerSec < 1) {
            throw std::runtime_error("Sample rate of the audio endpoint < 1.");
        }

        cl_int err{ 0 };

        // Create an instance of CiCLaDft
        CiCLaDft oDft;

        err = oDft.setOpenCL();
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to initialize OpenCL resources.");

        err = oDft.createOpenCLKernel(static_cast<int>(m_sizeBatch), oDft.P1SN);
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to create an OpenCL kernel.");

        std::vector<float> onesidePowerA(oDft.getOnesideSize());
        std::vector<float> onesidePowerB(oDft.getOnesideSize());

        size_t i = 1;

        double dbDt = m_sizeBatch / static_cast<double>(m_dwSamplesPerSec);

        do
        {
            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = moveFirstSample();

            if (m_sizeBatch > std::get<0>(audioData).size()) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            err = oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

            err = oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

            // Move the cursor to the beginning of the console
            printf("\033[0;0H");
            printf("\n  Normalized One-Sided Power Spectrum after ");
            printf(" %10.6f seconds (frames left: %6d)\n", i * dbDt, static_cast<int>(getAudioDataSize()));
            printf("----------------------------------------------\n");
            printf(" Frequency | Index  |   Power A  |   Power B\n");
            printf("----------------------------------------------\n");

            for (int j = m_nIndexMinF; j <= m_nIndexMaxF; ++j) {
                float freq = j * static_cast<float>(m_dwSamplesPerSec) / m_sizeBatch;
                printf("%10.2f | %6d | %10.6f | %10.6f\n", freq, j, onesidePowerA.data()[j], onesidePowerB.data()[j]);
            }

            ++i;

        } while (m_nMessageID == AM_DATASTART);

        oDft.releaseOpenCLResources();
    }
};

int main() {


    try {
        // Create an instance of CiAudioDft
        CiAudioDft audio;
        std::wcout << audio.getAudioEndpointsInfo();

        // Get endpoint number from user
        int endpointNumber;
        std::cout << "Please enter the endpoint number: ";
        std::cin >> endpointNumber;

        // Activate the endpoint
        audio.activateEndpointByIndex(endpointNumber);
        std::wcout << audio.getStreamFormatInfo();

        // Get sample size from user
        int nSampleSize;
        std::cout << "Please enter the sample size (2^n): ";
        std::cin >> nSampleSize;

        // Get time from user
        float fpTime;
        std::cout << "Please enter the time: ";
        std::cin >> fpTime;

        audio.setBatchSize(nSampleSize);
        audio.setIndexRangeF(0, 40);

        const float samplingFrequency = static_cast<float>(audio.getSamplesPerSec());
        std::cout << "\nSample Size: " << nSampleSize << "  Sampling Frequency: " << samplingFrequency << "\n";

        std::cout << "\nPress any key to continue, or 'Esc' to exit . . .\n";
        int nR = _getch();  // Wait for any key press

        // Check if the key pressed was 'Esc'
        if (nR == 27) {
            return 0;  // Exit the program
        }

        // Clear the console
        std::cout << "\033c";

        // Read audio data in a new thread
        std::thread t1(&CiAudioDft::readAudioData, &audio, fpTime);

        // Process the audio data in a new thread
        std::thread t2(&CiAudioDft::processAudioData, &audio);

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
