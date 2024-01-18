#pragma once
#include "CiUser.hpp"
#include "CiCLaDft.hpp"
#include "CiAudio.hpp"

#include <conio.h>
#include <iomanip>

/// <summary>
/// Print the power spectrum to the console.
/// </summary>
/// <param name="spectrumPower">Array of spectrum power values.</param>
/// <param name="sampleSize">Size of the sample.</param>
/// <param name="samplingFrequency">Sampling frequency.</param>
void printPower(const float* spectrumPower, const int sampleSize, const float samplingFrequency) {
    printf("-----------------------------------\n");
    printf(" Frequency | Index  |    Power   \n");
    printf("-----------------------------------\n");

    for (int i = 0; i <= sampleSize / 2; ++i) {
        float freq = i * samplingFrequency / sampleSize;
        printf("%10.2f | %6d | %10.4f\n", freq, i, spectrumPower[i]);
    }
}

void printPowerRange(const float* spectrumPower, const int sampleSize, const float samplingFrequency, int nMin, int nMax) {
    printf("-----------------------------------\n");
    printf(" Frequency | Index  |    Power   \n");
    printf("-----------------------------------\n");

    for (int i = nMin; i <= nMax; ++i) {
        float freq = i * samplingFrequency / sampleSize;
        printf("%10.2f | %6d | %10.6f\n", freq, i, spectrumPower[i]);
    }
}

void printPowerRange2Ch(const float* spectrumPowerA, const float* spectrumPowerB, const int sampleSize, const float samplingFrequency, int nMin, int nMax) {
    printf("----------------------------------------------\n");
    printf(" Frequency | Index  |   Power A  |   Power B\n");
    printf("----------------------------------------------\n");

    for (int i = nMin; i <= nMax; ++i) {
        float freq = i * samplingFrequency / sampleSize;
        printf("%10.2f | %6d | %10.6f | %10.6f\n", freq, i, spectrumPowerA[i], spectrumPowerB[i]);
    }
}



