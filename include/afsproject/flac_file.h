#ifndef flac_file_h_
#define flac_file_h_

#include <afsproject/audio_file.h>
#include <bit>
#include <bitset>
#include <cstdint>
#include <span>
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

  bool decodeStreaminfo(uint32_t, long);
  bool decodePadding();
  bool decodeApplication();
  bool decodeSeektable();
  bool decodeVorbiscomment();
  bool decodeCuesheet();
  bool decodePicture();
  
  bool decodeFlacFile();

  bool encodeFlacFile();
};

class BitStreamReader
{
private:
  std::span<const uint8_t> m_data;
  size_t m_bit_position = 0;

  std::vector<uint8_t> extract_relevant_bytes(int);

public:
  explicit BitStreamReader(std::span<const uint8_t>);

  uint64_t read_bits(int, std::endian = std::endian::big);

private:
  uint64_t read_simple_bits(int, std::endian);
  uint64_t read_complex_bits(int, std::endian);  
  uint64_t swap_bytes_in_value(uint64_t, int);

public:
  void reset();
  [[nodiscard]] size_t get_bit_position() const;
  [[nodiscard]] size_t get_remaining_bits() const;
};

std::bitset<THIRTY_TWO> extract_from_lsb(const std::bitset<THIRTY_TWO> &, size_t, int);
std::bitset<THIRTY_TWO> extract_from_msb(const std::bitset<THIRTY_TWO> &, size_t, int);


}// namespace afs

#endif
