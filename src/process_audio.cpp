#include <algorithm>
#include <array>
#include <asfproject/either.h>
#include <asfproject/process_audio.h>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace asf {

ProcessAudio::ProcessAudio()
  : sampleRate(DEFAULT_SAMPLE_RATE), bitDepth(DEFAULT_BIT_DEPTH), fileFormat(AudioFormat::WAVE)
{
  samples.resize(1);
}

ProcessAudio::ProcessAudio(const std::string &fileName) : ProcessAudio() { loadAudioFromFile(fileName); }

AudioFormat ProcessAudio::getAudioFormat()
{
  if (fileData.size() < 4) return AudioFormat::Error;// NOLINT

  const std::string headerID(fileData.begin(), fileData.begin() + 4);

  if (headerID == "RIFF") {
    return AudioFormat::WAVE;
  } else {
    return AudioFormat::Unsupported;
  }
}

int32_t ProcessAudio::convertFourBytesToInt32(size_t startIdx, Endianness endianness)
{
  if (fileData.size() >= startIdx + 4) {
    int32_t res{};

    if (endianness == Endianness::LittleEndian) {
      res = (fileData[startIdx + 3] << 24) | (fileData[startIdx + 2] << 16) | (fileData[startIdx + 1] << 8)// NOLINT
            | fileData[startIdx];
    } else {
      res = (fileData[startIdx] << 24) | (fileData[startIdx + 1] << 16) | (fileData[startIdx + 2] << 8)// NOLINT
            | fileData[startIdx + 3];
    }

    return res;
  } else {
    std::cerr << "The data is just not structured correctly.\n";
    return -1;
  }
}

int16_t ProcessAudio::convertTwoBytesToInt16(size_t startIdx, Endianness endianness)
{
  int16_t res{};

  if (endianness == Endianness::LittleEndian) {
    res = static_cast<int16_t>(fileData[startIdx + 1] << 8) | fileData[startIdx];// NOLINT
  } else {
    res = static_cast<int16_t>(fileData[startIdx] << 8) | fileData[startIdx + 1];// NOLINT
  }

  return res;
}

Either<size_t, std::string>
  ProcessAudio::getIndexOfChunk(const std::string &ckID, size_t startIdx, [[maybe_unused]] Endianness _endianness)
{
  constexpr size_t reqLength = 4;

  if (ckID.size() != reqLength) {
    const std::string msg{ "The chunk ID length you passed is invalid bruh.\n" };
    return right(msg);
  }

  size_t idx = startIdx;
  while (idx < fileData.size() - reqLength) {
    if (std::memcmp(&fileData[idx], ckID.data(), reqLength) == 0) { return left(idx); }

    idx += reqLength;

    if ((idx + 4) >= fileData.size()) {
      const std::string msg{ "It seems like we ran out of data to read.\n" };
      return right(msg);
    }

    const int32_t ckSize = convertFourBytesToInt32(idx);
    if (ckSize > static_cast<int32_t>(fileData.size() - idx - reqLength) || (ckSize < 0)) {
      const std::string msg{ "The chunk size is somehow invalid.\n" };
      return right(msg);
    }

    idx += (reqLength + static_cast<size_t>(ckSize));
  }

  const std::string msg{ "We couldn't find that chunk for some reason.\n" };
  return right(msg);
}

Either<WaveHeaderChunk, std::string> ProcessAudio::decodeHeaderChunk()
{
  WaveHeaderChunk headerChunk{};

  headerChunk.ckID = { fileData.begin(), fileData.begin() + 4 };
  headerChunk.ckSize = convertFourBytesToInt32(4);
  headerChunk.fileTypeHeader = { fileData.begin() + 8, fileData.begin() + 12 };// NOLINT

  return left(headerChunk);
}

