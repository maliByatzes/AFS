#include <algorithm>
#include <asfproject/audio_engine.h>
#include <asfproject/audio_file.h>
#include <asfproject/wave_file.h>
#include <cmath>
#include <cstddef>
#include <cstdlib>
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

std::vector<double> AudioEngine::getPCMData(IAudioFile &audio_file) { return audio_file.getPCMData(); }

void AudioEngine::stereoToMono(IAudioFile &audio_file)
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

void AudioEngine::normalizePCMData(IAudioFile &audio_file)
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
/*
void AudioEngine::applyLowPassFilter(IAudioFile &audio_file)
{
  
}*/

}// namespace asf
