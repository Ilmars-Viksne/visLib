#pragma once
#include "CiUser.hpp"
#include "CiCLaDft.hpp"
#include "CiAudio.hpp"

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

int goCiCLaDft()
{
    const int sampleSize = 16;
    const float samplingFrequency = 160.0;

    cl_int err{ 0 };

    CiCLaDft oDft;

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
                inputReal[i] = (float)(ampl1 * sinf(PI2 * freq1 * time) + ampl2 * sinf(PI2 * freq2 * time));
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
    catch (const OpenCLException& e) {
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
