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
