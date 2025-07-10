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
  size_t index{};
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
  size_t index{};
  std::string ckID;
  int32_t ckSize{};
  int nSamples{};
  int samplesStartIdx{};
};


template<class T> class ProcessAudio
{

private:
  using AudioBuffer = std::vector<std::vector<T>>;

  std::vector<uint8_t> fileData;
  int32_t sampleRate{};
  int16_t bitDepth{};
  int16_t duration{};
  AudioFormat fileFormat{};

  AudioFormat getAudioFormat();
  int32_t convertFourBytesToInt32(size_t, Endianness = Endianness::LittleEndian);
  int16_t convertTwoBytesToInt16(size_t, Endianness = Endianness::LittleEndian);
  Either<size_t, std::string> getIndexOfChunk(const std::string &, size_t, Endianness = Endianness::LittleEndian);
  void clearAudioBuffer();

  Either<WaveHeaderChunk, std::string> decodeHeaderChunk();
  Either<WaveFmtChunk, std::string> decodeFmtChunk();
  Either<WaveDataChunk, std::string> decodeDataChunk();

  bool decodeSamples(const WaveFmtChunk &, WaveDataChunk &);

  bool loadWaveFile();

public:
  AudioBuffer samples;

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


enum SampleLimit {
  SignedInt16_Min = -32768,
  SignedInt16_Max = 32767,
  UnsignedInt16_Min = 0,
  UnsignedInt16_Max = 65535,
  SignedInt24_Min = -8388608,
  SignedInt24_Max = 8388607,
  UnsignedInt24_Min = 0,
  UnsignedInt24_Max = 16777215
};

template<class T> struct AudioSampleConverter
{
  static T signedByteToSample(int8_t);
  static int8_t sampleToSignedByte(T);

  static T unsignedByteToSample(uint8_t);
  static uint8_t sampleToUnsignedByte(T);

  static T sixteenBitIntToSample(int16_t);
  static int16_t sampleToSixteenBitInt(T);

  static T twentyFourBitIntToSample(int32_t);
  static int32_t sampleToTwentyFourBitInt(T);

  static T thirtyTwoBitIntToSample(int32_t);
  static int32_t sampleToThirtyTwoBitInt(T);

  static T clamp(T, T, T);
};
};// namespace asf

#endif
