#include <afsproject/either.h>
#include <afsproject/wave_file.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace afs {

bool WaveFile::load(const std::string &file_path)
{
  std::ifstream file(file_path, std::ios_base::binary);

  if (!file.good()) {
    std::cerr << "Opening that file failed badly or it doesn't exist in this universe: " << file_path << "\n";
    return false;
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_length = file.tellg();
  file.seekg(0, std::ios::beg);

  m_file_data.resize(static_cast<size_t>(file_length));

  file.read(reinterpret_cast<char *>(m_file_data.data()), file_length);// NOLINT
  file.close();

  if (file.gcount() != file_length) {
    std::cerr << "Uh oh that did not read the entire file data.\n";
    return false;
  }

  constexpr int MIN_FILE_DATA_SIZE = 12;
  if (m_file_data.size() < MIN_FILE_DATA_SIZE) {
    std::cerr << "The file data size is just too short mate.\n";
    return false;
  }

  return decodeWaveFile();
}

bool WaveFile::save(const std::string &file_path) const
{
  std::vector<uint8_t> data{};
  return encodeWaveFile(data) && writeDataToFile(data, file_path);
}

std::vector<double> WaveFile::getPCMData() const { return m_pcm_data; };

uint32_t WaveFile::getSampleRate() const { return m_sample_rate; }

uint16_t WaveFile::getNumChannels() const { return m_num_channels; }

double WaveFile::getDurationSeconds() const { return double(getNumSamplesPerChannel()) / double(m_sample_rate); }

bool WaveFile::isMono() const { return getNumChannels() == 1; }

bool WaveFile::isStereo() const { return getNumChannels() == 2; }

uint16_t WaveFile::getBitDepth() const { return m_bit_depth; }

int WaveFile::getNumSamplesPerChannel() const
{
  if (!m_pcm_data.empty()) {
    return int(m_pcm_data.size());
  } else {
    return 0;
  }
}

WaveHeaderChunk WaveFile::decodeHeaderChunk()
{
  WaveHeaderChunk header_chunk{};

  header_chunk.ck_id = { m_file_data.begin(), m_file_data.begin() + 4 };
  header_chunk.ck_size = convFourBytesToInt32(std::span(m_file_data.begin() + 4, m_file_data.begin() + 8));// NOLINT
  header_chunk.file_type_header = { m_file_data.begin() + 8, m_file_data.begin() + 12 };// NOLINT

  return header_chunk;
}

Either<WaveFmtChunk, std::string> WaveFile::decodeFmtChunk()
{
  WaveFmtChunk fmt_chunk{};

  fmt_chunk.index = getIdxOfChunk("fmt ", 12)// NOLINT
                      .leftMap([](auto idx) { return int(idx); })
                      .rightMap([](const auto &msg) {
                        std::cerr << msg;
                        return -1;
                      })
                      .join();

  int f = fmt_chunk.index;// NOLINT
  auto idx = m_file_data.begin() + f;

  fmt_chunk.ck_id = { idx, idx + 4 };
  // NOLINTBEGIN
  fmt_chunk.ck_size = convFourBytesToInt32(std::span(idx + 4, idx + 8));
  fmt_chunk.format_tag = convTwoBytesToInt16(std::span(idx + 8, idx + 10));
  fmt_chunk.n_channels = convTwoBytesToInt16(std::span(idx + 10, idx + 12));
  fmt_chunk.sample_rate = convFourBytesToInt32(std::span(idx + 12, idx + 16));
  fmt_chunk.n_avg_bytes_per_sec = convFourBytesToInt32(std::span(idx + 16, idx + 20));
  fmt_chunk.n_block_align = convTwoBytesToInt16(std::span(idx + 20, idx + 22));
  fmt_chunk.bit_depth = convTwoBytesToInt16(std::span(idx + 22, idx + 24));
  // NOLINTEND

  // TODO: Add more format tags to handle here
  /*if (fmt_chunk.format_tag != WaveFormat::PCM) {
    const std::string msg{ "That wave format is not supported or not a wave format at all.\n" };
    return right(msg);
  }*/

  if (fmt_chunk.n_channels < 1 || fmt_chunk.n_channels > 128) {// NOLINT
    const std::string msg{ "The number of channels is just to high or too low, who knows.\n" };
    return right(msg);
  }

  if (fmt_chunk.n_avg_bytes_per_sec
      != (fmt_chunk.n_channels * fmt_chunk.sample_rate * fmt_chunk.bit_depth) / 8) {// NOLINT
    const std::string msg{ "The calculation is not mathing bruh, it ain't right.\n" };
    return right(msg);
  }

  if (fmt_chunk.bit_depth != 8 && fmt_chunk.bit_depth != 16 && fmt_chunk.bit_depth != 24// NOLINT
      && fmt_chunk.bit_depth != 32) {// NOLINT
    const std::string msg{ "Somehow this bit depth is not valid, HOW!?\n" };
    return right(msg);
  }

  return left(fmt_chunk);
}

WaveDataChunk WaveFile::decodeDataChunk()
{
  WaveDataChunk data_chunk{};

  data_chunk.index = getIdxOfChunk("data", 12)// NOLINT
                       .leftMap([](auto idx) { return int(idx); })
                       .rightMap([](const auto &msg) {
                         std::cerr << msg;
                         return -1;
                       })
                       .join();

  int d = data_chunk.index;// NOLINT
  auto idx = m_file_data.begin() + d;

  data_chunk.ck_id = { idx, idx + 4 };
  data_chunk.ck_size = convFourBytesToInt32(std::span(idx + 4, idx + 8));// NOLINT

  return data_chunk;
}

void WaveFile::decode8Bits(const WaveFmtChunk &fmt_chunk, const std::vector<uint8_t> &data)
{
  if (fmt_chunk.n_channels == 1) {
    for (size_t i = 0; i < m_pcm_data.size(); ++i) { m_pcm_data[i] = data[i]; }
  } else {
    throw std::domain_error("That quantity of channels is still unsupported.");
  }
}

void WaveFile::decode16Bits(const WaveFmtChunk &fmt_chunk, const std::vector<uint8_t> &data)
{
  if (fmt_chunk.n_channels == 1) {
    for (size_t i = 0, k = 0; i < m_pcm_data.size(); ++i, k += 2) {
      double sample = static_cast<int16_t>((data[k + 1] << 8) | data[k]);// NOLINT
      m_pcm_data[i] = sample;
    }
  } else if (fmt_chunk.n_channels == 2) {
    size_t k = 0;// NOLINT
    for (size_t i = 0; i < m_pcm_data.size() - 1; i += 2) {
      int16_t leftSample = static_cast<int16_t>((data[k + 1] << 8) | data[k]);// NOLINT
      int16_t rightSample = static_cast<int16_t>((data[k + 3] << 8) | data[k + 2]);// NOLINT
      m_pcm_data[i] = leftSample;
      m_pcm_data[i + 1] = rightSample;// NOLINT
      k += 4;
    }
  } else {
    throw std::domain_error("That quantity of chanels is still unsupported.");
  }
}

bool WaveFile::decodeSamples(const WaveFmtChunk &fmt_chunk, const WaveDataChunk &data_chunk)
{
  std::vector<uint8_t> data(size_t(data_chunk.ck_size));
  int offset = data_chunk.index + 8;// NOLINT

  if (offset + data_chunk.ck_size <= int(m_file_data.size())) {
    std::copy(m_file_data.begin() + offset, m_file_data.begin() + offset + data_chunk.ck_size, data.begin());
  }

  int bytes_per_sample = fmt_chunk.bit_depth / 8;// NOLINT
  auto num_samples = size_t(data_chunk.ck_size / (bytes_per_sample * fmt_chunk.n_channels));

  m_pcm_data.clear();
  m_pcm_data.resize(num_samples);

  if (fmt_chunk.bit_depth == 8) {// NOLINT
    decode8Bits(fmt_chunk, data);
  } else if (fmt_chunk.bit_depth == 16) {// NOLINT
    decode16Bits(fmt_chunk, data);
  } else if (fmt_chunk.bit_depth == 24) {// NOLINT
    assert(true && "Not implemented!");
  } else if (fmt_chunk.bit_depth == 32) {// NOLINT
    assert(true && "Not implemented!");
  }

  // Scale down samples to [-1,1]
  m_pcm_data = scaleDownSamples(m_pcm_data);

  return true;
}

bool WaveFile::decodeWaveFile()
{
  const WaveHeaderChunk header_chunk = decodeHeaderChunk();
  const WaveFmtChunk fmt_chunk = decodeFmtChunk()
                                   .leftMap([](auto fck) { return fck; })
                                   .rightMap([](const auto &msg) {
                                     std::cerr << msg;
                                     WaveFmtChunk fmt_ck;
                                     fmt_ck.index = -1;
                                     return fmt_ck;
                                   })
                                   .join();
  const WaveDataChunk data_chunk = decodeDataChunk();

  if (fmt_chunk.index == -1 || data_chunk.index == -1 || header_chunk.ck_id != "RIFF"
      || header_chunk.file_type_header != "WAVE") {
    return false;
  }

  m_sample_rate = uint32_t(fmt_chunk.sample_rate);
  m_bit_depth = uint16_t(fmt_chunk.bit_depth);
  m_num_channels = uint16_t(fmt_chunk.n_channels);
  m_format_tag = uint16_t(fmt_chunk.format_tag);

  return decodeSamples(fmt_chunk, data_chunk);
}

Either<size_t, std::string> WaveFile::getIdxOfChunk(const std::string &ck_id, size_t start_idx)
{
  constexpr size_t req_len = 4;

  if (ck_id.size() != req_len) {
    const std::string msg{ "The chunk ID length is not valid because its length is not 4.\n" };
    return right(msg);
  }

  size_t idx = start_idx;
  while (idx < m_file_data.size() - req_len) {
    if (std::memcmp(&m_file_data[idx], ck_id.data(), req_len) == 0) { return left(idx); }

    idx += req_len;

    if ((idx + 4) >= m_file_data.size()) {
      const std::string msg{ "It seems like we have ran out of data to read.\n" };
      return right(msg);
    }

    const int32_t ck_size =
      convFourBytesToInt32(std::span(m_file_data.begin() + long(idx), m_file_data.begin() + long(idx) + 4));
    if (ck_size > static_cast<int32_t>(m_file_data.size() - idx - req_len) || (ck_size < 0)) {// NOLINT
      const std::string msg{ "The chunk size we got is somehow invalid.\n" };
      return right(msg);
    }

    idx += (req_len + size_t(ck_size));
  }

  const std::string msg{ "We could not find a chunk with that ID.\n" };
  return right(msg);
}


bool WaveFile::writeDataToFile(const std::vector<uint8_t> &data, const std::string &file_path)
{
  std::ofstream out_file(file_path, std::ios::binary);
  if (!out_file.is_open()) { return false; }

  out_file.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));// NOLINT
  out_file.close();
  return true;
}

