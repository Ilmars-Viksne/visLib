#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include "goTest.hpp"
#include <iostream>
#include <conio.h>
#include <iomanip>

int main() {

    const int nSampleSize = 2048;

    cl_int err{ 0 };
    CiCLaDft oDft;

    try {

        // Create an instance of CiAudio
        CiAudio audio;
        std::wcout << audio.getAudioEndpointsInfo();

        // Activate the first endpoint 1
        audio.activateEndpointByIndex(1);
        std::wcout << audio.getStreamFormatInfo();

        const float samplingFrequency = static_cast<float>(audio.getSamplesPerSec());
        std::cout << "\nSample Size: " << nSampleSize << "  Sampling Frequency: "  << samplingFrequency << "\n\n";

        std::cout << "\nPress any key to continue . . .\n";
        int nR = _getch();  // Wait for any key press

        // Clear the console
        std::cout << "\033c";

        // Read audio data
        audio.readAudioData(2.0f);

        err = oDft.setOpenCL();
        if (err != CL_SUCCESS) return 1;

        err = oDft.createOpenCLKernel(nSampleSize, oDft.P1SN);
        if (err != CL_SUCCESS) return 1;

        std::vector<float> inputReal(nSampleSize);
        std::vector<float> onesidePower(oDft.getOnesideSize());

        size_t i = 1;

        while (nSampleSize <= audio.getAudioDataSize())
        {

            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = audio.moveFirstFrames(nSampleSize);

            inputReal = std::get<0>(audioData);

            err = oDft.executeOpenCLKernel(inputReal.data(), onesidePower.data());
            if (err != CL_SUCCESS) {
                oDft.releaseOpenCLResources();
                return 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Move the cursor to the beginning of the console
            std::cout << "\033[0;0H";
            if (oDft.getKernelNo() == oDft.P1S) std::cout << "\nOne-Sided Power Spectrum after ";
            if (oDft.getKernelNo() == oDft.P1SN) std::cout << "\nNormalized One-Sided Power Spectrum after ";
            std::cout << std::fixed << std::setprecision(6) << nSampleSize /samplingFrequency * i << " seconds:\n";
            printPowerRange(onesidePower.data(), nSampleSize, samplingFrequency, 0, 40);

            ++i;

        }

        oDft.releaseOpenCLResources();

    }

    catch (const OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        oDft.releaseOpenCLResources();
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
