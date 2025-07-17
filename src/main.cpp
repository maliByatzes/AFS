#include <asfproject/process_audio.h>
#include <cstddef>
#include <matplot/freestanding/plot.h>
#include <vector>

using namespace asf;
using namespace matplot;

int main()
{

  ProcessAudio prA{};
  if (!prA.loadAudioFromFile("audio/1hertz.wav")) { return 1; }

  const double MAX_INT16_VALUE = 32767.0;

  std::vector<double> scaled_samples(size_t(prA.getNumSamplesPerChannel()));
  for (size_t i = 0; i < size_t(prA.getNumSamplesPerChannel()); ++i) {
    scaled_samples[i] = static_cast<double>(prA.samples[i]) / MAX_INT16_VALUE;
  }

  std::vector<double> time_values(size_t(prA.getNumSamplesPerChannel()));
  for (int i = 0; i < prA.getNumSamplesPerChannel(); ++i) {
    time_values[size_t(i)] = static_cast<double>(i) / prA.getNumSamplesPerChannel();
  }

  const int DOWNSAMPLE_FACTOR = 1000;
  std::vector<double> downsampled_time_values;
  std::vector<double> downsampled_scaled_samples;
  for (int i = 0; i < prA.getNumSamplesPerChannel(); i += DOWNSAMPLE_FACTOR) {
    downsampled_time_values.push_back(time_values[size_t(i)]);
    downsampled_scaled_samples.push_back(scaled_samples[size_t(i)]);
  }

  plot(downsampled_time_values, downsampled_scaled_samples, "-x");
  save("stem_plot.svg");

  return 0;
}
