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
       std::this_thread::sleep_for(std::chrono::seconds(1));

       // Get the read audio data
       const std::vector<float>& audioData = audio.getAudioData();

       // Print data points
       for (size_t i = 0; i < audioData.size(); i += 2 ) {
           std::cout << "[" << i/2 << "] " << audioData[i] << "   " << audioData[i+1] << std::endl;
       }

    }

    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