int goCiAudioCSV() {

    try {

        // Create an instance of CiAudioDft
        vi::CiAudioDft<vi::AudioCH2F> audio;

        // Set endpoint number
        int endpointNumber = 1;

        // Activate the endpoint
        audio.activateEndpointByIndex(endpointNumber);

        audio.getStreamFormatInfo();

        // Set sample size
        int nSampleSize = 2048;
        audio.setBatchSize(nSampleSize);

        // Set audio recording time
        float fpTime = 5.f;

        // Frequency range of interest
        float fpMinF = 0.f;
        float fpMaxF = 24000.f;

        audio.setFolderPath("E:/Test_Data");

        float fpFrequencyStep = static_cast<float>(audio.getSamplesPerSec()) / nSampleSize;

        audio.setIndexRangeF(static_cast<int>(std::floor(fpMinF / fpFrequencyStep)),
            static_cast<int>(std::ceil(fpMaxF / fpFrequencyStep)));

        std::cout << "Calculation duration in seconds: " << fpTime << "\n";
        std::cout << "Sample size:: " << audio.getBatchSize() << "\n";
        std::cout << "Index range of displayed frequencies is from "
            << audio.getIndexMinF() << " to " << audio.getIndexMaxF() << "\n";
        std::cout << "Data folder location: " << audio.getFolderPath() << "\n";

        audio.getReady(audio.TO_CSV_A);

        std::cout << "\nPress any key to start the calculation or 'Esc' to exit . . .\n";
        int nR = _getch();  // Wait for any key press
        // Check if the key pressed was 'Esc'
        if (nR == 27) {
            return 0;  // Exit the program
        }
        std::cout << "\n\tCalculation in progress ...\n";

        // Read audio data in a new thread
        std::thread t1(&vi::CiAudioDft<vi::AudioCH2F>::readAudioData, &audio, fpTime);

        // Process the audio data in a new thread
        std::thread t2(&vi::CiAudioDft<vi::AudioCH2F>::processAudioData, &audio);

        // Wait for both threads to finish
        t2.join();
        t1.join();

        std::cout << "\n\tThe calculation is complete.\n\n";
        std::cout << "Number of unprocessed audio frames: " << audio.getAudioDataSize() << "\n";

        // Print the file names
        std::cout << "\n\tList of saved files in folder " << audio.getFolderPath() << "\n";
        std::vector<std::string> fileList = audio.listFiles();
        for (const std::string& file : fileList) {
            std::cout << file << "\n";
        }

        std::cout << "\nPress 'Y' to delete files or any other key to continue: ";
        // Read a character from the console
        int key = getchar();
        // Check if the pressed key is 'Y'
        if (key == 'Y' || key == 'y') {
            // Delete the filees
            for (const std::string& file : fileList) {
                audio.deleteFile(file);
            }
            audio.deleteFolderIfEmpty();
        }

    }

    catch (const vi::OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        return 1;
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    std::cout << "\n\n\tPress any key to end . . .\n";
    int nR = _getch();  // Wait for any key press

    return 0;
}




int goCiAudioConsole() {

    try {
        // Create an instance of CiAudioDft
        vi::CiAudioDft<vi::AudioCH2F> audio;
        std::cout << "\n\tAvailable audio endpoints:\n\n";
        std::wcout << audio.getAudioEndpointsInfo();
        std::cout << "\t---------------------\n\n";

        // Get endpoint number from user
        int endpointNumber;
        std::cout << "Enter the endpoint index number (starting from 0): ";
        std::cin >> endpointNumber;

        // Activate the endpoint
        audio.activateEndpointByIndex(endpointNumber);
        std::cout << '\n';
        std::wcout << audio.getStreamFormatInfo();
        std::cout << '\n';

        // Get sample size from user
        std::cin.ignore();
        int nSampleSize = vi::getNumberFromInput<int>("Enter the sample size as a 2^n number (default 2048): ", 2048);
        std::cout << "Sample size:: " << nSampleSize << "\n\n";
        audio.setBatchSize(nSampleSize);

        // Get time from user
        float fpTime = vi::getNumberFromInput<float>("Enter the duration of the calculation in seconds (default 10): ", 10.f);
        std::cout << "Calculation duration in seconds: " << fpTime << "\n\n";

        int nIndexMinF = vi::getNumberFromInput<int>("Index of the lower limit of the displayed frequency range (default 0): ", 0);
        int nIndexMaxF = vi::getNumberFromInput<int>("Index of the upper limit of the displayed frequency range (default 40): ", 40);
        std::cout << "Index range of displayed frequencies is from " << nIndexMinF << " to " << nIndexMaxF << "\n\n";
        audio.setIndexRangeF(nIndexMinF, nIndexMaxF);

        audio.getReady(audio.TO_CONSOLE_A);

        std::cout << "\n Press any key to start the calculation or 'Esc' to exit . . .\n";
        int nR = _getch();  // Wait for any key press

        // Check if the key pressed was 'Esc'
        if (nR == 27) {
            return 0;  // Exit the program
        }

        // Clear the console
        vi::clearConsole();

        // Read audio data in a new thread
        std::thread t1(&vi::CiAudioDft<vi::AudioCH2F>::readAudioData, &audio, fpTime);

        // Process the audio data in a new thread
        std::thread t2(&vi::CiAudioDft<vi::AudioCH2F>::processAudioData, &audio);

        // Wait for both threads to finish
        t2.join();
        t1.join();
    }

    catch (const vi::OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        return 1;
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    std::cout << "\n Press any key to end . . .\n";
    int nR = _getch();  // Wait for any key press

    return 0;
}

int goCiAudioStereo() {

    const int nSampleSize = 2048;
    cl_int err{ 0 };
    vi::CiCLaDft oDft;

    try {
        // Create an instance of CiAudio
        vi::CiAudioDft<vi::AudioCH2F> audio;
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

        // Read audio data
        audio.readAudioData(2.0f);

        err = oDft.setOpenCL();
        if (err != CL_SUCCESS) return 1;

        err = oDft.createOpenCLKernel(nSampleSize, oDft.P1SN);
        if (err != CL_SUCCESS) return 1;

        std::vector<float> onesidePowerA(oDft.getOnesideSize());
        std::vector<float> onesidePowerB(oDft.getOnesideSize());

        size_t i = 1;

        while (nSampleSize <= audio.getAudioDataSize())
        {
            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = audio.moveFirstFrames(nSampleSize);

            err = oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
            if (err != CL_SUCCESS) {
                oDft.releaseOpenCLResources();
                return 1;
            }

            err = oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
            if (err != CL_SUCCESS) {
                oDft.releaseOpenCLResources();
                return 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Move the cursor to the beginning of the console
            std::cout << "\033[0;0H";
            std::cout << "\n   Normalized One-Sided Power Spectrum after ";
            std::cout << std::fixed << std::setprecision(6) << nSampleSize / samplingFrequency * i << " seconds:\n";
            printPowerRange2Ch(onesidePowerA.data(), onesidePowerB.data(), nSampleSize, samplingFrequency, 0, 40);

            ++i;
        }
        oDft.releaseOpenCLResources();
    }

    catch (const vi::OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        oDft.releaseOpenCLResources();
        return 1;
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}

int goCiAudioV01() {

    try {
        // Create an instance of CiAudio
        vi::CiAudioDft<vi::AudioCH2F> audio;
        std::wcout << audio.getAudioEndpointsInfo();

        // Activate the first endpoint 1
        audio.activateEndpointByIndex(1);
        std::wcout << audio.getStreamFormatInfo();

        // Read audio data
        audio.readAudioData(0.1f);

        size_t nSapleSize = 480;

        size_t i = 1;

        while (nSapleSize <= audio.getAudioDataSize())
        {
            //std::cout << "\nNumber of frames: " << audio.getAudioDataSize() << "\n";

            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = audio.moveFirstFrames(nSapleSize);

            // Print data points
            for (size_t j = 0; j < nSapleSize; j++)
            {
                std::cout << "[" << i << "] ";
                std::cout << std::get<0>(audioData)[j] << "    " << std::get<1>(audioData)[j] << "\n";
                ++i;
            }
        }
    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}

int goCiCLaDft()
{
    const int sampleSize = 16;
    const float samplingFrequency = 160.0;

    cl_int err{ 0 };

    vi::CiCLaDft oDft;

    try
    {

        err = oDft.setOpenCL();
        if (err != CL_SUCCESS) return 1;

        err = oDft.createOpenCLKernel(sampleSize, oDft.P1SN);
        if (err != CL_SUCCESS) return 1;

        std::vector<float> inputReal(sampleSize);
        std::vector<float> onesidePower(oDft.getOnesideSize());

        for (int seconds = 1; seconds <= 5; seconds++) {
            float freq1 = 10.0f * seconds;
            float freq2 = 90.0f - 10.0f * seconds;
            float ampl1 = 1.0f;
            float ampl2 = 5.0f;

            for (int i = 0; i < sampleSize; i++) {
                float time = (float)i / samplingFrequency;
                inputReal[i] = (float)(ampl1 * sinf(vi::PI2 * freq1 * time) + ampl2 * sinf(vi::PI2 * freq2 * time));
            }

            err = oDft.executeOpenCLKernel(inputReal.data(), onesidePower.data());
            if (err != CL_SUCCESS) {
                oDft.releaseOpenCLResources();
                return 1;
            }
            if (oDft.getKernelNo() == oDft.P1S) std::cout << "\nOne-Sided Power Spectrum after ";
            if (oDft.getKernelNo() == oDft.P1SN) std::cout << "\nNormalized One-Sided Power Spectrum after ";
            std::cout << seconds << " seconds:\n";
            printPower(onesidePower.data(), sampleSize, samplingFrequency);

        }

        oDft.releaseOpenCLResources();

    }
    catch (const vi::OpenCLException& e) {
        std::cerr << "OpenCL Error: " << e.what() << " (Error Code: " << e.getErrorCode() << ")" << std::endl;
        oDft.releaseOpenCLResources();
        return 1;
    }

    return 0;
}

int goCiUser()
{
    // Create an instance of the CiUser class
    vi::CiUser user;

    // Set the user information
    user.setUserId(1);
    user.setUserName("John Doe");
    user.setUserEmail("johndoe@example.com");

    // Print out the user information
    user.printUserInfo();

    return 0;
}
