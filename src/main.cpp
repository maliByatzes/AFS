#include "asfproject/audio_engine.h"
#include "asfproject/audio_file.h"
#include "asfproject/spectrum.h"
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <asfproject/signal.h>
#include <asfproject/wave.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

using namespace asf;

int main()
{
  /*
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

  double start = 1.2;// NOLINT
  double duration = 0.6;// NOLINT
  auto segment2 = wave2.segment(start, duration);
  // segment2.plot();

  Spectrum spectrum = segment2.makeSpectrum();
  // spectrum.plot(std::nullopt);

  // spectrum.plot(10000.0);//NOLINT

  spectrum.lowPass(3000);// NOLINT
  // spectrum.plot(10000);//NOLINT

  auto filtered = spectrum.makeWave();
  filtered.normalize();
  // filtered.apodize();
  // filtered.plot();

  segment2.normalize();
  // segment2.apodize();
  // segment2.plot();

  // Demonstrating spectral leakage and Hamming window
  std::unique_ptr<Sinusoid> signal = sinSignal(440);// NOLINT
  double duration = signal->period() * 30;// NOLINT
  Wave wave = signal->makeWave(duration);
  wave.plot();
  
  Spectrum spectrum = wave.makeSpectrum();
  spectrum.plot(800);// NOLINT



  double duration2 = signal->period() * 30.25;// NOLINT
  Wave wave2 = signal->makeWave(duration2);
  wave2.plot();

  Spectrum spec2 = wave2.makeSpectrum();
  spec2.plot(880);// NOLINT

  wave2.hamming();
  Spectrum spec3 = wave2.makeSpectrum();
  spec3.plot(880);// NOLINT*/

  const AudioEngine engine;

  std::unique_ptr<IAudioFile> wave_audio = engine.loadAudioFile("audio/92002__jcveliz__violin-origional.wav");
  if (!wave_audio) {
    std::cerr << "Failed to load WAV file.\n";
    return EXIT_FAILURE;
  }

  engine.processServerSide(*wave_audio);  

  // Plot the whole wav file wave.
  std::vector<double> pcm_data = wave_audio->getPCMData();
  const nc::NdArray<double> ys(pcm_data.begin(), pcm_data.end());

  const Wave wave(ys, wave_audio->getSampleRate());

  wave.plot();
    
  return 0;
}
