#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <cstdint>
#include <vector>

namespace afs {

/*
enum class MetadataBlockType : uint8_t {
  STREAM_INFO = 0,
  PADDING = 1,
  APPLICATION = 2,
  SEEK_TABLE = 3,
  VORBIS_COMMENT = 4,
  CUE_SHEET = 5,
  PICTURE = 6,
  FORBIDDEN = 127
};*/

struct StreamInfo
{
};

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

  bool decodeStreaminfo();
  bool decodePadding();
  bool decodeApplication();
  bool decodeSeektable();
  bool decodeVorbiscomment();
  bool decodeCuesheet();
  bool decodePicture();
  
  bool decodeFlacFile();

  bool encodeFlacFile();
};

}// namespace afs

#endif