bool WaveFile::encodeWaveFile(std::vector<uint8_t> &data) const
{
  encodeHeaderChunk(data);
  encodeFmtChunk(data);
  encodeDataChunk(data);

  return true;
}


void WaveFile::encodeHeaderChunk(std::vector<uint8_t> &data) const
{
  int32_t data_ck_size = getNumSamplesPerChannel() * (getNumChannels() * m_bit_depth / 8);// NOLINT
  int32_t fmt_ck_size = m_format_tag == int16_t(WaveFormat::PCM) ? 16 : 18;// NOLINT

  addStringToData(data, "RIFF");

  int32_t header_ck_size = 4 + fmt_ck_size + 8 + 8 + data_ck_size;// NOLINT
  addInt32ToData(data, header_ck_size);

  addStringToData(data, "WAVE");
}

void WaveFile::encodeFmtChunk(std::vector<uint8_t> &data) const
{
  int32_t fmt_ck_size = m_format_tag == int16_t(WaveFormat::PCM) ? 16 : 18;// NOLINT

  // ckID	4	Chunk ID: "fmt "
  addStringToData(data, "fmt ");
  // cksize	4	Chunk size: 16, 18 or 40
  addInt32ToData(data, fmt_ck_size);
  // wFormatTag	2	Format code
  addInt16ToData(data, int16_t(m_format_tag));
  // nChannels	2	Number of interleaved channels
  addInt16ToData(data, int16_t(getNumChannels()));
  // 	nSamplesPerSec	4	Sampling rate (blocks per second)
  addInt32ToData(data, int32_t(m_sample_rate));
  // nAvgBytesPerSec	4	Data rate
  addInt32ToData(data, int32_t(m_sample_rate * (m_bit_depth / 8) * getNumChannels()));// NOLINT
  // nBlockAlign	2	Data block size (bytes)
  addInt16ToData(data, int16_t((m_bit_depth / 8) * getNumChannels()));// NOLINT
  // wBitsPerSample	2	Bits per sample
  addInt16ToData(data, int16_t(m_bit_depth));
}

