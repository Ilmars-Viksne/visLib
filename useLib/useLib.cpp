#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include "goTest.hpp"
#include <iostream>
#include <conio.h>
#include <iomanip>

#include <ctime>
#include <direct.h>

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
    CiCLaDft m_oDft;
    double m_dbTimeStep;
    float m_fpFrequencyStep;

public:

    // Constructor to initialize class variables
    CiAudioDft() : m_nIndexMinF(0), m_nIndexMaxF(0), m_dbTimeStep(0.0), m_fpFrequencyStep(0.0f) {}

    // Setter for m_nIndexMinF and m_nIndexMaxF
    void setIndexRangeF(const int nIndexMinF, const int nIndexMaxF) {
        m_nIndexMinF = nIndexMinF;
        m_nIndexMaxF = nIndexMaxF;
    }

    // Getter for m_nIndexMinF
    int getIndexMinF() const { return m_nIndexMinF; }

    // Getter for m_nIndexMaxF
    int getIndexMaxF() const { return m_nIndexMaxF; }

    // Getter for m_dbTimeStep
    double getTimeStep() const { return m_dbTimeStep; }

    // Getter for m_fpFrequencyStep
    float getFrequencyStep() const { return m_fpFrequencyStep; }

    void processAudioData() {

        if (m_dwSamplesPerSec < 1) {
            throw std::runtime_error("Sample rate of the audio endpoint < 1.");
        }

        cl_int err{ 0 };

        err = m_oDft.setOpenCL();
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to initialize OpenCL resources.");

        err = m_oDft.createOpenCLKernel(static_cast<int>(m_sizeBatch), m_oDft.P1SN);
        if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to create an OpenCL kernel.");

        std::vector<float> onesidePowerA(m_oDft.getOnesideSize());
        std::vector<float> onesidePowerB(m_oDft.getOnesideSize());

        m_dbTimeStep = m_sizeBatch / static_cast<double>(m_dwSamplesPerSec);
        m_fpFrequencyStep = static_cast<float>(m_dwSamplesPerSec) / m_sizeBatch;

        showPowerOnConsole(onesidePowerA, onesidePowerB);
        //savePowerAsCSV(onesidePowerA, onesidePowerB);

        m_oDft.releaseOpenCLResources();
    }

    void showPowerOnConsole(std::vector<float>& onesidePowerA, std::vector<float>& onesidePowerB) {
        size_t i = 1;
        do
        {
            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = moveFirstSample();

            if (m_sizeBatch > std::get<0>(audioData).size()) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            cl_int err = m_oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

            err = m_oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

            // Move the cursor to the beginning of the console
            //printf("\033[0;0H");
            setCursorPosition(0, 0);
            printf("\n  Normalized One-Sided Power Spectrum after ");
            printf(" %10.6f seconds (frames left: %6d)\n", i * m_dbTimeStep, static_cast<int>(getAudioDataSize()));
            printf("----------------------------------------------\n");
            printf(" Frequency | Index  |   Power A  |   Power B\n");
            printf("----------------------------------------------\n");

            for (int j = m_nIndexMinF; j <= m_nIndexMaxF; ++j) {
                float freq = j * m_fpFrequencyStep;
                printf("%10.2f | %6d | %10.6f | %10.6f\n", freq, j, onesidePowerA.data()[j], onesidePowerB.data()[j]);
            }

            ++i;

        } while (m_nMessageID == AM_DATASTART);
    }

    void savePowerAsCSV(std::vector<float>& onesidePowerA, std::vector<float>& onesidePowerB) {
        size_t i = 1;

        // Get current time
        std::time_t t = std::time(nullptr);
        std::tm now;
        localtime_s(&now, &t);

        // Create a folder named with the creation date and time YYMMDD_HHMMSS
        std::ostringstream oss;
        oss << std::put_time(&now, "%y%m%d_%H%M%S");
        std::string folderName = oss.str();
        int err = _mkdir(folderName.c_str());
        if (err != 0) {
            throw std::runtime_error("Problem creating directory " + folderName);
        }

        do
        {
            // Get the read audio data
            std::tuple<std::vector<float>, std::vector<float>>
                audioData = moveFirstSample();

            if (m_sizeBatch > std::get<0>(audioData).size()) {
                //std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            cl_int err = m_oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

            err = m_oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

            // Create a file with a name that always consists of 10 symbols consisting of "dbTime = i * m_dbTimeStep" expressed in whole microseconds
            double dbTime = i * m_dbTimeStep;
            std::ostringstream oss;
            oss << std::setw(10) << std::setfill('0') << static_cast<int>(dbTime * 1e6);
            std::string fileName = folderName + "/" + oss.str() + ".csv";

            // Write to the CSV file
            std::ofstream file(fileName);
            file << "Index,Frequency,Power A,Power B\n";
            for (int j = m_nIndexMinF; j <= m_nIndexMaxF; ++j) {
                float freq = j * m_fpFrequencyStep;
                file << j << "," << freq << "," << onesidePowerA.data()[j] << "," << onesidePowerB.data()[j] << "\n";
            }
            file.close();

            ++i;

        } while (m_nMessageID == AM_DATASTART);
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


int goCiAudioConsole() {

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

