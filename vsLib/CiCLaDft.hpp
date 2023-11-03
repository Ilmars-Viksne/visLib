// This C++ code implements the Discrete Fourier Transform(DFT) using OpenCL 
// for parallel processing. A class CiCLaDft contains member functions
// and data members to set up and execute the DFT using OpenCL.
// Author: Ilmars Viksne
// Date: October 27, 2023

#pragma once
#include <iostream>
#include <CL/cl.h>
#include <fstream>
#include <vector>

const float PI2 = 6.28319f;

/// <summary>
/// Custom exception class for OpenCL errors.
/// </summary>
class OpenCLException : public std::exception {
private:
    cl_int errorCode;
    std::string errorDescription;

public:
    /// <summary>
    /// Constructor for OpenCLException.
    /// </summary>
    /// <param name="code">OpenCL error code.</param>
    /// <param name="description">Error description.</param>
    OpenCLException(cl_int code, const std::string& description) : errorCode(code), errorDescription(description) {}

    /// <summary>
    /// Get the error message.
    /// </summary>
    const char* what() const noexcept override {
        return errorDescription.c_str();
    }

    /// <summary>
    /// Get the OpenCL error code.
    /// </summary>
    cl_int getErrorCode() const {
        return errorCode;
    }
};

/// <summary>
/// Class for performing Discrete Fourier Transform (DFT) using OpenCL.
/// </summary>
class CiCLaDft {
public:

    CiCLaDft() : m_kernelSource(""), m_platform(nullptr), m_device(nullptr), m_context(nullptr), m_commandQueue(nullptr),
        m_inputRealBuffer(nullptr), m_onesidePowerBuffer(nullptr), m_program(nullptr), m_kernel(nullptr),
        m_sampleSize{ 0 }, m_onesideSize{ 0 }, m_kernelNo{ -1 } {}

    const int P1S = 0;
    const int P1SN = 1;

    /// <summary>
    /// Initialize OpenCL resources.
    /// </summary>
    /// <returns>0 on success, 1 on failure.</returns>
    int setOpenCL() {

        cl_int err;

        err = loadKernelFromFile("dft_kernel.cl");
        if (err != CL_SUCCESS) return 1;

        err = clGetPlatformIDs(1, &m_platform, nullptr);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to load OpenCL kernel.");
        }