void WaveFile::encodeDataChunk(std::vector<uint8_t> &data) const
{
  // ckID	4	Chunk ID: "data"
  addStringToData(data, "data");
  // cksize	4	Chunk size: n
  int32_t data_ck_size = getNumSamplesPerChannel() * (getNumChannels() * m_bit_depth / 8);// NOLINT
  addInt32ToData(data, data_ck_size);// NOLINT

  // 	sampled data	n	Samples
  if (m_bit_depth == 8) {// NOLINT
    encode8Bits(data);
  } else if (m_bit_depth == 16) {// NOLINT
    encode16Bits(data);
  } else if (m_bit_depth == 24) {// NOLINT
    assert(true && "Not implemented!");
  } else if (m_bit_depth == 32) {// NOLINT
    assert(true && "Not implemented!");
  }
}

void WaveFile::encode8Bits(std::vector<uint8_t> &data) const
{
  if (getNumChannels() == 1) {
    for (const double sampleDbl : m_pcm_data) {
      const uint8_t sample =
        static_cast<uint8_t>(std::round(std::max(static_cast<double>(std::numeric_limits<uint8_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<uint8_t>::max()), sampleDbl))));
      data.push_back(sample);
    }
  } else {
    throw std::domain_error("That quantity of channels is not supported yet!");
  }
}

