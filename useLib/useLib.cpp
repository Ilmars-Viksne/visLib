#include "CiAudio.hpp"
#include <iostream>

int main() {

    try {
        // Create an instance of CiAudio
        CiAudio audio;
        std::wcout << audio.getAudioEndpointsInfo();

        // Activate the first endpoint 1
        audio.activateEndpointByIndex(1);
        std::wcout << audio.getStreamFormatInfo();

        // Read audio data
        audio.readAudioData();

        // Wait for a while to let the child thread finish reading audio data
        //std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get the read audio data
        std::queue<float> audioData = audio.getAudioData();

        // Print data points
        size_t i = 0;
        while (!audioData.empty()) {
            std::cout << "[" << i << "] ";
            std::cout << audioData.front() << "    ";
            audioData.pop();
            std::cout << audioData.front() << "\n";
            audioData.pop();
            ++i;
        }

    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
