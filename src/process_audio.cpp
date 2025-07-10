#include <asfproject/process_audio.h>
#include <asfproject/either.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <type_traits>
#include <string>

namespace asf {

template<class T>
ProcessAudio<T>::ProcessAudio()
  : sampleRate(DEFAULT_SAMPLE_RATE), bitDepth(DEFAULT_BIT_DEPTH), fileFormat(AudioFormat::WAVE)
{
  samples.resize(1);
  samples[0].resize(0);
}

template<class T> ProcessAudio<T>::ProcessAudio(const std::string &fileName) : ProcessAudio<T>()
{
  loadAudioFromFile(fileName);
}

template<class T> AudioFormat ProcessAudio<T>::getAudioFormat()
{
  if (fileData.size() < 4) return AudioFormat::Error;// NOLINT

  const std::string headerID(fileData.begin(), fileData.begin() + 4);

  if (headerID == "RIFF") {
    return AudioFormat::WAVE;
  } else {
    return AudioFormat::Unsupported;
  }
}

template<class T> int32_t ProcessAudio<T>::convertFourBytesToInt32(size_t startIdx, Endianness endianness)
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

template<class T> int16_t ProcessAudio<T>::convertTwoBytesToInt16(size_t startIdx, Endianness endianness)
{
  int16_t res{};

  if (endianness == Endianness::LittleEndian) {
    res = static_cast<int16_t>(fileData[startIdx + 1] << 8) | fileData[startIdx];// NOLINT
  } else {
    res = static_cast<int16_t>(fileData[startIdx] << 8) | fileData[startIdx + 1];// NOLINT
  }

  return res;
}

template<class T>
Either<size_t, std::string>
  ProcessAudio<T>::getIndexOfChunk(const std::string &ckID, size_t startIdx, [[maybe_unused]] Endianness _endianness)
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

template<class T> void ProcessAudio<T>::clearAudioBuffer()
{
  for (size_t i = 0; i < samples.size(); i++) { samples[i].clear(); }
  samples.clear();
}

template<class T> Either<WaveHeaderChunk, std::string> ProcessAudio<T>::decodeHeaderChunk()
{
  WaveHeaderChunk headerChunk{};

  headerChunk.ckID = { fileData.begin(), fileData.begin() + 4 };
  headerChunk.ckSize = convertFourBytesToInt32(4);
  headerChunk.fileTypeHeader = { fileData.begin() + 8, fileData.begin() + 12 };// NOLINT

  return left(headerChunk);
}

template<class T> Either<WaveFmtChunk, std::string> ProcessAudio<T>::decodeFmtChunk()
{
  WaveFmtChunk fmtChunk{};

  fmtChunk.index = getIndexOfChunk("fmt ", 12)// NOLINT
                     .leftMap([](auto idx) { return idx; })
                     .rightMap([](auto &msg) {
                       std::cerr << msg;
                       return size_t(0);
                     })
                     .join();

  size_t f = fmtChunk.index;// NOLINT

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

  if (fmtChunk.nAvgBytesPerSec
      != (fmtChunk.nChannels * fmtChunk.sampleRate * fmtChunk.bitDepth) / 8) {// NOLINT
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

template<class T> Either<WaveDataChunk, std::string> ProcessAudio<T>::decodeDataChunk()
{
  WaveDataChunk dataChunk{};

  dataChunk.index = getIndexOfChunk("data", 12)// NOLINT
                      .leftMap([](auto idx) { return idx; })
                      .rightMap([](auto &msg) {
                        std::cerr << msg;
                        return size_t(0);
                      })
                      .join();
  size_t d = dataChunk.index;// NOLINT

  dataChunk.ckID = { fileData.begin() + int(d), fileData.begin() + int(d) + 4 };
  dataChunk.ckSize = convertFourBytesToInt32(d + 4);

  return left(dataChunk);
}

template<class T> bool ProcessAudio<T>::decodeSamples(const WaveFmtChunk &fmtChunk, WaveDataChunk &dataChunk)
{
  dataChunk.nSamples = dataChunk.ckSize / (fmtChunk.nChannels * fmtChunk.bitDepth / 8);// NOLINT
  int16_t numBytesPerSample = fmtChunk.bitDepth / int16_t(8);// NOLINT
  size_t samplesStartIdx = dataChunk.index + 8;// NOLINT

  clearAudioBuffer();
  samples.resize(static_cast<size_t>(fmtChunk.nChannels));

  for (size_t i = 0; i < size_t(dataChunk.nSamples); i++) {
    for (size_t channel = 0; channel < size_t(fmtChunk.nChannels); channel++) {

      const size_t sampleIndex = samplesStartIdx + (size_t(fmtChunk.nBlockAlign) * i) + (channel * size_t(numBytesPerSample));

      if ((sampleIndex + (size_t(fmtChunk.bitDepth) / 8) - 1) >= fileData.size()) {// NOLINT
        std::cerr << "There is somehow more samples than there are in the file data.\n";
        return false;
      }

      if (fmtChunk.bitDepth == 8) {// NOLINT
        T sample = AudioSampleConverter<T>::unsignedByteToSample(fileData[sampleIndex]);
        samples[channel].push_back(sample);
      } else if (fmtChunk.bitDepth == 16) {// NOLINT
        const int16_t sampleAsInt = convertTwoBytesToInt16(sampleIndex);
        T sample = AudioSampleConverter<T>::sixteenBitIntToSample(sampleAsInt);
        samples[channel].push_back(sample);
      } else if (fmtChunk.bitDepth == 24) {// NOLINT
        int32_t sampleAsInt{};
        sampleAsInt =
          (fileData[sampleIndex + 2] << 16) | (fileData[sampleIndex + 1] << 8) | fileData[sampleIndex];// NOLINT

        if (sampleAsInt & 0x800000) sampleAsInt = sampleAsInt | ~0xFFFFFF;// NOLINT

        T sample = AudioSampleConverter<T>::twentyFourBitIntToSample(sampleAsInt);
        samples[channel].push_back(sample);
      } else if (fmtChunk.bitDepth == 32) {// NOLINT
        const int32_t sampleAsInt = convertFourBytesToInt32(sampleIndex);
        T sample = AudioSampleConverter<T>::thirtyTwoBitIntToSample(sampleAsInt);
        samples[channel].push_back(sample);
      } else {
        // Shouldn't execute
        return false;
      }
    }
  }

  return true;
}

template<class T> bool ProcessAudio<T>::loadWaveFile()
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
                              fmtCk.index = size_t(0);
                              return fmtCk;
                            })
                            .join();

