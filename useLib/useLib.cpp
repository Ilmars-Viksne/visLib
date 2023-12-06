#include "CiAudioDft.hpp"
#include "goTest.hpp"


int main() {

    int k = 2;

    if(k == 1) return goCiAudioConsole();

    try {

        // Create an instance of CiAudioDft
        vi::CiAudioDft audio;

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

        audio.setIndexRangeF(static_cast<int>(std::floor(fpMinF/ fpFrequencyStep)), 
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
        std::thread t1(&vi::CiAudioDft::readAudioData, &audio, fpTime);

        // Process the audio data in a new thread
        std::thread t2(&vi::CiAudioDft::processAudioData, &audio);

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

