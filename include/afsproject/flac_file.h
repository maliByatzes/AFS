#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <cstdint>
#include <vector>

namespace afs {

class FlacFile : public IAudioFile// NOLINT
{
public:
  FlacFile() = default;
  ~FlacFile() override = default;

  bool load(const std::string &file_path) override;
  [[nodiscard]] bool save(const std::string &file_path) const override;
  [[nodiscard]] std::vector<double> getPCMData() const override;
  [[nodiscard]] int32_t getSampleRate() const override;
  [[nodiscard]] int16_t getNumChannels() const override;
  [[nodiscard]] double getDurationSeconds() const override;
  [[nodiscard]] bool isMono() const override;
  [[nodiscard]] bool isStereo() const override;
  [[nodiscard]] int16_t getBitDepth() const override;
  [[nodiscard]] int getNumSamplesPerChannel() const override;

private:
  std::vector<uint8_t> m_file_data;
  
  bool decodeFlacFile();

  bool encodeFlacFile();
};

}// namespace afs

#endif