void WaveFile::encode16Bits(std::vector<uint8_t> &data) const
{
  if (getNumChannels() == 1) {
    for (const double sampleDbl : m_pcm_data) {
      const int16_t sample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), sampleDbl))));

      data.push_back(static_cast<uint8_t>(sample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));// NOLINT
    }
  } else if (getNumChannels() == 2) {
    for (size_t i = 0; i < m_pcm_data.size(); i += 2) {
      const int16_t leftSample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), m_pcm_data[i]))));

      const int16_t rightSample =
        static_cast<int16_t>(std::round(std::max(static_cast<double>(std::numeric_limits<int16_t>::min()),
          std::min(static_cast<double>(std::numeric_limits<int16_t>::max()), m_pcm_data[i + 1]))));

      data.push_back(static_cast<uint8_t>(leftSample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((leftSample >> 8) & 0xFF));// NOLINT

      data.push_back(static_cast<uint8_t>(rightSample & 0xFF));// NOLINT
      data.push_back(static_cast<uint8_t>((rightSample >> 8) & 0xFF));// NOLINT
    }
  } else {
    throw std::domain_error("That quantity of channels is unsupported.");
  }
}

int32_t convFourBytesToInt32(std::span<uint8_t> data, std::endian endianness)
{
  int32_t result = -1;

  if (endianness == std::endian::little) {
    result = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];// NOLINT
  } else {
    result = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];// NOLINT
  }

  return result;
}

int16_t convTwoBytesToInt16(std::span<uint8_t> data, std::endian endianness)
{
  int16_t result = -1;

  if (endianness == std::endian::little) {
    result = static_cast<int16_t>((data[1] << 8) | data[0]);// NOLINT
  } else {
    result = static_cast<int16_t>((data[0] << 8) | data[1]);// NOLINT
  }

  return result;
}

void addStringToData(std::vector<uint8_t> &data, const std::string &str)
{
  std::ranges::for_each(str, [&](char val) { data.push_back(uint8_t(val)); });
}

void addInt32ToData(std::vector<uint8_t> &data, int32_t value)
{
  const std::array<uint8_t, 4> bytes{ convInt32ToFourBytes(value) };
  std::ranges::copy(bytes, data.begin());
}

void addInt16ToData(std::vector<uint8_t> &data, int16_t value)
{
  const std::array<uint8_t, 2> bytes{ convInt16ToTwoBytes(value) };
  std::ranges::copy(bytes, data.begin());
}

std::array<uint8_t, 4> convInt32ToFourBytes(int32_t value, std::endian endianness)
{
  std::array<uint8_t, 4> bytes{};

  // NOLINTBEGIN
  if (endianness == std::endian::little) {
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

std::array<uint8_t, 2> convInt16ToTwoBytes(int16_t value, std::endian endianness)
{
  std::array<uint8_t, 2> bytes{};

  // NOLINTBEGIN
  if (endianness == std::endian::little) {
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[0] = static_cast<uint8_t>(value & 0xFF);
  } else {
    bytes[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    bytes[1] = static_cast<uint8_t>(value & 0xFF);
  }
  // NOLINTEND

  return bytes;
}

std::vector<double> scaleDownSamples(const std::vector<double> &samples)
{
  if (samples.empty()) { return {}; }

  double max_sample = 0.0;

  std::ranges::for_each(samples, [&max_sample](double sample) {
    auto abs_sample = std::abs(sample);
    max_sample = std::fmax(max_sample, abs_sample);
  });

  std::vector<double> scaled_samples(samples.size());

  if (max_sample == 0.0) {
    return samples;
  } else {
    for (size_t i = 0; i < samples.size(); ++i) { scaled_samples[i] = samples[i] / max_sample; }
  }

  return scaled_samples;
}

}// namespace afs
