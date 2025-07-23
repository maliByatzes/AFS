#ifndef wave_file_h_
#define wave_file_h_

#include <asfproject/audio_file.h>
#include <asfproject/either.h>
#include <bit>
#include <cstdint>
#include <span>

namespace asf {

enum class WaveFormat : uint16_t {
  PCM = 0x0001,
  IEEE_FLOAT = 0x0003,
  ALAW = 0x0006,
  MULAW = 0x0007,
  EXTENSIBLE = 0xFFFE
};

struct WaveHeaderChunk
{
  std::string ck_id;
  int32_t ck_size;
  std::string file_type_header;
};

struct WaveFmtChunk
{
  int index;
  std::string ck_id;
  int32_t ck_size;
  int16_t format_tag;
  int16_t n_channels;
  int32_t sample_rate;
  int32_t n_avg_bytes_per_sec;
  int16_t n_block_align;
  int16_t bit_depth;
};

struct WaveDataChunk
{
  int index;
  std::string ck_id;
  int32_t ck_size;
  int n_samples;
  int samples_start_idx;
};

class WaveFile : public IAudioFile//NOLINT
{
public:
  WaveFile() = default;
  ~WaveFile() override = default;

  bool load(const std::string &file_path) override;
  [[nodiscard]] std::vector<double> getPCMData() const override;
  [[nodiscard]] int getSampleRate() const override;
  [[nodiscard]] int getNumChannels() const override;
  [[nodiscard]] double getDurationSeconds() const override;
  [[nodiscard]] bool isMono() const override;
  [[nodiscard]] bool isStero() const override;
  [[nodiscard]] int getBitDepth() const override;
  [[nodiscard]] int getNumSamplesPerChannel() const override;

private:
  std::vector<uint8_t> m_file_data;

  WaveHeaderChunk decodeHeaderChunk();
  Either<WaveFmtChunk, std::string> decodeFmtChunk();
  WaveDataChunk decodeDataChunk();
  Either<size_t, std::string> getIdxOfChunk(const std::string &, size_t);

  void decode8Bits(const WaveFmtChunk &, const std::vector<uint8_t> &);
  void decode16Bits(const WaveFmtChunk &, const std::vector<uint8_t> &);
  bool decodeSamples(const WaveFmtChunk &, WaveDataChunk &);
  bool decodeWaveFile();
};

// TODO: Move these utilities functions to audio_engine.h file
int32_t convFourBytesToInt32(std::span<uint8_t>, std::endian = std::endian::little);
int16_t convTwoBytesToInt16(std::span<uint8_t>, std::endian = std::endian::little);

}// namespace asf

#endif