Either<WaveFmtChunk, std::string> ProcessAudio::decodeFmtChunk()
{
  WaveFmtChunk fmtChunk{};

  fmtChunk.index = getIndexOfChunk("fmt ", 12)// NOLINT
                     .leftMap([](auto idx) { return int(idx); })
                     .rightMap([](auto &msg) {
                       std::cerr << msg;
                       return -1;
                     })
                     .join();

  size_t f = size_t(fmtChunk.index);// NOLINT

  fmtChunk.ckID = { fileData.begin() + int(f), fileData.begin() + int(f) + 4 };
  fmtChunk.ckSize = convertFourBytesToInt32(f + 4);
  // NOLINTBEGIN
  fmtChunk.tag = convertTwoBytesToInt16(f + 8);
  fmtChunk.nChannels = convertTwoBytesToInt16(f + 10);
  fmtChunk.sampleRate = convertFourBytesToInt32(f + 12);
  fmtChunk.nAvgBytesPerSec = convertFourBytesToInt32(f + 16);
  fmtChunk.nBlockAlign = convertTwoBytesToInt16(f + 20);
  fmtChunk.bitDepth = convertTwoBytesToInt16(f + 22);
  // NOLINTEND

  if (fmtChunk.tag != WaveFormat::PCM) {
    const std::string msg{ "That wave format is not supported or not a wave format at all.\n" };
    return right(msg);
  }

  if (fmtChunk.nChannels < 1 || fmtChunk.nChannels > 128) {// NOLINT
    const std::string msg{ "Number of channels is just too high or too low, who knows.\n" };
    return right(msg);
  }

  if (fmtChunk.nAvgBytesPerSec != (fmtChunk.nChannels * fmtChunk.sampleRate * fmtChunk.bitDepth) / 8) {// NOLINT
    const std::string msg{ "The calculation is not mathing bruh, it ain't right.\n" };
    return right(msg);
  }

  if (fmtChunk.bitDepth != 8 && fmtChunk.bitDepth != 16 && fmtChunk.bitDepth != 24// NOLINT
      && fmtChunk.bitDepth != 32) {// NOLINT
    const std::string msg{ "Somehow this bit depth is not valid, HOW!?\n" };
    return right(msg);
  }

  return left(fmtChunk);
}

Either<WaveDataChunk, std::string> ProcessAudio::decodeDataChunk()
{
  WaveDataChunk dataChunk{};

  dataChunk.index = getIndexOfChunk("data", 12)// NOLINT
                      .leftMap([](auto idx) { return int(idx); })
                      .rightMap([](auto &msg) {
                        std::cerr << msg;
                        return -1;
                      })
                      .join();
  size_t d = size_t(dataChunk.index);// NOLINT

  dataChunk.ckID = { fileData.begin() + int(d), fileData.begin() + int(d) + 4 };
  dataChunk.ckSize = convertFourBytesToInt32(d + 4);

  return left(dataChunk);
}

void ProcessAudio::encodeHeaderChunk(std::vector<uint8_t> &data) const
{
  int32_t dataChunkSize = getNumSamplesPerChannel() * (getNumOfChannels() * bitDepth / 8);// NOLINT
  int16_t audioFormat = bitDepth == 32 && WaveFormat::PCM;// NOLINT
  int32_t fmtChunkSize = audioFormat == WaveFormat::PCM ? 16 : 18;// NOLINT

  // ckID	4	Chunk ID: "RIFF"
  addStringToData(data, "RIFF");

  // cksize	4	Chunk size: 4+n
  int32_t headerCkSize = 4 + fmtChunkSize + 8 + 8 + dataChunkSize;// NOLINT
  addInt32ToData(data, headerCkSize);

  // 	WAVEID	4	WAVE ID: "WAVE"
  addStringToData(data, "WAVE");
}

void ProcessAudio::encodeFmtChunk(std::vector<uint8_t> &data) const
{
  int16_t audioFormat = bitDepth == 32 && WaveFormat::PCM;// NOLINT
  int32_t fmtChunkSize = audioFormat == WaveFormat::PCM ? 16 : 18;// NOLINT

  // ckID	4	Chunk ID: "fmt "
  addStringToData(data, "fmt ");
  // cksize	4	Chunk size: 16, 18 or 40
  addInt32ToData(data, fmtChunkSize);
  // wFormatTag	2	Format code
  addInt16ToData(data, audioFormat);
  // nChannels	2	Number of interleaved channels
  addInt16ToData(data, getNumOfChannels());
  // 	nSamplesPerSec	4	Sampling rate (blocks per second)
  addInt32ToData(data, sampleRate);
  // nAvgBytesPerSec	4	Data rate
  addInt32ToData(data, sampleRate * (bitDepth / 8) * getNumOfChannels());// NOLINT
  // nBlockAlign	2	Data block size (bytes)
  addInt16ToData(data, int16_t((bitDepth / 8) * getNumOfChannels()));// NOLINT
  // wBitsPerSample	2	Bits per sample
  addInt16ToData(data, bitDepth);
}

void ProcessAudio::encodeDataChunk(std::vector<uint8_t> &data)
{
  // ckID	4	Chunk ID: "data"
  addStringToData(data, "data");
  // cksize	4	Chunk size: n
  int16_t dataCkSize = int16_t(getNumSamplesPerChannel() * (getNumOfChannels() * bitDepth / 8));// NOLINT
  addInt16ToData(data, dataCkSize);// NOLINT

  // 	sampled data	n	Samples
  if (bitDepth == 8) {// NOLINT
    encode8Bits(data);
  } else if (bitDepth == 16) {// NOLINT
    encode16Bits(data);
  } else if (bitDepth == 24) {// NOLINT
    assert(true && "Not implemented!");
  } else if (bitDepth == 32) {// NOLINT
    assert(true && "Not implemented!");
  }
}

