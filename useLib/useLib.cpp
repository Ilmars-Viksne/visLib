#include <iostream>
#include "CiAudio.hpp"

int main() {

    try {
        CiAudio audio;
        std::wcout << audio.getAudioEndpointsInfo();
        audio.activateEndpointByIndex(1);
        std::wcout << audio.getStreamFormatInfo();

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
