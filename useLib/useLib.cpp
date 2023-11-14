#include <iostream>
#include "CiAudio.hpp"

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
       std::this_thread::sleep_for(std::chrono::seconds(3));

       // Get the read audio data
       const std::vector<float>& audioData = audio.getAudioData();

       // Print the first 10 data points
       for (size_t i = 0; i < 10 && i < audioData.size(); i++) {
           std::cout << "Data point " << i << ": " << audioData[i] << std::endl;
       }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
