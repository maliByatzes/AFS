#include "process_audio.h"

namespace asf
{

  AudioFormat ProcessAudio::getAudioFormat()
  {
    if (fileData.size() < 4)
      return AudioFormat::Error;
      
    std::string headerID (fileData.begin(), fileData.begin() + 4);

    if (headerID == "RIFF")
      return AudioFormat::WAVE;
    else
      return AudioFormat::Unsupported;
  }

  int32_t ProcessAudio::convertFourBytesToInt32(int startIdx, Endianness endianness)
  {
    if (fileData.size() >= startIdx + 4) {
      int32_t res;

      if (endianness == Endianness::LittleEndian)
        res = (fileData[startIdx + 3] << 24) |
              (fileData[startIdx + 2] << 16) |
              (fileData[startIdx + 1] << 8) |
              fileData[startIdx];
      else
        res = (fileData[startIdx] << 24) |
              (fileData[startIdx + 1] << 16) |
              (fileData[startIdx + 2] << 8) |
              fileData[startIdx + 3];

      return res;
    } else {
      std::cerr << "The data is just not structured correctly.\n";
      return -1;
    }
  }

  int16_t ProcessAudio::convertTwoBytesToInt16(int startIdx, Endianness endianness)
  {
    int16_t res;

    if (endianness == Endianness::LittleEndian)
      res = (fileData[startIdx + 1] << 8) | fileData[startIdx];
    else
      res = (fileData[startIdx] << 8) | fileData[startIdx + 1];  

    return res;
  }

  int ProcessAudio::getIndexOfChunk(const std::string &ckID, int startIdx, Endianness endianness)
  {
    constexpr int reqLength = 4;

    if (ckID.size() != reqLength) {
      std::cerr << "The chunk ID length you passed is invalid bruh.\n";
      return -1;
    }

    int idx = startIdx;
    while (idx < fileData.size() - reqLength) {
      if (std::memcmp(&fileData[idx], ckID.data(), reqLength) == 0) {
        return idx;
      }

      idx += reqLength;

      if ((idx + 4) >= fileData.size()){
        std::cerr << "It seems like we ran out of data to read.\n";
        return -1;
      }

      int32_t ckSize = convertFourBytesToInt32(idx);
      if (ckSize > (fileData.size() - idx - reqLength) || (ckSize < 0)) {
        std::cerr << "The chunk size is somehow invalid.\n";
        return -1;
      }

      idx += (reqLength + ckSize);
    }

    return -1;
  }

  bool ProcessAudio::loadWaveFile()
  {
    // ============= HEADER CHUNK ==============
    std::string headerChunkID (fileData.begin(), fileData.begin() + 4);
    int32_t headerChunkSize = convertFourBytesToInt32(4);
    std::string fileTypeHeader (fileData.begin() + 8, fileData.begin() + 12);

    int idxOfFmtChunk = getIndexOfChunk("fmt ", 12);
    int idxOfDataChunk = getIndexOfChunk("data", 12);

    if (idxOfFmtChunk == -1 || idxOfDataChunk == -1 || headerChunkID != "RIFF" || fileTypeHeader != "WAVE") {
      std::cerr << "This is not a valid .WAV file.\n";
      return false;
    }

    // ========== FORMAT CHUNK ==================
    int f = idxOfFmtChunk;
    std::string fmtCkID (fileData.begin() + f, fileData.begin() + f + 4);
    int32_t fmtCkSize = convertFourBytesToInt32(f + 4);
    uint16_t fmtTag = convertTwoBytesToInt16(f + 8);
    uint16_t nChannels = convertTwoBytesToInt16(f + 10);
    uint32_t nSamplesPerSec = convertFourBytesToInt32(f + 12);
    uint32_t nAvgBytesPerSec = convertFourBytesToInt32(f + 16);
    uint16_t nBlockAlign = convertTwoBytesToInt16(f + 20);
    uint16_t bitsPerSample = convertTwoBytesToInt16(f + 22);

    if (fmtTag != WaveFormat::PCM &&
        fmtTag != WaveFormat::IEEE_FLOAT &&
        fmtTag != WaveFormat::ALAW &&
        fmtTag != WaveFormat::MULAW &&
        fmtTag != WaveFormat::EXTENSIBLE
    ) {
      std::cerr << "That wave format is not supported or not a wave format at all.\n";    
      return false;
    }

    if (nChannels < 1 || nChannels > 128) {
      std::cerr << "Number of channels is just too high or too low, who knows.\n";
      return false;
    }

    if (nAvgBytesPerSec != static_cast<uint32_t>((nChannels * nSamplesPerSec * bitsPerSample) / 8)) {
      std::cerr << "The calculation is not mathing bruh, it ain't right.\n";
      return false;
    }

    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
      std::cerr << "Somehow this bit depth is not valid, HOW!?\n";
      return false;
    }

    // ================ DATA CHUNK ===============
    int d = idxOfDataChunk;
    std::string dataCkID (fileData.begin() + d, fileData.begin() + d + 4);
    int32_t dataCkSize = convertFourBytesToInt32(d + 4);

    int nSamples = dataCkSize / (nChannels * bitsPerSample / 8);
    int sampleStartIdx = idxOfDataChunk + 8;

    return true;
  }

  bool ProcessAudio::loadAudioFromFile(const std::string &fileName)
  {
    std::ifstream file (fileName, std::ios_base::binary);

    if (!file.good()) {
     std::cerr << "Opening that file failed badly or the file doesn't exist in this universe: "
     << fileName << "\n";
      return false;
    }

    file.unsetf(std::ios::skipws);
    file.seekg(0, std::ios::end);
    std::size_t fileLength = file.tellg();
    file.seekg(0, std::ios::beg);

    fileData.resize(fileLength);

    file.read(reinterpret_cast<char*>(fileData.data()), fileLength);
    file.close();

    if (file.gcount() != fileLength) {
      std::cerr << "Uh oh that didn't read the entire file.\n";
      return false;
    }

    if (fileData.size() < 12) {
      std::cerr << "The file data is just too short mate.\n";
      return false;
    }

    AudioFormat audioFileFormat = getAudioFormat();

    if (audioFileFormat == AudioFormat::WAVE) {
      return loadWaveFile();
    } else {
      std::cerr << "We just don't support that file format as yet.\n";
      return false;
    }

    return true;
  }
  
}
