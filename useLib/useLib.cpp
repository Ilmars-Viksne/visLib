#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include "goTest.hpp"
#include <iostream>
#include <conio.h>
#include <iomanip>

void processAudioData(CiAudio& audio, const int nSampleSize, const float samplingFrequency) {
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

    while (nSampleSize <= audio.getAudioDataSize())
    {
        // Get the read audio data
        std::tuple<std::vector<float>, std::vector<float>>
            audioData = audio.moveFirstFrames(nSampleSize);

        err = oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

        err = oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Move the cursor to the beginning of the console
        std::cout << "\033[0;0H";
        std::cout << "\n   Normalized One-Sided Power Spectrum after ";
        std::cout << std::fixed << std::setprecision(6) << nSampleSize / samplingFrequency * i << " seconds:\n";
        printPowerRange2Ch(onesidePowerA.data(), onesidePowerB.data(), nSampleSize, samplingFrequency, 0, 40);

        ++i;
    }
    oDft.releaseOpenCLResources();
}

int main() {
    const int nSampleSize = 2048;

    try {
        // Create an instance of CiAudio
        CiAudio audio;
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
        std::thread t1(&CiAudio::readAudioData, &audio, 30.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Process the audio data in a new thread
        std::thread t2(processAudioData, std::ref(audio), nSampleSize, samplingFrequency);

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
