// NOLINTBEGIN
#include <asfproject/process_audio.h>
#include <matplot/matplot.h>

using namespace asf;

int main()
{
  using namespace matplot;

  ProcessAudio processAudio{};
  if (!processAudio.loadAudioFromFile("audio/file_example_WAV_1MG.wav")) {
    processAudio.~ProcessAudio();
    return 1;
  }

  /*
  for (int i = 0; i < processAudio.getNumSamplesPerChannel(); i++) {
    for (int channel = 0; channel < processAudio.getNumOfChannels(); channel++) {
      std::cout << processAudio.samples[size_t(channel)][size_t(i)] << "\n";
    }
  }

  std::vector<double> x_time = linspace(0, processAudio.getLengthInSeconds());

  plot(x_time, processAudio.samples[0], "-o");
  plot(x_time, processAudio.samples[1], "-o");
  
  save("waveform.svg");*/
      
  return 0;
}
// NOLINTEND
