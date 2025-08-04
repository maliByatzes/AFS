#include <asfproject/signal.h>

using namespace asf;

int main()
{
  auto cos_sig = cosSignal(440, 1.0, 0);//NOLINT
  auto sin_sig = sinSignal(880, 0.5, 0);//NOLINT
  
  auto mixed_sig = *cos_sig + *sin_sig;

  mixed_sig->plot();
  /*
  const AudioEngine engine;

  std::unique_ptr<IAudioFile> wave_audio = engine.loadAudioFile("audio/4k_hertz_stereo.wav");
  if (!wave_audio) {
    std::cerr << "Failed to load WAV file.\n";
    return EXIT_FAILURE;
  }

  // Step 1: Change stereo to mono by simple averaging
  engine.stereoToMono(*wave_audio);
  
  // Step 2: Apply low pass filter*/

  return 0;
}
