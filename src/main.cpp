#include "process_audio.h"

using namespace asf;

int main(int argc, char** argv) {    

    ProcessAudio<float> pa {};
    if (!pa.loadAudioFromFile("file_example_WAV_1MG.wav")) {
      pa.~ProcessAudio<float>();
      return 1;
    }
    
    return 0;
}
