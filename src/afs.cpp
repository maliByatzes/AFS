#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <afsproject/afs.h>
#include <afsproject/audio_file.h>
#include <afsproject/fft.h>
#include <afsproject/spectrum.h>
#include <afsproject/wave.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace afs {

void AFS::stereoToMono(IAudioFile &audio_file)
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

void AFS::normalizePCMData(IAudioFile &audio_file)
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

void AFS::applyLowPassFilter(IAudioFile &audio_file)
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

void AFS::downSampling(IAudioFile &audio_file)
{
  // Downsample for sample rate @ 44100 Hz

  if (audio_file.getSampleRate() == 44100) {// NOLINT
    std::vector<double> pcm_data = audio_file.getPCMData();
    std::vector<double> new_pcm_data(pcm_data.size() / 4);

    for (size_t i = 0; i < pcm_data.size(); i += 4) { new_pcm_data.push_back(pcm_data[i]); }

    audio_file.setPCMData(new_pcm_data, audio_file.getSampleRate() / 4, audio_file.getNumChannels());
  }
}

void AFS::shortTimeFourierTransform(IAudioFile &audio_file)
{
  stereoToMono(audio_file);
  applyLowPassFilter(audio_file);
  downSampling(audio_file);

  std::vector<double> pcm_data = audio_file.getPCMData();

  // 1. Define the Hamming window for all calculations
  const int sample_window = 1024;
  std::vector<double> window(sample_window);

  for (size_t i = 0; i < window.size(); ++i) {
    double ans = 0.54 - (0.46 * std::cos((2.0 * M_PI * static_cast<double>(i)) / (sample_window - 1)));// NOLINT
    window[i] = ans;
  }

  // 2. Slide the window and perform calculations
  const int half_window = sample_window / 2;
  std::vector<std::vector<double>> result{};

  int end_of_last_win = 0;
  for (size_t x = 0; x < pcm_data.size(); x += half_window) {// NOLINT
    if (x + sample_window > pcm_data.size()) {
      end_of_last_win = int(x) + (sample_window - 1);
      break;
    }
  }

  const int num_of_zeros = end_of_last_win - (int(pcm_data.size()) - 1);

  std::vector<double> zeros_arr(static_cast<size_t>(num_of_zeros));
  pcm_data.append_range(zeros_arr);// IDK if this will work

  for (size_t i = 0; i < pcm_data.size(); i += half_window) {
    const std::vector<double> data_block(pcm_data.begin() + int(i), pcm_data.begin() + int(i) + sample_window);
    std::vector<double> res(sample_window);

    for (size_t j = 0; j < window.size(); ++j) { res[j] = data_block[j] * window[j]; }

    result.push_back(res);
  }

  // 3. Apply FFT on vectors inside the result vector
  std::vector<std::vector<double>> matrix{};

  for (const auto &ys : result) {
    const VecComplexDoub hs_vec = FFT::convertToFrequencyDomain(ys);
    std::vector<double> magnitudes{};
    std::ranges::for_each(hs_vec, [&](const auto &value) { magnitudes.push_back(std::abs(value)); });
    matrix.push_back(magnitudes);
  }

  // set matrix to m_matrix
  m_matrix = matrix;
}

void AFS::filtering()
{
  // std::vector<std::vector<double>> filtered_matrix;

  for (auto &bins : m_matrix) {
    // 1. Divide the bins int logarithmic bands
    // NOLINTBEGIN
    std::vector<double> very_low_sound_bins(bins.begin(), bins.begin() + 10);
    std::cout << very_low_sound_bins.size() << "\n";
    std::vector<double> low_sound_bins(bins.begin() + 10, bins.begin() + 20);
    std::cout << low_sound_bins.size() << "\n";
    std::vector<double> low_mid_sound_bins(bins.begin() + 20, bins.begin() + 40);
    std::cout << low_mid_sound_bins.size() << "\n";
    std::vector<double> mid_sound_bins(bins.begin() + 40, bins.begin() + 80);
    std::cout << mid_sound_bins.size() << "\n";
    std::vector<double> mid_high_sound_bins(bins.begin() + 80, bins.begin() + 160);
    std::cout << mid_high_sound_bins.size() << "\n";
    std::vector<double> high_sound_bins(bins.begin() + 160, bins.begin() + 513);
    std::cout << high_sound_bins.size() << "\n";
    // NOLINTEND

    break;
  }
}

}// namespace afs
