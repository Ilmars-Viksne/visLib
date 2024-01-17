#include "CiAudioDft.hpp"
#include "goTest.hpp"


int main() {

    int k = 3;

    if (k == 1) return goCiAudioConsole();
    if (k == 2) return goCiAudioCSV();
    if (k == 3) return goCiAudioV01();

    return 0;
}

