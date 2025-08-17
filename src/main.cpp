#include "afsproject/afs.h"
#include <afsproject/audio_engine.h>
#include <afsproject/audio_file.h>
#include <cstdlib>
#include <iostream>
#include <memory>

using namespace afs;

int main()
{
  const AudioEngine engine;

  const std::unique_ptr<IAudioFile> wave_audio = engine.loadAudioFile("audio/92002__jcveliz__violin-origional.wav");
  if (!wave_audio) {
    std::cerr << "Failed to load WAV file.\n";
    return EXIT_FAILURE;
  }

  AFS afs;
  afs.shortTimeFourierTransform(*wave_audio);

  auto filtered_matrix = afs.filtering();
    
  return 0;
}
