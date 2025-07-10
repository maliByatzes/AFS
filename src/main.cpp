#include <asfproject/process_audio.h>
#include <iostream>
// #include <matplot/matplot.h>

using namespace asf;
// namespace plt = matplot;

int main()
{

  ProcessAudio<float> processAudio{};
  if (!processAudio.loadAudioFromFile("audio/file_example_WAV_1MG.wav")) {
    processAudio.~ProcessAudio<float>();
    return 1;
  }

  std::cout << "That summary bruh:\n";
  std::cout << "Num of channels: " << processAudio.getNumOfChannels() << "\n";
  std::cout << "Num Samples Per Channel: " << processAudio.getNumSamplesPerChannel() << "\n";
  std::cout << "Bit Depth: " << processAudio.getBitDepth() << "\n";
  std::cout << "Length in seconds: " << processAudio.getLengthInSeconds() << "\n";
  
  return 0;
}