void ProcessAudio::decode8Bits(const WaveFmtChunk &fmtCk, const std::vector<uint8_t> &data)
{
  if (fmtCk.nChannels == 1) {
    for (size_t i = 0; i < samples.size(); ++i) { samples[i] = data[i]; }
  } else {
    throw std::domain_error("That quantity of channels is still unsupported.");
  }
}

void ProcessAudio::decode16Bits(const WaveFmtChunk &fmtCk, const std::vector<uint8_t> &data)
{
  if (fmtCk.nChannels == 1) {
    for (size_t i = 0, k = 0; i < samples.size(); ++i, k += 2) {
      double sample = static_cast<int16_t>((data[k + 1] << 8) | data[k]);// NOLINT
      samples[i] = sample;
    }
  } else if (fmtCk.nChannels == 2) {
    size_t k = 0;// NOLINT
    for (size_t i = 0; i < samples.size() - 1; i += 2) {
      int16_t leftSample = static_cast<int16_t>((data[k + 1] << 8) | data[k]);// NOLINT
      int16_t rightSample = static_cast<int16_t>((data[k + 3] << 8) | data[k + 2]);// NOLINT
      samples[i] = leftSample;
      samples[i + 1] = rightSample;// NOLINT
      k += 4;
    }
  } else {
    throw std::domain_error("That quantity of chanels is still unsupported.");
  }
}

bool ProcessAudio::decodeSamples(const WaveFmtChunk &fmtChunk, WaveDataChunk &dataChunk)
{
  std::vector<uint8_t> data(size_t(dataChunk.ckSize));
  size_t offset = size_t(dataChunk.index + 8);// NOLINT

  if (offset + size_t(dataChunk.ckSize) <= fileData.size()) {// NOLINT
    std::copy(fileData.begin() + int(offset), fileData.begin() + int(offset) + dataChunk.ckSize, data.begin());
  }

  int bytesPerSample = fmtChunk.bitDepth / 8;// NOLINT
  auto numSamples = size_t(dataChunk.ckSize / (bytesPerSample * fmtChunk.nChannels));

  samples.clear();
  samples.resize(numSamples);

  if (fmtChunk.bitDepth == 8) {// NOLINT
    decode8Bits(fmtChunk, data);
  } else if (fmtChunk.bitDepth == 16) {// NOLINT
    decode16Bits(fmtChunk, data);
  } else if (fmtChunk.bitDepth == 24) {// NOLINT
    assert(true && "Not implemented!");
  } else if (fmtChunk.bitDepth == 32) {// NOLINT
    assert(true && "Not implemented!");
  }

  return true;
}

bool ProcessAudio::loadWaveFile()
{

  const WaveHeaderChunk headerChunk = decodeHeaderChunk()
                                        .leftMap([](auto hCk) { return hCk; })
                                        .rightMap([](auto &msg) {
                                          std::cerr << msg;
                                          return WaveHeaderChunk{};
                                        })
                                        .join();

  const WaveFmtChunk fmtChunk = decodeFmtChunk()
                                  .leftMap([](auto fCk) { return fCk; })
                                  .rightMap([](auto &msg) {
                                    std::cerr << msg;
                                    WaveFmtChunk fmtCk;
                                    fmtCk.index = -1;
                                    return fmtCk;
                                  })
                                  .join();

  WaveDataChunk dataChunk = decodeDataChunk()
                              .leftMap([](auto dCk) { return dCk; })
                              .rightMap([](auto &msg) {
                                std::cerr << msg;
                                WaveDataChunk dataCk;
                                dataCk.index = -1;
                                return dataCk;
                              })
                              .join();

  if (fmtChunk.index == -1 || dataChunk.index == -1 || headerChunk.ckID != "RIFF"
      || headerChunk.fileTypeHeader != "WAVE") {
    // std::cerr << "This is not a valid .WAV file.\n";
    return false;
  }

  sampleRate = fmtChunk.sampleRate;
  bitDepth = fmtChunk.bitDepth;
  nChannels = fmtChunk.nChannels;

  return decodeSamples(fmtChunk, dataChunk);
}

bool ProcessAudio::encodeWaveFile(std::vector<uint8_t> &data)
{
  encodeHeaderChunk(data);
  encodeFmtChunk(data);
  encodeDataChunk(data);

  return true;
}

void ProcessAudio::encode8Bits(std::vector<uint8_t> &data)
{
  if (getNumOfChannels() == 1) {
    for (const double sampleDbl : samples) {
      const uint8_t sample =
        static_cast<uint8_t>(std::round(std::max(static_cast<double>(std::numeric_limits<uint8_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<uint8_t>::max()), sampleDbl))));
      data.push_back(sample);
    }
  } else {
    throw std::domain_error("That quantity of channels is not supported yet!");
  }
}