  WaveDataChunk dataChunk = decodeDataChunk()
                              .leftMap([](auto dCk) { return dCk; })
                              .rightMap([](auto &msg) {
                                std::cerr << msg;
                                WaveDataChunk dataCk;
                                dataCk.index = size_t(0);
                                return dataCk;
                              })
                              .join();

  if (fmtChunk.index == 0 || dataChunk.index == 0 || headerChunk.ckID != "RIFF"
      || headerChunk.fileTypeHeader != "WAVE") {
    // std::cerr << "This is not a valid .WAV file.\n";
    return false;
  }

  sampleRate = fmtChunk.sampleRate;
  bitDepth = fmtChunk.bitDepth;

  return decodeSamples(fmtChunk, dataChunk);
}

template<class T> bool ProcessAudio<T>::loadAudioFromFile(const std::string &fileName)
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


template<class T> int32_t ProcessAudio<T>::getSampleRate() const
{
  return sampleRate;
}

template<class T> int16_t ProcessAudio<T>::getNumOfChannels() const
{
  return int16_t(samples.size());
}

template<class T> bool ProcessAudio<T>::isMono() const
{
  return getNumOfChannels() == 1;
}

template<class T> bool ProcessAudio<T>::isStereo() const
{
  return getNumOfChannels() == 2;
}

template<class T> int16_t ProcessAudio<T>::getBitDepth() const
{
  return bitDepth;
}

template<class T> int ProcessAudio<T>::getNumSamplesPerChannel() const
{
  if (!samples.empty()) {
    return int(samples[0].size());
  } else {
    return 0;
  }
}

template<class T> double ProcessAudio<T>::getLengthInSeconds() const
{
  return double(getNumSamplesPerChannel()) / double(sampleRate);
}

template<typename SignedType> typename std::make_unsigned_t<SignedType> convertSignedToUnsigned(SignedType signedValue)
{
  static_assert(std::is_signed_v<SignedType>, "The input to this function must be a signed value.");

  typename std::make_unsigned_t<SignedType> unsignedValue =
    static_cast<typename std::make_unsigned_t<SignedType>>(1) + std::numeric_limits<SignedType>::max();

  unsignedValue += signedValue;

  return unsignedValue;
}


template<class T> T AudioSampleConverter<T>::signedByteToSample(int8_t sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(sample) / static_cast<T>(127.);// NOLINT
  } else if constexpr (std::numeric_limits<T>::is_integer) {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<T>(sample);
    } else {
      return static_cast<T>(convertSignedToUnsigned<int8_t>(sample));
    }
  }
}

