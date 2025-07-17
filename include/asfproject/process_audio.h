#ifndef process_audio_h_
#define process_audio_h_

#include "either.h"
#include <cstdint>
#include <string>
#include <vector>

namespace asf {

// ======= Constants ========
constexpr int16_t DEFAULT_BIT_DEPTH = 16;
constexpr int32_t DEFAULT_SAMPLE_RATE = 44100;


enum class Endianness : uint8_t { LittleEndian, BigEndian };

enum class AudioFormat : uint8_t { WAVE, Unsupported, Error };

// =========== Wave Type Specific structures ===========
enum WaveFormat : uint16_t { PCM = 0x0001, IEEE_FLOAT = 0x0003, ALAW = 0x0006, MULAW = 0x0007, EXTENSIBLE = 0xFFFE };

struct WaveHeaderChunk
{
  std::string ckID;
  int32_t ckSize{};
  std::string fileTypeHeader;
};

struct WaveFmtChunk
{
  int index{};
  std::string ckID;
  int32_t ckSize{};
  int16_t tag{};
  int16_t nChannels{};
  int32_t sampleRate{};
  int32_t nAvgBytesPerSec{};
  int16_t nBlockAlign{};
  int16_t bitDepth{};
};

struct WaveDataChunk
{
  int index{};
  std::string ckID;
  int32_t ckSize{};
  int nSamples{};
  int samplesStartIdx{};
};


class ProcessAudio
{

private:
  std::vector<uint8_t> fileData;
  int32_t sampleRate{};
  int16_t bitDepth{};
  int16_t nChannels {};
  AudioFormat fileFormat{};

  AudioFormat getAudioFormat();
  int32_t convertFourBytesToInt32(size_t, Endianness = Endianness::LittleEndian);
  int16_t convertTwoBytesToInt16(size_t, Endianness = Endianness::LittleEndian);
  Either<size_t, std::string> getIndexOfChunk(const std::string &, size_t, Endianness = Endianness::LittleEndian);

  Either<WaveHeaderChunk, std::string> decodeHeaderChunk();
  Either<WaveFmtChunk, std::string> decodeFmtChunk();
  Either<WaveDataChunk, std::string> decodeDataChunk();

  bool decodeSamples(const WaveFmtChunk &, WaveDataChunk &);
  void decode8Bits(const WaveFmtChunk &, const std::vector<uint8_t> &);
  void decode16Bits(const WaveFmtChunk &, const std::vector<uint8_t> &);

  bool loadWaveFile();

public:
  std::vector<float> samples;

  ProcessAudio();
  explicit ProcessAudio(const std::string &);

  bool loadAudioFromFile(const std::string &);

  [[nodiscard]] int32_t getSampleRate() const;
  [[nodiscard]] int16_t getNumOfChannels() const;
  [[nodiscard]] bool isMono() const;
  [[nodiscard]] bool isStereo() const;
  [[nodiscard]] int16_t getBitDepth() const;
  [[nodiscard]] int getNumSamplesPerChannel() const;
  [[nodiscard]] double getLengthInSeconds() const;
};

};// namespace asf

#endif
