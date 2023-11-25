#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include "goTest.hpp"
#include <iostream>
#include <conio.h>
#include <iomanip>

template <typename T>
T getNumberFromInput(const std::string& prompt, T defaultValue) {
    T number;

    // Prompt the user for input
    std::cout << prompt;

    // Try to read the input
    std::string input;
    std::getline(std::cin, input);

    // Check if input is empty
    if (input.empty()) {
        // If input is empty, assign a default value
        number = defaultValue;
    }
    else {
        // Try to convert the input to the specified type
        try {
            size_t pos;
            number = static_cast<T> (std::stod(input, &pos));

            // Check if the entire input was used in the conversion
            if (pos != input.size()) {
                throw std::invalid_argument("");
            }
        }
        catch (std::invalid_argument&) {
            // If conversion fails, assign a default value
            number = defaultValue;
        }
    }

    return number;
}

void clearConsole() {
    // Get the console handle
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to get console handle");
    }

    // Get the size of the console window
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        throw std::runtime_error("Failed to get console buffer info");
    }
    DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;

    // Fill the entire screen with blanks
    DWORD charsWritten;
    if (!FillConsoleOutputCharacter(hConsole, (TCHAR)' ', consoleSize, { 0, 0 }, &charsWritten)) {
        throw std::runtime_error("Failed to fill console output character");
    }

    // Get the current text attribute
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        throw std::runtime_error("Failed to get console buffer info");
    }

    // Set the buffer's attributes accordingly
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, consoleSize, { 0, 0 }, &charsWritten)) {
        throw std::runtime_error("Failed to fill console output attribute");
    }

    // Put the cursor at (0, 0)
    if (!SetConsoleCursorPosition(hConsole, { 0, 0 })) {
        throw std::runtime_error("Failed to set console cursor position");
    }
}

void setCursorPosition(int x, int y) {
    // Get the console handle
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to get console handle");
    }

    // Set the cursor position
    COORD coord;
    coord.X = x;
    coord.Y = y;
    if (!SetConsoleCursorPosition(hConsole, coord)) {
        throw std::runtime_error("Failed to set console cursor position");
    }
}

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
            //printf("\033[0;0H");
            setCursorPosition(0, 0);
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
        int nSampleSize = getNumberFromInput<int>("Enter the sample size as a 2^n number (default 2048): ", 2048);
        std::cout << "Sample size:: " << nSampleSize << "\n\n";
        audio.setBatchSize(nSampleSize);

        // Get time from user
        float fpTime = getNumberFromInput<float>("Enter the duration of the calculation in seconds (default 10): ", 10.f);
        std::cout << "Calculation duration in seconds: " << fpTime << "\n\n";

        int nIndexMinF = getNumberFromInput<int>("Index of the lower limit of the displayed frequency range (default 0): ", 0);
        int nIndexMaxF = getNumberFromInput<int>("Index of the upper limit of the displayed frequency range (default 40): ", 40);
        std::cout << "Index range of displayed frequencies is from " << nIndexMinF << " to " << nIndexMaxF << "\n\n";
        audio.setIndexRangeF(nIndexMinF, nIndexMaxF);

        std::cout << "\n Press any key to start the calculation or 'Esc' to exit . . .\n";
        int nR = _getch();  // Wait for any key press

        // Check if the key pressed was 'Esc'
        if (nR == 27) {
            return 0;  // Exit the program
        }

        // Clear the console
        //std::cout << "\033c";
        clearConsole();

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

    std::cout << "\n Press any key to end . . .\n";
    int nR = _getch();  // Wait for any key press

    return 0;
}