template<class T> int8_t AudioSampleConverter<T>::sampleToSignedByte(T sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    sample = clamp(sample, -1., 1.);
    return static_cast<int8_t>(sample * T(0x7F));// NOLINT
  } else {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<int8_t>(clamp(sample, -128, 127));// NOLINT
    } else {
      return static_cast<int8_t>(clamp(sample, 0, 255) - 128);// NOLINT
    }
  }
}

template<class T> T AudioSampleConverter<T>::unsignedByteToSample(uint8_t sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(sample - 128) / static_cast<T>(127.);// NOLINT
  } else if (std::numeric_limits<T>::is_integer) {
    if constexpr (std::is_unsigned_v<T>) {
      return static_cast<T>(sample);
    } else {
      return static_cast<T>(sample - 128);// NOLINT
    }
  }
}

template<class T> uint8_t AudioSampleConverter<T>::sampleToUnsignedByte(T sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    sample = clamp(sample, -1., 1.);
    sample = (sample + 1.) / 2.;// NOLINT
    return static_cast<uint8_t>(1 + (sample * 254));// NOLINT
  } else {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<uint8_t>(clamp(sample, -128, 127) + 128);// NOLINT
    } else {
      return static_cast<uint8_t>(clamp(sample, 0, 255));// NOLINT
    }
  }
}

template<class T> T AudioSampleConverter<T>::sixteenBitIntToSample(int16_t sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(sample) / static_cast<T>(32767.);// NOLINT
  } else if constexpr (std::numeric_limits<T>::is_integer) {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<T>(sample);
    } else {
      return static_cast<T>(convertSignedToUnsigned<int16_t>(sample));
    }
  }
}

template<class T> int16_t AudioSampleConverter<T>::sampleToSixteenBitInt(T sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    sample = clamp(sample, -1., 1.);
    return static_cast<int16_t>(sample * 32767.);// NOLINT
  } else {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<int16_t>(clamp(sample, SignedInt16_Min, SignedInt16_Max));
    } else {
      return static_cast<int16_t>(clamp(sample, UnsignedInt16_Min, UnsignedInt16_Max) + SignedInt16_Min);
    }
  }
}

template<class T> T AudioSampleConverter<T>::twentyFourBitIntToSample(int32_t sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(sample) / static_cast<T>(8388607.);// NOLINT
  } else if (std::numeric_limits<T>::is_integer) {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<T>(clamp(sample, SignedInt24_Min, SignedInt24_Max));
    } else {
      return static_cast<T>(clamp(sample + 8388608, UnsignedInt24_Min, UnsignedInt24_Max));// NOLINT
    }
  }
}

template<class T> int32_t AudioSampleConverter<T>::sampleToTwentyFourBitInt(T sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    sample = clamp(sample, -1., 1.);
    return static_cast<int32_t>(sample * 8388607.);// NOLINT
  } else {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<int32_t>(clamp(sample, SignedInt24_Min, SignedInt24_Max));
    } else {
      return static_cast<int32_t>(clamp(sample, UnsignedInt24_Min, UnsignedInt24_Max) + SignedInt24_Min);
    }
  }
}

template<class T> T AudioSampleConverter<T>::thirtyTwoBitIntToSample(int32_t sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    return static_cast<T>(sample) / static_cast<T>(std::numeric_limits<int32_t>::max());
  } else if (std::numeric_limits<T>::is_integer) {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<T>(sample);
    } else {
      return static_cast<T>(clamp(static_cast<T>(sample + 2147483648), 0, 4294967295));// NOLINT
    }
  }
}

template<class T> int32_t AudioSampleConverter<T>::sampleToThirtyTwoBitInt(T sample)
{
  if constexpr (std::is_floating_point_v<T>) {
    if constexpr (std::is_same_v<T, float>) {
      if (sample >= 1.f) {// NOLINT
        return std::numeric_limits<int32_t>::max();
      } else if (sample <= -1.f) {// NOLINT
        return std::numeric_limits<int32_t>::lowest() + 1;
      } else {
        return static_cast<int32_t>(sample * std::numeric_limits<int32_t>::max());
      }
    } else {
      return static_cast<int32_t>(clamp(sample, -1., 1.) * std::numeric_limits<int32_t>::max());
    }
  } else {
    if constexpr (std::is_signed_v<T>) {
      return static_cast<int32_t>(clamp(sample, -2147483648LL, 2147483647LL));// NOLINT
    } else {
      return static_cast<int32_t>(clamp(sample, 0, 4294967295) - 2147483648);// NOLINT
    }
  }
}

template<class T> T AudioSampleConverter<T>::clamp(T value, T minValue, T maxValue)// NOLINT
{
  value = std::min(value, maxValue);
  value = std::max(value, minValue);
  return value;
}

// Explicit Template Instantiation
template class ProcessAudio<float>;
}// namespace asf
