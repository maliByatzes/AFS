#include "process_audio.h"

using namespace asf;

int main(int argc, char** argv) {    

    ProcessAudio pa{};
    pa.loadAudioFromFile("M1F1-Alaw-AFsp.wav");
    
    return 0;
}
