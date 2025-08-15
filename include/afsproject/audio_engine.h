#ifndef audio_engine_h_
#define audio_engine_h_

#include <afsproject/audio_file.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace afs {

enum class AudioFormat : uint8_t { Wave, Aiff, Mp3, Unsupported };

class IAudioFile;

class AudioEngine// NOLINT
{
public:
  AudioEngine() = default;
  ~AudioEngine() = default;

  static std::unique_ptr<IAudioFile> loadAudioFile(const std::string &);
  static bool saveAudioFile(const IAudioFile &, const std::string &);
};

}// namespace afs

#endif
