#include "process_audio.h"
#include <algorithm>
#include <limits>
#include <type_traits>

namespace asf
{

  template<class T>
  ProcessAudio<T>::ProcessAudio()
  {
    bitDepth = 16;
    sampleRate = 44100;
    samples.resize(1);
    samples[0].resize(0);
    fileFormat = AudioFormat::WAVE;
  }

  template<class T>
  ProcessAudio<T>::ProcessAudio(const std::string &fileName)
  : ProcessAudio<T>()
  {
    loadAudioFromFile(fileName);
  }

  template<class T>
  AudioFormat ProcessAudio<T>::getAudioFormat()
  {
    if (fileData.size() < 4)
      return AudioFormat::Error;
      
    std::string headerID (fileData.begin(), fileData.begin() + 4);

    if (headerID == "RIFF")
      return AudioFormat::WAVE;
    else
      return AudioFormat::Unsupported;
  }

  template<class T>
  int32_t ProcessAudio<T>::convertFourBytesToInt32(int startIdx, Endianness endianness)
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

  template<class T>
  int16_t ProcessAudio<T>::convertTwoBytesToInt16(int startIdx, Endianness endianness)
  {
    int16_t res;

    if (endianness == Endianness::LittleEndian)
      res = (fileData[startIdx + 1] << 8) | fileData[startIdx];
    else
      res = (fileData[startIdx] << 8) | fileData[startIdx + 1];  

    return res;
  }

  template<class T>
  int ProcessAudio<T>::getIndexOfChunk(const std::string &ckID, int startIdx, Endianness endianness)
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

  template<class T>
  void ProcessAudio<T>::clearAudioBuffer()
  {
    for (size_t i = 0; i < samples.size(); i++)
      samples[i].clear();
    samples.clear();
  }

  template<class T>
  Either<WaveHeaderChunk, std::string> ProcessAudio<T>::decodeHeaderChunk()
  {
    WaveHeaderChunk headerChunk {};
    
    headerChunk.ckID = { fileData.begin(), fileData.begin() + 4 };
    headerChunk.ckSize = convertFourBytesToInt32(4);
    headerChunk.fileTypeHeader = { fileData.begin() + 8, fileData.begin() + 12 };

    return left(headerChunk);
  }

  template<class T>
  Either<WaveFmtChunk, std::string> ProcessAudio<T>::decodeFmtChunk()
  {
    WaveFmtChunk fmtChunk {};

    fmtChunk.index = getIndexOfChunk("fmt ", 12);
    int f = fmtChunk.index;
    
    fmtChunk.ckID = { fileData.begin() + f, fileData.begin() + f + 4 };
    fmtChunk.ckSize = convertFourBytesToInt32(f + 4);
    fmtChunk.tag = convertTwoBytesToInt16(f + 8);
    fmtChunk.nChannels = convertTwoBytesToInt16(f + 10);
    fmtChunk.sampleRate = convertFourBytesToInt32(f + 12);
    fmtChunk.nAvgBytesPerSec = convertFourBytesToInt32(f + 16);
    fmtChunk.nBlockAlign = convertTwoBytesToInt16(f + 20);
    fmtChunk.bitDepth = convertTwoBytesToInt16(f + 22);

    if (fmtChunk.tag != WaveFormat::PCM) {
      std::string msg { "That wave format is not supported or not a wave format at all.\n" };
      return right(msg);
    }

    if (fmtChunk.nChannels < 1 || fmtChunk.nChannels > 128) {
      std::string msg { "Number of channels is just too high or too low, who knows.\n" };
      return right(msg);
    }

    if (fmtChunk.nAvgBytesPerSec != static_cast<uint32_t>((fmtChunk.nChannels * fmtChunk.sampleRate * fmtChunk.bitDepth) / 8)) {
      std::string msg { "The calculation is not mathing bruh, it ain't right.\n" };
      return right(msg);
    }

    if (fmtChunk.bitDepth != 8 && fmtChunk.bitDepth != 16 && fmtChunk.bitDepth != 24 && fmtChunk.bitDepth != 32) {
      std::string msg { "Somehow this bit depth is not valid, HOW!?\n" };
      return right(msg);
    }

    return left(fmtChunk);
  }

  template<class T>
  Either<WaveDataChunk, std::string> ProcessAudio<T>::decodeDataChunk()
  {
    WaveDataChunk dataChunk {};

    dataChunk.index = getIndexOfChunk("data", 12);
    int d = dataChunk.index;
    
    dataChunk.ckID = { fileData.begin() + d, fileData.begin() + d + 4 };
    dataChunk.ckSize = convertFourBytesToInt32(d + 4);

    return left(dataChunk);  
  }

