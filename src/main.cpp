#include <algorithm>
#include <asfproject/fft.h>
#include <asfproject/process_audio.h>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <matplot/core/figure_registry.h>
#include <matplot/freestanding/axes_functions.h>
#include <matplot/freestanding/plot.h>
#include <matplot/util/common.h>
#include <matplot/util/keywords.h>
#include <vector>

using namespace asf;
using namespace matplot;

std::vector<double> scaleDownSamples(const std::vector<double> &samples);
void plotSamples(const std::vector<double> &samples, size_t n_samples);
void plotFrequencySpectrums(VecComplexDoub &frequency_values, int sample_rate);
std::vector<double> unwrap(const std::vector<double> &phase_rad);

int main()
{

  ProcessAudio prA{};
  if (!prA.loadAudioFromFile("audio/1hertz.wav")) { return 1; }

  /*
  const std::vector<double> scaled_samples{ scaleDownSamples(prA.samples) };
  auto nSamples = size_t(prA.getNumSamplesPerChannel());

  plotSamples(scaled_samples, nSamples);*/

  const FFT fft{};

  VecComplexDoub frequency_values = fft.convertToFrequencyDomain(prA.samples);

  plotFrequencySpectrums(frequency_values, prA.getSampleRate());

  return 0;
}

std::vector<double> scaleDownSamples(const std::vector<double> &samples)
{
  if (samples.empty()) { return {}; }

  double max_sample = 0.0;

  std::ranges::for_each(samples, [&max_sample](double sample) {
    auto abs_sample = std::abs(sample);
    max_sample = std::fmax(max_sample, abs_sample);
  });

  std::vector<double> scaled_samples(samples.size());

  if (max_sample == 0.0) {
    std::ranges::for_each(scaled_samples, [](double &sample) { sample = 0.0; });
  } else {
    for (size_t i = 0; i < samples.size(); ++i) { scaled_samples[i] = samples[i] / max_sample; }
  }

  return scaled_samples;
}

void plotSamples(const std::vector<double> &samples, size_t n_samples)
{
  std::vector<double> time_values(n_samples);
  for (size_t i = 0; i < n_samples; ++i) { time_values[i] = static_cast<double>(i) / double(n_samples); }

  // More down scaling to not draw everything
  const int DOWNSAMPLE_FACTOR = 1000;
  std::vector<double> downsampled_time_values;
  std::vector<double> downsampled_scaled_samples;
  for (size_t i = 0; i < n_samples; i += DOWNSAMPLE_FACTOR) {
    downsampled_time_values.push_back(time_values[i]);
    downsampled_scaled_samples.push_back(samples[i]);
  }

  stem(downsampled_time_values, downsampled_scaled_samples);
  save("stem_plot.svg");
}

void plotFrequencySpectrums(VecComplexDoub &frequency_values, int sample_rate)
{
  size_t N = frequency_values.size();// NOLINT
  const size_t num_uniq_freq_bins = (N / 2) + 1;

  std::vector<double> magnitude_values(num_uniq_freq_bins);
  std::vector<double> phase_values(num_uniq_freq_bins);

  for (size_t i = 0; i < num_uniq_freq_bins; ++i) {
    magnitude_values[i] = std::abs(frequency_values[i]);
    phase_values[i] = std::arg(frequency_values[i]);
  }

  std::vector<double> unwrapped_phase_radians = unwrap(phase_values);

  std::vector<double> unwrapped_phase_degrees(unwrapped_phase_radians.size());
  for (size_t i = 0; i < unwrapped_phase_radians.size(); ++i) {
    unwrapped_phase_degrees[i] = unwrapped_phase_radians[i] * 180.0 / M_PI;// NOLINT
  }

  double nyquist_freq = sample_rate / 2.0;// NOLINT
  const std::vector<double> frequencies = linspace(0.0, nyquist_freq, num_uniq_freq_bins);

  figure();
  plot(frequencies, unwrapped_phase_degrees, "-");
  xlabel("Frequency (Hz)");
  ylabel("Phase (degrees)");
  title("Phase Spectrum of Audio Recording");
  grid(on);
  save("audio_phase_spectrum.svg");

  std::vector<double> magnitude_values_db(num_uniq_freq_bins);
  for (size_t i = 0; i < num_uniq_freq_bins; ++i) {
    if (magnitude_values[i] > 1e-12) {// NOLINT
      magnitude_values_db[i] = 20.0 * std::log10(magnitude_values[i]);// NOLINT
    } else {
      magnitude_values_db[i] = -200.0;// NOLINT
    }
  }

  figure();
  plot(frequencies, magnitude_values_db, "-");
  xlabel("Frequency (Hz)");
  ylabel("Magnitude (dB)");
  title("Magnitude Spectrum of Audio Recording");
  grid(on);
  save("audio_magnitude_spectrum.svg");

  show();
}

std::vector<double> unwrap(const std::vector<double> &phase_rad)
{
  //-- Helper for phase unwrapping
  if (phase_rad.empty()) { return {}; }

  std::vector<double> unwrapped = phase_rad;
  for (size_t i = 1; i < unwrapped.size(); ++i) {
    const double diff = unwrapped[i] - unwrapped[i - 1];
    if (diff > M_PI) {
      unwrapped[i] -= 2.0 * M_PI;// NOLINT
    } else if (diff < -M_PI) {
      unwrapped[i] += 2.0 * M_PI;// NOLINT
    }
  }

  return unwrapped;
}
