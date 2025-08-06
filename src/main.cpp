#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <asfproject/audio_engine.h>
#include <asfproject/audio_file.h>
#include <asfproject/signal.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace asf;

int main()
{
  auto cos_sig = cosSignal(440, 1.0, 0);// NOLINT
  auto sin_sig = sinSignal(880, 0.5, 0);// NOLINT

  auto mixed_sig = *cos_sig + *sin_sig;
  auto wave = mixed_sig->makeWave(0.5, 0, 11025);// NOLINT

  // std::cout << "Number of samples: " << wave.getYs().size() << "\n";
  // double timestep = 1.0 / wave.getFramerate() * 1000.0;//NOLINT
  // std::cout << "Timestep in ms: " << timestep << "\n";// NOLINT

  const double period = mixed_sig->period();
  auto segment = wave.segment(0, period * 3);

  // segment.plot();

  wave.normalize();
  // wave.apodize();
  // wave.plot();

  // TODO: Fix `wave.write()` call
  // const std::string filename("audio/temp.wav");
  // wave.write(filename);

  const AudioEngine engine;
  
  std::unique_ptr<IAudioFile> wave_audio = engine.loadAudioFile("audio/92002__jcveliz__violin-origional.wav");
  if (!wave_audio) {
    std::cerr << "Failed to load WAV file.\n";
    return EXIT_FAILURE;
  }

  std::vector<double> pcm_data = wave_audio->getPCMData();

  const nc::NdArray<double> ys(pcm_data.begin(), pcm_data.end());
  const Wave wave2(ys, wave_audio->getSampleRate());

  double start = 1.2;//NOLINT
  double duration = 0.6;//NOLINT
  auto segment2 = wave2.segment(start, duration);
  segment2.plot();
  
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
