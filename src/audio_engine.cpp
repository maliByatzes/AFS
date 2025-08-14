#include "asfproject/spectrum.h"
#include "asfproject/wave.h"
#include <NumCpp/Functions/hamming.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/audio_engine.h>
#include <asfproject/audio_file.h>
#include <asfproject/wave_file.h>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace asf {

std::unique_ptr<IAudioFile> AudioEngine::loadAudioFile(const std::string &file_path)
{
  // NOTE: Using file extensions to determine audio file types.
  // TODO: Use file magic bytes to robustly determine the file type.

  if (file_path.ends_with(".wav")) {
    auto file = std::make_unique<WaveFile>();
    if (file->load(file_path)) { return file; }
  }

  return nullptr;
}

bool AudioEngine::saveAudioFile(const IAudioFile &audio_file, const std::string &file_path)
{
  return audio_file.save(file_path);
}

std::vector<double> AudioEngine::getPCMData(IAudioFile &audio_file) { return audio_file.getPCMData(); }

void AudioEngine::stereoToMono(IAudioFile &audio_file)
{
  // Compute simple averaging to chnage from stereo to mon
  // M(t) = (L(t) + R(t)) / 2

  if (audio_file.isStero()) {
    std::vector<double> pcm_data = audio_file.getPCMData();
    std::vector<double> avg_pcm_data{};

    for (size_t i = 0; i < pcm_data.size() - 1; i += 2) {
      avg_pcm_data.push_back((pcm_data.at(i) + pcm_data.at(i + 1)) / 2);
    }

    audio_file.setPCMData(avg_pcm_data, audio_file.getSampleRate(), 1);
  }
}

void AudioEngine::normalizePCMData(IAudioFile &audio_file)
{
  auto pcm_data = audio_file.getPCMData();

  if (pcm_data.empty()) { return; }

  double max_sample = 0.0;
  std::ranges::for_each(pcm_data, [&max_sample](double sample) {
    auto abs_sample = std::abs(sample);
    max_sample = std::fmax(max_sample, abs_sample);
  });

  std::vector<double> normalized_pcm_data(pcm_data.size());

  if (max_sample == 0.0) {
    std::ranges::for_each(normalized_pcm_data, [](double &sample) { sample = 0.0; });
  } else {
    for (size_t i = 0; i < pcm_data.size(); ++i) { normalized_pcm_data[i] = pcm_data[i] / max_sample; }
  }

  audio_file.setPCMData(normalized_pcm_data, audio_file.getSampleRate(), audio_file.getNumChannels());
}

void AudioEngine::applyLowPassFilter(IAudioFile &audio_file)
{
  std::vector<double> pcm_data = audio_file.getPCMData();

  const nc::NdArray<double> ys(pcm_data.begin(), pcm_data.end());
  Wave wave(ys, audio_file.getSampleRate());

  Spectrum spectrum = wave.makeSpectrum();
  spectrum.lowPass(5000);// NOLINT

  const Wave filtered = spectrum.makeWave();
  const std::vector<double> pcm_data2 = filtered.getYs().toStlVector();

  audio_file.setPCMData(pcm_data2, audio_file.getSampleRate(), audio_file.getNumChannels());
}

void AudioEngine::downSampling(IAudioFile &audio_file)
{
  // Downsample for sample rate @ 44100 Hz

  if (audio_file.getSampleRate() == 44100) {// NOLINT
    std::vector<double> pcm_data = audio_file.getPCMData();
    std::vector<double> new_pcm_data(pcm_data.size() / 4);

    for (size_t i = 0; i < pcm_data.size(); i += 4) { new_pcm_data.push_back(pcm_data[i]); }

    audio_file.setPCMData(new_pcm_data, audio_file.getSampleRate() / 4, audio_file.getNumChannels());
  }
}

void AudioEngine::applyHammingWindow(IAudioFile &audio_file)
{
  // use 1024
  std::vector<double> pcm_data = audio_file.getPCMData();
  nc::NdArray<double> ys(pcm_data.begin(), pcm_data.end());
  ys *= nc::hamming(512);// NOLINT
  audio_file.setPCMData(ys.toStlVector(), audio_file.getSampleRate(), audio_file.getNumChannels());
}

void AudioEngine::shortTimeFourierTransform(IAudioFile &audio_file)
{
  const std::vector<double> pcm_data = audio_file.getPCMData();

  // 1. Define the Hamming window for all calculations
  const int sample_window = 1024;
  std::vector<double> window(sample_window);

  for (size_t i = 0; i < window.size(); ++i) {
    double ans = 0.54 - (0.46 * std::cos((2.0 * M_PI * static_cast<double>(i)) / (sample_window - 1)));// NOLINT
    window[i] = ans;
  }

  // 2. Slide the window and perform calculations
  const int half_window = sample_window / 2;
  std::vector<std::vector<double>> result(half_window, std::vector<double>(half_window));
  std::cout << "...\n";

  for (size_t i = 0; i < pcm_data.size(); i += half_window) {
    std::vector<double> data_block(pcm_data.begin() + int(i), pcm_data.begin() + int(i) + half_window);
    std::vector<double> res(sample_window);

    for (size_t j = 0; j < window.size(); ++j) { res.push_back(data_block[i] * window[i]); }

    result.push_back(res);
  }

  std::cout << "[ ";
  for (const auto &res : result) {
    std::cout << "[ ";
    for (const double vvv : res) { std::cout << vvv << ", "; }
    std::cout << "], ";
  }
  std::cout << "]\n";
}

void AudioEngine::processServerSide(IAudioFile &audio_file)
{
  // 1. Stereo to Mono
  stereoToMono(audio_file);

  // 2. Low pass filter
  applyLowPassFilter(audio_file);

  // 3. Downsampling
  downSampling(audio_file);

  // 4. Hamming window
  // applyHammingWindow(audio_file);

  // 5. FFT
}

}// namespace asf
