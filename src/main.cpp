#include <algorithm>
#include <asfproject/fft.h>
#include <asfproject/process_audio.h>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <matplot/freestanding/plot.h>
#include <matplot/util/common.h>
#include <vector>

using namespace asf;
using namespace matplot;

std::vector<double> scaleDownSamples(const std::vector<double> &samples);
void plotSamples(const std::vector<double> &samples, size_t nSamples);

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

  std::vector<double> magnitude_values(frequency_values.size());
  std::vector<double> phase_values(frequency_values.size());

  for (size_t i = 0; i < frequency_values.size(); ++i) {
    magnitude_values[i] = std::abs(frequency_values[i]);
    phase_values[i] = std::arg(frequency_values[i]);
  }

  const std::vector<double> frequency = linspace(0, prA.getSampleRate());

  plot(frequency, phase_values, "-o");

  save("phase_values.svg");

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

void plotSamples(const std::vector<double> &samples, size_t nSamples)
{
  std::vector<double> time_values(nSamples);
  for (size_t i = 0; i < nSamples; ++i) { time_values[i] = static_cast<double>(i) / double(nSamples); }

  // More down scaling to not draw everything
  const int DOWNSAMPLE_FACTOR = 1000;
  std::vector<double> downsampled_time_values;
  std::vector<double> downsampled_scaled_samples;
  for (size_t i = 0; i < nSamples; i += DOWNSAMPLE_FACTOR) {
    downsampled_time_values.push_back(time_values[i]);
    downsampled_scaled_samples.push_back(samples[i]);
  }

  stem(downsampled_time_values, downsampled_scaled_samples);
  save("stem_plot.svg");
}
