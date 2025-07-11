#include <asfproject/process_audio.h>

using namespace asf;

int main()
{

  ProcessAudio processAudio{};
  if (!processAudio.loadAudioFromFile("audio/file_example_WAV_1MG.wav")) {
    processAudio.~ProcessAudio();
    return 1;
  }
      
  return 0;
}
