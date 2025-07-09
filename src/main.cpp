#include "process_audio.h"

using namespace asf;

int main(int argc, char** argv) {    

    ProcessAudio<float> pa {};
    if (!pa.loadAudioFromFile("M1F1-Alaw-AFsp.wav")) {
      pa.~ProcessAudio<float>();
      return 1;
    }
    
    return 0;
}