        err = clGetDeviceIDs(m_platform, CL_DEVICE_TYPE_GPU, 1, &m_device, NULL);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to get GPU device.");
        }

        m_context = clCreateContext(nullptr, 1, &m_device, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to create an OpenCL context.");
        }

        m_commandQueue = clCreateCommandQueue(m_context, m_device, 0, &err);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to create a command queue.");
        }

        return 0;
    }

    /// <summary>
    /// Create an OpenCL kernel for DFT computation.
    /// </summary>
    /// <param name="sampleSize">Size of the input samples.</param>
    /// <param name="kernelNo">Kernel number (P1S or P1SN).</param>
    /// <returns>0 on success, 1 on failure.</returns>
    int createOpenCLKernel(const int sampleSize, const int kernelNo) {
        cl_int err;

        if (sampleSize < 2 || sampleSize % 2 != 0) {
            throw OpenCLException(1, "The sample size must be a power of 2 or at least an even number.");
        }
        m_sampleSize = sampleSize;
        m_onesideSize = sampleSize / 2 + 1;

        if (kernelNo < 0 || kernelNo > 1) {
            throw OpenCLException(1, "No kernel functions with such number.");
        }
        m_kernelNo = kernelNo;

        m_inputRealBuffer = clCreateBuffer(m_context, CL_MEM_READ_ONLY, m_sampleSize * sizeof(float), nullptr, &err);
        m_onesidePowerBuffer = clCreateBuffer(m_context, CL_MEM_WRITE_ONLY, m_onesideSize * sizeof(float), nullptr, &err);

        if (err != CL_SUCCESS || !m_inputRealBuffer || !m_onesidePowerBuffer) {
            throw OpenCLException(err, "Failed to create OpenCL buffers.");
        }

        const char* pKernelSource = m_kernelSource.c_str();
        m_program = clCreateProgramWithSource(m_context, 1, &pKernelSource, NULL, &err);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to create an OpenCL program.");
        }

        err = clBuildProgram(m_program, 1, &m_device, NULL, NULL, NULL);
        if (err != CL_SUCCESS) {
            size_t logSize;
            clGetProgramBuildInfo(m_program, m_device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
            if (logSize > 0) {
                std::vector<char> log(logSize);
                clGetProgramBuildInfo(m_program, m_device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), NULL);
                throw OpenCLException(err, "Failed to build the OpenCL program. Build log:\n" + std::string(log.data()));
            }
            else {
                throw OpenCLException(err, "Failed to build the OpenCL program.");
            }
        }

        if (m_kernelNo == P1S) m_kernel = clCreateKernel(m_program, "dft_R1SP", &err);
        if (m_kernelNo == P1SN) m_kernel = clCreateKernel(m_program, "dft_R1SPN", &err);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to create the OpenCL kernel.");
        }

        return 0;
    }

    /// <summary>
    /// Execute the OpenCL DFT kernel.
    /// </summary>
    /// <param name="inputReal">Input real data.</param>
    /// <param name="onesidePower">Output one-sided power spectrum.</param>
    /// <returns>0 on success, 1 on failure.</returns>
    int executeOpenCLKernel(const float* inputReal, float* onesidePower) {
        cl_int err;

        err = clEnqueueWriteBuffer(m_commandQueue, m_inputRealBuffer, CL_TRUE, 0, m_sampleSize * sizeof(float), inputReal, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to write data to a buffer object in device memory.");
        }

        err = clSetKernelArg(m_kernel, 0, sizeof(cl_mem), &m_inputRealBuffer);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to set the argument value for the input buffer.");
        }

        err = clSetKernelArg(m_kernel, 1, sizeof(cl_mem), &m_onesidePowerBuffer);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to set the argument value for the output buffer.");
        }

        size_t globalWorkSize = (size_t)m_onesideSize;
        err = clEnqueueNDRangeKernel(m_commandQueue, m_kernel, 1, nullptr, &globalWorkSize, nullptr, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to enqueue the kernel for execution.");
        }

        err = clEnqueueReadBuffer(m_commandQueue, m_onesidePowerBuffer, CL_TRUE, 0, m_onesideSize * sizeof(float), onesidePower, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            throw OpenCLException(err, "Failed to read the data from the buffer object.");
        }

        return 0;
    }

    /// <summary>
    /// Release OpenCL resources.
    /// </summary>
    void releaseOpenCLResources() {
        if (m_context) clReleaseContext(m_context);
        if (m_commandQueue) clReleaseCommandQueue(m_commandQueue);
        if (m_inputRealBuffer) clReleaseMemObject(m_inputRealBuffer);
        if (m_onesidePowerBuffer) clReleaseMemObject(m_onesidePowerBuffer);
        if (m_program) clReleaseProgram(m_program);
        if (m_kernel) clReleaseKernel(m_kernel);
    }

    /// <summary>
    /// Get the size of the one-sided power spectrum.
    /// </summary>
    /// <returns>One-sided power spectrum size.</returns>
    int getOnesideSize() const { return m_onesideSize; }

    /// <summary>
    /// Get the kernel number.
    /// </summary>
    /// <returns>Kernel number (P1S or P1SN).</returns>
    int getKernelNo() const { return m_kernelNo; }

private:
    std::string m_kernelSource;
    cl_platform_id m_platform;
    cl_device_id m_device;
    cl_context m_context;
    cl_command_queue m_commandQueue;
    cl_mem m_inputRealBuffer;
    cl_mem m_onesidePowerBuffer;
    cl_program m_program;
    cl_kernel m_kernel;

    int m_sampleSize;
    int m_onesideSize;
    int m_kernelNo;

    /// <summary>
    /// Load OpenCL kernel source code from a file.
    /// </summary>
    /// <param name="fileName">Name of the kernel source file.</param>
    /// <returns>0 on success, 1 on failure.</returns>
    int loadKernelFromFile(const std::string& fileName) {
        std::ifstream file(fileName);
        if (!file.is_open()) {
            std::cerr << "Failed to open the kernel file." << std::endl;
            return 1;
        }
        m_kernelSource.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return 0;
    }

};
