#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <bitset>
#include <cstdint>
#include <etl/bit_stream.h>
#include <vector>

namespace afs {

const int THIRTY_TWO = 32;

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
  uint16_t min_block_size;
  uint16_t max_block_size;
  uint32_t min_frame_size;
  uint32_t max_frame_size;
  uint32_t sample_rate;
  uint8_t num_channels;
  uint8_t bit_depth;
  uint64_t total_samples;
};

class FlacFile : public IAudioFile// NOLINT
{
public:
  FlacFile() = default;
  ~FlacFile() override = default;

  bool load(const std::string &file_path) override;
  [[nodiscard]] bool save(const std::string &file_path) const override;
  [[nodiscard]] std::vector<double> getPCMData() const override;
  [[nodiscard]] uint32_t getSampleRate() const override;
  [[nodiscard]] uint16_t getNumChannels() const override;
  [[nodiscard]] double getDurationSeconds() const override;
  [[nodiscard]] bool isMono() const override;
  [[nodiscard]] bool isStereo() const override;
  [[nodiscard]] uint16_t getBitDepth() const override;
  [[nodiscard]] int getNumSamplesPerChannel() const override;

private:
  std::vector<uint8_t> m_file_data;
  uint32_t m_channel_mask;

  bool decodeStreaminfo(etl::bit_stream_reader &, uint32_t, uint8_t);
  static bool decodePadding(etl::bit_stream_reader &, uint32_t);
  static bool decodeApplication(etl::bit_stream_reader &, uint32_t);
  static bool decodeSeektable(etl::bit_stream_reader &, uint32_t);
  bool decodeVorbiscomment(etl::bit_stream_reader &, uint32_t);
  static bool decodeCuesheet(etl::bit_stream_reader &, uint32_t);
  static bool decodePicture(etl::bit_stream_reader &, uint32_t);

  bool decodeFlacFile();

  bool encodeFlacFile();
};

std::bitset<THIRTY_TWO> extract_from_lsb(const std::bitset<THIRTY_TWO> &, size_t, int);
std::bitset<THIRTY_TWO> extract_from_msb(const std::bitset<THIRTY_TWO> &, size_t, int);

std::string derteminePictureTypeStr(uint32_t);

}// namespace afs

#endif
