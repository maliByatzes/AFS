#ifndef audio_engine_h_
#define audio_engine_h_

#include <asfproject/audio_file.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace asf {

enum class AudioFormat : uint8_t { Wave, Aiff, Mp3, Unsupported };

class IAudioFile;

class AudioEngine//NOLINT
{
public:
  AudioEngine() = default;
  ~AudioEngine() = default;

  static std::unique_ptr<IAudioFile> loadAudioFile(const std::string &);
  static bool saveAudioFile(const IAudioFile &, const std::string &);

  // void play(IAudioFile &);
  static std::vector<double> getPCMData(IAudioFile &);
  static void normalizePCMData(IAudioFile &);
  static void stereoToMono(IAudioFile &);
  static void applyLowPassFilter(IAudioFile &);
  static void downSampling(IAudioFile &);
  static void applyHammingWindow(IAudioFile &);
  // NOTE: This may be temporary
  static void processServerSide(IAudioFile &);
};

}// namespace asf

#endif
