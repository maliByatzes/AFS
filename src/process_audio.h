#ifndef process_audio_h_
#define process_audio_h_

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

  enum WaveFormat
  {
    PCM        = 0x0001,
    IEEE_FLOAT = 0x0003,
    ALAW       = 0x0006,
    MULAW      = 0x0007,
    EXTENSIBLE = 0xFFFE
  };

  class ProcessAudio
  {

  private:
    std::vector<uint8_t> fileData {};
    uint32_t sampleRate {};
    uint16_t bitDepth {};
    uint16_t duration {};
    std::string fileFormat {};  

    AudioFormat getAudioFormat();
    int32_t convertFourBytesToInt32(int, Endianness = Endianness::LittleEndian);
    int16_t convertTwoBytesToInt16(int, Endianness = Endianness::LittleEndian);
    int getIndexOfChunk(const std::string &, int, Endianness = Endianness::LittleEndian);
    bool loadWaveFile();

  public:
    bool loadAudioFromFile(const std::string &);
  };
  
};

#endif