  template<class T>  
  bool ProcessAudio<T>::decodeSamples(const WaveFmtChunk &fmtChunk, WaveDataChunk &dataChunk)
  {
    dataChunk.nSamples = dataChunk.ckSize / (fmtChunk.nChannels * fmtChunk.bitDepth / 8);
    uint16_t numBytesPerSample = fmtChunk.bitDepth / 8;
    int samplesStartIdx = dataChunk.index + 8;

    clearAudioBuffer();
    samples.resize(fmtChunk.nChannels);

    for (int i = 0; i < dataChunk.nSamples; i++) {
      for (int channel = 0; channel < fmtChunk.nChannels; channel++) {

        int sampleIndex = samplesStartIdx + (fmtChunk.nBlockAlign * i) + channel * numBytesPerSample;

        if ((sampleIndex + (fmtChunk.bitDepth / 8) - 1) >= fileData.size()) {
          std::cerr << "There is somehow more samples than there are in the file data.\n";
          return false;    
        }

        if (fmtChunk.bitDepth == 8) {
          T sample = AudioSampleConverter<T>::unsignedByteToSample(fileData[sampleIndex]);
          samples[channel].push_back(sample);
        }
        else if (fmtChunk.bitDepth == 16) {
          int16_t sampleAsInt = convertTwoBytesToInt16(sampleIndex);
          T sample = AudioSampleConverter<T>::sixteenBitIntToSample(sampleAsInt);
          samples[channel].push_back(sample);
        }
        else if (fmtChunk.bitDepth == 24) {
          int32_t sampleAsInt;
          sampleAsInt = (fileData[sampleIndex + 2] << 16) | (fileData[sampleIndex + 1] << 8) | fileData[sampleIndex];

          if (sampleAsInt & 0x800000)
            sampleAsInt = sampleAsInt | ~0xFFFFFF;

          T sample = AudioSampleConverter<T>::twentyFourBitIntToSample(sampleAsInt);
          samples[channel].push_back(sample);
        }
        else if (fmtChunk.bitDepth == 32) {
          int32_t sampleAsInt = convertFourBytesToInt32(sampleIndex);
          T sample = AudioSampleConverter<T>::thirtyTwoBitIntToSample(sampleAsInt);
          samples[channel].push_back(sample);
        }
        else {
          // Shouldn't execute 
          return false;
        }
      }
    }

    return true;
  }
  
  template<class T>
  bool ProcessAudio<T>::loadWaveFile()
  {

    WaveHeaderChunk headerChunk = decodeHeaderChunk()
      .leftMap([](auto hCk) { return hCk; })
      .rightMap([](auto msg) { std::cerr << msg; return WaveHeaderChunk{}; })
      .join();

    WaveFmtChunk fmtChunk = decodeFmtChunk()
      .leftMap([](auto fCk) { return fCk; })
      .rightMap([](auto msg) { std::cerr << msg; return WaveFmtChunk{ .index = -1 }; })
      .join();

    WaveDataChunk dataChunk = decodeDataChunk()
      .leftMap([](auto dCk) { return dCk; })
      .rightMap([](auto msg) { std::cerr << msg; return WaveDataChunk { .index = -1 }; })
      .join();

    if (fmtChunk.index == -1 || dataChunk.index == -1 || headerChunk.ckID != "RIFF" || headerChunk.fileTypeHeader != "WAVE") {
      // std::cerr << "This is not a valid .WAV file.\n";
      return false;
    }

    sampleRate = fmtChunk.sampleRate;
    bitDepth = fmtChunk.bitDepth;

    return decodeSamples(fmtChunk, dataChunk);
  }

  template<class T>
  bool ProcessAudio<T>::loadAudioFromFile(const std::string &fileName)
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
  

