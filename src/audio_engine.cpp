#include <afsproject/audio_engine.h>
#include <afsproject/audio_file.h>
#include <afsproject/flac_file.h>
#include <afsproject/wave_file.h>
#include <memory>
#include <string>

namespace afs {

std::unique_ptr<IAudioFile> AudioEngine::loadAudioFile(const std::string &file_path)
{
  // NOTE: Using file extensions to determine audio file types.
  // TODO: Use file magic bytes to robustly determine the file type.

  if (file_path.ends_with(".wav")) {
    auto file = std::make_unique<WaveFile>();
    if (file->load(file_path)) { return file; }
  } else if (file_path.ends_with(".flac")) {
    auto file = std::make_unique<FlacFile>();
    if (file->load(file_path)) { return file; }
  }

  return nullptr;
}

bool AudioEngine::saveAudioFile(const IAudioFile &audio_file, const std::string &file_path)
{
  return audio_file.save(file_path);
}

}// namespace afs
