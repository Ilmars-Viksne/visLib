#include <iostream>
#include "CiAudio.hpp"

int main() {

    try {
        CiAudio audio;
        std::wcout << audio.getAudioEndpointsInfo();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    return 0;
}
