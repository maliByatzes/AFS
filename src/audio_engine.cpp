#include <asfproject/audio_engine.h>
#include <asfproject/audio_file.h>
#include <asfproject/wave_file.h>
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

std::vector<double> getPCMData(IAudioFile &audio_file) { return audio_file.getPCMData(); }

}// namespace asf
