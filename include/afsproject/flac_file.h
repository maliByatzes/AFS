#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <bitset>
#include <cstdint>
#include <etl/bit_stream.h>
#include <optional>
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

struct FrameHeader
{
  uint16_t frame_sync_code;
  int strategy_bit;
  int block_size_bits;
  uint32_t block_size;
  int sample_rate_bits;
  uint32_t sample_rate;
  int channel_bits;
  uint16_t num_channels;
  int bit_depth_bits;
  uint16_t bit_depth;
  uint64_t coded_number;
  int crc8;
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
  uint32_t m_bits_read;
  // NOTE: could store `etl::bit_stream_reader` as a member variable.

  bool decodeStreaminfo(etl::bit_stream_reader &, uint32_t, uint8_t);
  bool decodePadding(etl::bit_stream_reader &, uint32_t);
  bool decodeApplication(etl::bit_stream_reader &, uint32_t);
  bool decodeSeektable(etl::bit_stream_reader &, uint32_t);
  bool decodeVorbiscomment(etl::bit_stream_reader &, uint32_t);
  bool decodeCuesheet(etl::bit_stream_reader &, uint32_t);
  bool decodePicture(etl::bit_stream_reader &, uint32_t);

  bool decodeFrames(etl::bit_stream_reader &);
  bool decodeFrame(etl::bit_stream_reader &);

  std::optional<FrameHeader> decodeFrameHeader(etl::bit_stream_reader &);

  bool decodeSubframes(etl::bit_stream_reader &, FrameHeader &);
  bool decodeSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t);
  bool decodeSubframeHeader(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t);
  bool decodeConstantSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t);
  bool decodeVerbatimSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t);
  bool decodeFixedSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t, uint8_t);
  bool decodeLPCSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t, uint8_t);
  bool decodeResidual(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint8_t);

  bool decodeFrameFooter(etl::bit_stream_reader &);

  bool decodeFlacFile();

  bool encodeFlacFile();

  std::optional<uint64_t> readUTF8(etl::bit_stream_reader &);
  int32_t readSignedValue(etl::bit_stream_reader &, uint16_t);
  int32_t readRiceSignedValue(etl::bit_stream_reader &, uint32_t);
};

std::bitset<THIRTY_TWO> extract_from_lsb(const std::bitset<THIRTY_TWO> &, size_t, int);
std::bitset<THIRTY_TWO> extract_from_msb(const std::bitset<THIRTY_TWO> &, size_t, int);

std::string determinePictureTypeStr(uint32_t);
uint32_t determineBlockSize(int);
uint32_t determineSampleRate(int);
uint16_t determineChannels(int);
uint16_t determineBitDepth(int);
std::string determineSubframeType(int);

int utf8SequenceLength(uint8_t);

}// namespace afs

#endif
