#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <cstdint>
#include <etl/bit_stream.h>
#include <optional>
#include <vector>

namespace afs {

const int THIRTY_TWO = 32;

using Subframes = std::vector<std::vector<int32_t>>;

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
  uint64_t m_total_samples{};
  uint32_t m_channel_mask{};
  uint32_t m_bits_read{};
  std::vector<uint8_t> m_md5_checksum;
  bool m_has_md5_signature = false;
  std::vector<std::vector<int>> m_samples;
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
  bool seekToNextFrame(etl::bit_stream_reader &);

  std::optional<FrameHeader> decodeFrameHeader(etl::bit_stream_reader &);

  std::optional<Subframes> decodeSubframes(etl::bit_stream_reader &, FrameHeader &);
  bool decodeSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t);
  bool decodeSubframeHeader(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t);
  bool decodeConstantSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint16_t, uint8_t);
  bool decodeVerbatimSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t);
  bool decodeFixedSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t, uint8_t);
  bool decodeLPCSubframe(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint16_t, uint8_t, uint8_t);
  bool decodeResidual(etl::bit_stream_reader &, std::vector<int32_t> &, uint32_t, uint8_t);

  bool decodeFrameFooter(etl::bit_stream_reader &);

  bool decodeFlacFile();

  static bool encodeFlacFile();

  std::optional<uint64_t> readUTF8(etl::bit_stream_reader &);
  int32_t readSignedValue(etl::bit_stream_reader &, uint16_t);
  int32_t readRiceSignedValue(etl::bit_stream_reader &, uint32_t);
  static bool decorrelateChannels(std::vector<std::vector<int32_t>> &, int);
  bool isSyncCode(etl::bit_stream_reader &) const;
  void storeSamples(const std::vector<std::vector<int32_t>> &);
  bool validateMD5Checksum();
};

std::string determinePictureTypeStr(uint32_t);
uint32_t determineBlockSize(int);
uint32_t determineSampleRate(int);
uint16_t determineChannels(int);
uint16_t determineBitDepth(int);
std::string determineSubframeType(int);

int utf8SequenceLength(uint8_t);

const uint32_t INITIAL_A = 0x67452301;
const uint32_t INITIAL_B = static_cast<uint32_t>(0xEFCDAB89L);
const uint32_t INITIAL_C = static_cast<uint32_t>(0x98BADCFEL);
const uint32_t INITIAL_D = 0x10325476;

// NOLINTNEXTLINE
const std::vector<uint32_t> SHIFT_AMOUNTS = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

// NOLINTNEXTLINE
const std::vector<uint32_t> K = { 0xd76aa478,
  0xe8c7b756,
  0x242070db,
  0xc1bdceee,
  0xf57c0faf,
  0x4787c62a,
  0xa8304613,
  0xfd469501,
  0x698098d8,
  0x8b44f7af,
  0xffff5bb1,
  0x895cd7be,
  0x6b901122,
  0xfd987193,
  0xa679438e,
  0x49b40821,
  0xf61e2562,
  0xc040b340,
  0x265e5a51,
  0xe9b6c7aa,
  0xd62f105d,
  0x02441453,
  0xd8a1e681,
  0xe7d3fbc8,
  0x21e1cde6,
  0xc33707d6,
  0xf4d50d87,
  0x455a14ed,
  0xa9e3e905,
  0xfcefa3f8,
  0x676f02d9,
  0x8d2a4c8a,
  0xfffa3942,
  0x8771f681,
  0x6d9d6122,
  0xfde5380c,
  0xa4beea44,
  0x4bdecfa9,
  0xf6bb4b60,
  0xbebfbc70,
  0x289b7ec6,
  0xeaa127fa,
  0xd4ef3085,
  0x04881d05,
  0xd9d4d039,
  0xe6db99e5,
  0x1fa27cf8,
  0xc4ac5665,
  0xf4292244,
  0x432aff97,
  0xab9423a7,
  0xfc93a039,
  0x655b59c3,
  0x8f0ccc92,
  0xffeff47d,
  0x85845dd1,
  0x6fa87e4f,
  0xfe2ce6e0,
  0xa3014314,
  0x4e0811a1,
  0xf7537e82,
  0xbd3af235,
  0x2ad7d2bb,
  0xeb86d391 };

std::vector<uint8_t> computeMD5(const std::vector<uint8_t> &message);
std::string to_hex_string(const std::vector<uint8_t> &bytes);
std::vector<uint8_t> to_byte_vector(const std ::string &text);

}// namespace afs

#endif
