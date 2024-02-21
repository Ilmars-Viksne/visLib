#include "CiAudioDft.hpp"
#include "goTest.hpp"


int main() {

    int k = 5;

    if (k == 5) return goCiAudioCSVMono();
    if (k == 4) return goCiAudioConsoleMono();
    if (k == 1) return goCiAudioConsole();
    if (k == 2) return goCiAudioCSV();
    if (k == 3) return goCiAudioV01();

    return 0;
}