void ProcessAudio::encode16Bits(std::vector<uint8_t> &data)
{
  if (getNumOfChannels() == 1) {
    for (const double sampleDbl : samples) {
      const int16_t sample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), sampleDbl))));

      data.push_back(static_cast<uint8_t>(sample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));// NOLINT
    }
  } else if (getNumOfChannels() == 2) {
    for (size_t i = 0; i < samples.size(); i += 2) {
      const int16_t leftSample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), samples[i]))));

      const int16_t rightSample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), samples[i + 1]))));

      data.push_back(static_cast<uint8_t>(leftSample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((leftSample >> 8) & 0xFF));// NOLINT

      data.push_back(static_cast<uint8_t>(rightSample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((rightSample >> 8) & 0xFF));// NOLINT
    }
  } else {
    throw std::domain_error("That quantity of channels is unsupported.");
  }
}

bool ProcessAudio::saveDataToMemory(std::vector<uint8_t> &data, AudioFormat format)
{
  if (format == AudioFormat::WAVE) { return encodeWaveFile(data); }

  return false;
}

bool ProcessAudio::writeDataToFile(std::vector<uint8_t> &data, const std::string &file_name)
{
  std::ofstream outputFile(file_name, std::ios::binary);

  if (!outputFile.is_open()) { return false; }

  outputFile.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));// NOLINT
  outputFile.close();
  return true;
}

bool ProcessAudio::loadAudioFromFile(const std::string &fileName)
{
  std::ifstream file(fileName, std::ios_base::binary);

  if (!file.good()) {
    std::cerr << "Opening that file failed badly or the file doesn't exist in this universe: " << fileName << "\n";
    return false;
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto fileLength = file.tellg();
  file.seekg(0, std::ios::beg);

  fileData.resize(static_cast<size_t>(fileLength));

  file.read(reinterpret_cast<char *>(fileData.data()), fileLength);// NOLINT
  file.close();

  if (file.gcount() != fileLength) {
    std::cerr << "Uh oh that didn't read the entire file.\n";
    return false;
  }

  if (fileData.size() < 12) {// NOLINT
    std::cerr << "The file data is just too short mate.\n";
    return false;
  }

  const AudioFormat audioFileFormat = getAudioFormat();

  if (audioFileFormat == AudioFormat::WAVE) {
    return loadWaveFile();
  } else {
    std::cerr << "We just don't support that file format as yet.\n";
    return false;
  }

  return true;
}

bool ProcessAudio::saveAudioToFile(const std::string &file_name, AudioFormat format)
{
  std::vector<uint8_t> data{};
  return saveDataToMemory(data, format) && writeDataToFile(data, file_name);
}

int32_t ProcessAudio::getSampleRate() const { return sampleRate; }

int16_t ProcessAudio::getNumOfChannels() const { return nChannels; }

bool ProcessAudio::isMono() const { return getNumOfChannels() == 1; }

bool ProcessAudio::isStereo() const { return getNumOfChannels() == 2; }

int16_t ProcessAudio::getBitDepth() const { return bitDepth; }

int ProcessAudio::getNumSamplesPerChannel() const
{
  if (!samples.empty()) {
    return int(samples.size());
  } else {
    return 0;
  }
}

double ProcessAudio::getLengthInSeconds() const { return double(getNumSamplesPerChannel()) / double(sampleRate); }

/*-- Helper functions --*/

void addStringToData(std::vector<uint8_t> &data, const std::string &str)
{
  for (const auto &value : str) { data.push_back(uint8_t(value)); }
}

void addInt32ToData(std::vector<uint8_t> &data, int32_t value)
{
  const std::array<uint8_t, 4> bytes{ convertInt32ToFourBytes(value) };

  for (const auto &val : bytes) { data.push_back(val); }
}

void addInt16ToData(std::vector<uint8_t> &data, int16_t value)
{
  const std::array<uint8_t, 2> bytes{ convertInt16ToTwoBytes(value) };

  for (const auto &val : bytes) { data.push_back(val); }
}

std::array<uint8_t, 4> convertInt32ToFourBytes(int32_t value, Endianness endianness)
{
  std::array<uint8_t, 4> bytes{};

  // NOLINTBEGIN
  if (endianness == Endianness::LittleEndian) {
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
  } else {
    bytes[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    bytes[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    bytes[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[3] = static_cast<uint8_t>(value & 0xFF);
  }
  // NOLINTEND

  return bytes;
}

std::array<uint8_t, 2> convertInt16ToTwoBytes(int16_t value, Endianness endianness)
{
  std::array<uint8_t, 2> bytes{};

  // NOLINTBEGIN
  if (endianness == Endianness::LittleEndian) {
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
  } else {
    bytes[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[1] = static_cast<uint8_t>(value & 0xFF);
  }
  // NOLINTEND

  return bytes;
}

}// namespace asf
