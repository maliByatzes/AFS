#ifndef process_audio_h_
#define process_audio_h_

#include "either.h"
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>

namespace asf
{

  enum class Endianness
  {
    LittleEndian,
    BigEndian
  };

  enum class AudioFormat
  {
    WAVE,
    Unsupported,
    Error
  };

  // =========== Wave Type Specific structures ===========
  enum WaveFormat
  {
    PCM        = 0x0001,
    IEEE_FLOAT = 0x0003,
    ALAW       = 0x0006,
    MULAW      = 0x0007,
    EXTENSIBLE = 0xFFFE
  };

  struct WaveHeaderChunk
  {
    std::string ckID {};
    int32_t ckSize {};
    std::string fileTypeHeader {};
  };

  struct WaveFmtChunk
  {
    int index {};
    std::string ckID {};
    int32_t ckSize {};
    uint16_t tag {};
    uint16_t nChannels {};
    uint32_t sampleRate {};
    uint32_t nAvgBytesPerSec {};
    uint16_t nBlockAlign {};
    uint16_t bitDepth {};
  };

  struct WaveDataChunk
  {
    int index {};
    std::string ckID {};
    int32_t ckSize {};
    int nSamples {};
    int samplesStartIdx {};
  };


  template<class T>
  class ProcessAudio
  {

  private:
    typedef std::vector<std::vector<T>> AudioBuffer;
    
    std::vector<uint8_t> fileData {};
    uint32_t sampleRate {};
    uint16_t bitDepth {};
    uint16_t duration {};
    std::string fileFormat {};
    AudioBuffer samples; 

    AudioFormat getAudioFormat();
    int32_t convertFourBytesToInt32(int, Endianness = Endianness::LittleEndian);
    int16_t convertTwoBytesToInt16(int, Endianness = Endianness::LittleEndian);
    int getIndexOfChunk(const std::string &, int, Endianness = Endianness::LittleEndian);
    void clearAudioBuffer();

    Either<WaveHeaderChunk, std::string> decodeHeaderChunk();
    Either<WaveFmtChunk, std::string> decodeFmtChunk();
    Either<WaveDataChunk, std::string> decodeDataChunk();

    bool decodeSamples(WaveFmtChunk &, WaveDataChunk &);
    
    bool loadWaveFile();

  public:
    bool loadAudioFromFile(const std::string &);
  };
  

  enum SampleLimit
  {
    SignedInt16_Min = -32768,
    SignedInt16_Max = 32767,
    UnsignedInt16_Min = 0,
    UnsignedInt16_Max = 65535,
    SignedInt24_Min = -8388608,
    SignedInt24_Max = 8388607,
    UnsignedInt24_Min = 0,
    UnsignedInt24_Max = 16777215
  };
  
  template<class T>
  struct AudioSampleConverter
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

    static T clamp (T v1, T minValue, T maxValue);
  };
};

#endif