  template<typename SignedType>
  typename std::make_unsigned<SignedType>::type convertSignedToUnsigned(SignedType signedValue)
  {
    static_assert(std::is_signed<SignedType>::value, "The input to this function must be a signed value.");

    typename std::make_unsigned<SignedType>::type unsignedValue = static_cast<typename std::make_unsigned<SignedType>::type>(1) + std::numeric_limits<SignedType>::max();

    unsignedValue += signedValue;

    return unsignedValue;
  }

  
  template<class T>
  T AudioSampleConverter<T>::signedByteToSample(int8_t sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
      return static_cast<T>(sample) / static_cast<T>(127.);
    } else if constexpr (std::numeric_limits<T>::is_integer) {
      if constexpr (std::is_signed_v<T>)
        return static_cast<T>(sample);
      else
        return static_cast<T>(convertSignedToUnsigned<int8_t>(sample));
    }
  }

  template<class T>
  int8_t AudioSampleConverter<T>::sampleToSignedByte(T sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
      sample = clamp(sample, -1., 1.);
      return static_cast<int8_t>(sample * (T)0x7F);
    } else {
      if constexpr (std::is_signed_v<T>)
        return static_cast<int8_t>(clamp (sample, -128, 127));
      else
        return static_cast<int8_t>(clamp (sample, 0, 255) - 128);
    }
  }

  template<class T>
  T AudioSampleConverter<T>::unsignedByteToSample(uint8_t sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
      return static_cast<T>(sample - 128) / static_cast<T>(127.);
    } else if (std::numeric_limits<T>::is_integer) {
      if constexpr (std::is_unsigned_v<T>)
        return static_cast<T>(sample);
      else
        return static_cast<T>(sample - 128);
    }
  }

  template<class T>
  uint8_t AudioSampleConverter<T>::sampleToUnsignedByte(T sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        sample = clamp (sample, -1., 1.);
        sample = (sample + 1.) / 2.;
        return static_cast<uint8_t> (1 + (sample * 254));
    } else {
        if constexpr (std::is_signed_v<T>)
            return static_cast<uint8_t> (clamp (sample, -128, 127) + 128);
        else
            return static_cast<uint8_t> (clamp (sample, 0, 255));
    }
  }

  template<class T>
  T AudioSampleConverter<T>::sixteenBitIntToSample(int16_t sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        return static_cast<T> (sample) / static_cast<T> (32767.);
    } else if constexpr (std::numeric_limits<T>::is_integer) {
        if constexpr (std::is_signed_v<T>)
            return static_cast<T> (sample);
        else
            return static_cast<T> (convertSignedToUnsigned<int16_t> (sample));
    }
  }

  template<class T>
  int16_t AudioSampleConverter<T>::sampleToSixteenBitInt(T sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        sample = clamp (sample, -1., 1.);
        return static_cast<int16_t> (sample * 32767.);
    } else {
        if constexpr (std::is_signed_v<T>)
            return static_cast<int16_t> (clamp (sample, SignedInt16_Min, SignedInt16_Max));
        else
            return static_cast<int16_t> (clamp (sample, UnsignedInt16_Min, UnsignedInt16_Max) + SignedInt16_Min);
    }
    
  } 

  template<class T>
  T AudioSampleConverter<T>::twentyFourBitIntToSample(int32_t sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        return static_cast<T> (sample) / static_cast<T> (8388607.);
    } else if (std::numeric_limits<T>::is_integer) {
        if constexpr (std::is_signed_v<T>)
            return static_cast<T> (clamp (sample, SignedInt24_Min, SignedInt24_Max));
        else
            return static_cast<T> (clamp (sample + 8388608, UnsignedInt24_Min, UnsignedInt24_Max));
    }
  }

  template<class T>
  int32_t AudioSampleConverter<T>::sampleToTwentyFourBitInt(T sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        sample = clamp (sample, -1., 1.);
        return static_cast<int32_t> (sample * 8388607.);
    } else {
        if constexpr (std::is_signed_v<T>)
            return static_cast<int32_t> (clamp (sample, SignedInt24_Min, SignedInt24_Max));
        else
            return static_cast<int32_t> (clamp (sample, UnsignedInt24_Min, UnsignedInt24_Max) + SignedInt24_Min);
    }
  }

  template<class T>
  T AudioSampleConverter<T>::thirtyTwoBitIntToSample(int32_t sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        return static_cast<T> (sample) / static_cast<T> (std::numeric_limits<int32_t>::max());
    } else if (std::numeric_limits<T>::is_integer) {
        if constexpr (std::is_signed_v<T>)
			return static_cast<T> (sample);
        else
            return static_cast<T> (clamp (static_cast<T> (sample + 2147483648), 0, 4294967295));
    }
  }

  template<class T>
  int32_t AudioSampleConverter<T>::sampleToThirtyTwoBitInt(T sample)
  {
    if constexpr (std::is_floating_point<T>::value) {
        if constexpr (std::is_same_v<T, float>) {
            if (sample >= 1.f)
                return std::numeric_limits<int32_t>::max();
            else if (sample <= -1.f)
                return std::numeric_limits<int32_t>::lowest() + 1;
            else
                return static_cast<int32_t> (sample * std::numeric_limits<int32_t>::max());
        } else {
            return static_cast<int32_t> (clamp (sample, -1., 1.) * std::numeric_limits<int32_t>::max());
        }
    } else {
        if constexpr (std::is_signed_v<T>)
            return static_cast<int32_t> (clamp (sample, -2147483648LL, 2147483647LL));
        else
            return static_cast<int32_t> (clamp (sample, 0, 4294967295) - 2147483648);
    }
  }

  template<class T>
  T AudioSampleConverter<T>::clamp(T value, T minValue, T maxValue)
  {
    value = std::min(value, maxValue);
    value = std::max(value, minValue);
    return value;
  }

  // Explicit Template Instantiation
  template class ProcessAudio<float>;
}
