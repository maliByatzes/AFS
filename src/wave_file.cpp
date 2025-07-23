#include <algorithm>
#include <asfproject/either.h>
#include <asfproject/wave_file.h>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace asf {

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

std::vector<double> WaveFile::getPCMData() const { return m_pcm_data; };

int WaveFile::getSampleRate() const { return m_sample_rate; }

int WaveFile::getNumChannels() const { return m_num_channels; }

double WaveFile::getDurationSeconds() const { return double(getNumSamplesPerChannel()) / double(m_sample_rate); }

bool WaveFile::isMono() const { return getNumChannels() == 1; }

bool WaveFile::isStero() const { return getNumChannels() == 2; }

int WaveFile::getBitDepth() const { return m_bit_depth; }

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
  header_chunk.ck_size = convFourBytesToInt32(std::span(m_file_data.begin() + 4, m_file_data.end() + 8));// NOLINT
  header_chunk.file_type_header = { m_file_data.begin() + 8, m_file_data.begin() + 12 };// NOLINT

  return header_chunk;
}

Either<WaveFmtChunk, std::string> WaveFile::decodeFmtChunk()
{
  WaveFmtChunk fmt_chunk{};

  fmt_chunk.index = getIdxOfChunk("fmt ", 12)// NOLINT
                      .leftMap([](auto idx) { return int(idx); })
                      .rightMap([](auto &msg) {
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
                       .rightMap([](auto &msg) {
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

bool WaveFile::decodeSamples(const WaveFmtChunk &fmt_chunk, WaveDataChunk &data_chunk)
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

  return true;
}

bool WaveFile::decodeWaveFile()
{
  const WaveHeaderChunk header_chunk = decodeHeaderChunk();
  const WaveFmtChunk fmt_chunk = decodeFmtChunk()
                                   .leftMap([](auto fck) { return fck; })
                                   .rightMap([](auto &msg) {
                                     std::cerr << msg;
                                     WaveFmtChunk fmt_ck;
                                     fmt_ck.index = -1;
                                     return fmt_ck;
                                   })
                                   .join();
  WaveDataChunk data_chunk = decodeDataChunk();

  if (fmt_chunk.index == -1 || data_chunk.index == -1 || header_chunk.ck_id != "RIFF"
      || header_chunk.file_type_header != "WAVE") {
    return false;
  }

  m_sample_rate = fmt_chunk.sample_rate;
  m_bit_depth = fmt_chunk.bit_depth;
  m_num_channels = fmt_chunk.n_channels;
  m_format_tag = fmt_chunk.format_tag;

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
    if (ck_size > static_cast<int32_t>(m_file_data.size() - idx - req_len) || (ck_size < 0)) {
      const std::string msg{ "The chunk size we got is somehow invalid.\n" };
      return right(msg);
    }

    idx += (req_len + size_t(ck_size));
  }

  const std::string msg{ "We could not find a chunk with that ID.\n" };
  return right(msg);
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

}// namespace asf
