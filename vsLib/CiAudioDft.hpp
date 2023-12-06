#pragma once
#include "CiAudio.hpp"
#include "CiCLaDft.hpp"
#include <string>
#include <Windows.h>
#include <iomanip>
#include <direct.h>
#include <io.h>


namespace vi {

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
        std::string m_sFolderPath;
        std::string m_sFolderName;
        float m_fpRecordThreshold;
        int m_nDoFor;

    public:

        const int TO_CONSOLE_A = 0;
        const int TO_CSV_A = 10;

        // Constructor to initialize class variables
        CiAudioDft() : m_nIndexMinF(0), m_nIndexMaxF(0), m_dbTimeStep(0.0), m_fpFrequencyStep(0.0f), m_nDoFor(0),
            m_sFolderPath(""), m_sFolderName(""), m_fpRecordThreshold(0.0000005f) {}

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

        // Setter for m_sFolderPath
        void setFolderPath(const std::string& sFolderPath) { m_sFolderPath = sFolderPath; }

        // Getter for m_sFolderPath
        std::string getFolderPath() const { return m_sFolderPath; }

        // Getter for m_sFolderName
        std::string getFolderName() const { return m_sFolderName; }

        void getReady(const int nDoFor) {

            m_nDoFor = nDoFor;

            if (m_dwSamplesPerSec < 1) {
                throw std::runtime_error("Sample rate of the audio endpoint < 1.");
            }

            cl_int err{ 0 };

            err = m_oDft.setOpenCL();
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to initialize OpenCL resources.");

            err = m_oDft.createOpenCLKernel(static_cast<int>(m_sizeBatch), m_oDft.P1SN);
            if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to create an OpenCL kernel.");

            m_dbTimeStep = m_sizeBatch / static_cast<double>(m_dwSamplesPerSec);
            m_fpFrequencyStep = static_cast<float>(m_dwSamplesPerSec) / m_sizeBatch;

            if (m_nDoFor == TO_CSV_A)
            {
                createDataFolder();
            }

        }

        void processAudioData() {

            int nOnesideSize = m_oDft.getOnesideSize();

            // Output frequency index check
            if (m_nIndexMaxF > nOnesideSize) m_nIndexMaxF = nOnesideSize;
            if (m_nIndexMinF > m_nIndexMaxF) m_nIndexMinF = m_nIndexMaxF;

            std::vector<float> onesidePowerA(nOnesideSize);
            std::vector<float> onesidePowerB(nOnesideSize);

            if (m_nDoFor == TO_CSV_A) savePowerAsCSV_A(onesidePowerA, onesidePowerB);
            if (m_nDoFor == TO_CONSOLE_A) showPowerOnConsole_A(onesidePowerA, onesidePowerB);

            m_oDft.releaseOpenCLResources();
        }

        void showPowerOnConsole_A(std::vector<float>& onesidePowerA, std::vector<float>& onesidePowerB) {
            size_t i = 1;
            do
            {
                // Get the read audio data
                std::tuple<std::vector<float>, std::vector<float>>
                    audioData = moveFirstSample();

                if (m_sizeBatch > std::get<0>(audioData).size()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                cl_int err = m_oDft.executeOpenCLKernel(std::get<0>(audioData).data(), onesidePowerA.data());
                if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for A.");

                err = m_oDft.executeOpenCLKernel(std::get<1>(audioData).data(), onesidePowerB.data());
                if (err != CL_SUCCESS) throw OpenCLException(err, "Failed to execute an OpenCL kernel for B.");

                // Move the cursor to the beginning of the console
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

            } while (m_nMessageID == AM_DATASTART || getAudioDataSize() >= m_sizeBatch);
        }

        void savePowerAsCSV_A(std::vector<float>& onesidePowerA, std::vector<float>& onesidePowerB) {

            size_t i = 1;

            double dbFrequencyStep = static_cast<double>(m_fpFrequencyStep);

            do
            {
                // Get the read audio data
                std::tuple<std::vector<float>, std::vector<float>>
                    audioData = moveFirstSample();

                if (m_sizeBatch > std::get<0>(audioData).size()) {
                    //std::cout << "\nWrong Batch at " << i << ": " << std::get<0>(audioData).size() << "\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                std::string fileName = m_sFolderPath + "/" + oss.str() + ".csv";


                // Write to the CSV file
                FILE* file;
                err = fopen_s(&file, fileName.c_str(), "w");
                if (err == 0) {
                    fprintf(file, "Frequency,Power A,Power B\n");
                    for (int j = m_nIndexMinF; j <= m_nIndexMaxF; ++j) {
                        // Skip the record if the power of both channels is less than the threshold value.
                        if (onesidePowerA.data()[j] < m_fpRecordThreshold && onesidePowerB.data()[j] < m_fpRecordThreshold) continue;
                        fprintf(file, "%.2f,%f,%f\n", j * dbFrequencyStep, onesidePowerA.data()[j], onesidePowerB.data()[j]);
                    }
                    fclose(file);
                }
                else {
                    throw std::runtime_error("Can't open a file " + fileName + ".");
                }

                ++i;

            } while (m_nMessageID == AM_DATASTART || getAudioDataSize() >= m_sizeBatch);
        }

        // Method to create a new data folder
        void createDataFolder() {
            // Get current time
            std::time_t t = std::time(nullptr);
            std::tm now;
            localtime_s(&now, &t);

            // Create a folder named with the creation date and time YYMMDD_HHMMSS
            std::ostringstream oss;
            oss << std::put_time(&now, "%y%m%d_%H%M%S");
            m_sFolderName = oss.str();  // Update m_sFolderName
            m_sFolderPath = m_sFolderPath + "/" + m_sFolderName;  // Update m_sFolderPath
            int err = _mkdir(m_sFolderPath.c_str());
            if (err != 0) {
                throw std::runtime_error("Problem creating directory " + m_sFolderPath);
            }
        }

        // Method to delete a file in the created folder
        void deleteFile(const std::string& fileName) {
            std::string filePath = m_sFolderPath + "/" + fileName;
            if (_access(filePath.c_str(), 0) != -1) {
                remove(filePath.c_str());
            }
            else {
                throw std::runtime_error("File " + fileName + " does not exist in the directory " + m_sFolderPath);
            }
        }

        // Method to return a list of all files in the created folder
        std::vector<std::string> listFiles() {
            std::vector<std::string> files;
            struct _finddata_t fileinfo;
            intptr_t handle;

            std::string folderSearch = m_sFolderPath + "/*.*";
            if ((handle = _findfirst(folderSearch.c_str(), &fileinfo)) != -1) {
                do {
                    // Check if the item is not a directory
                    if ((fileinfo.attrib & _A_SUBDIR) == 0) {
                        files.push_back(fileinfo.name);
                    }
                } while (_findnext(handle, &fileinfo) == 0);
                _findclose(handle);
            }

            return files;
        }

        // Method to delete the created folder if the folder is empty
        void deleteFolderIfEmpty() {
            std::vector<std::string> files = listFiles();
            if (files.size() <= 2) {  // '.' and '..' are always present
                int err = _rmdir(m_sFolderPath.c_str());
            }
            else {
                throw std::runtime_error("Directory " + m_sFolderPath + " is not empty.");
            }
        }

    };

}