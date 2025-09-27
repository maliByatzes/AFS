#include <afsproject/wave_file.h>
#include <afsproject/flac_file.h>
#include <bit>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// NOTE/TODO: should probably write the implementation somewhere
// once so i don't repeat same code over and over again.
namespace afs {

/*
 * FlacFile class implementation
 */

bool FlacFile::load(const std::string &file_path)
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

  m_bit_reader = std::make_unique<BitStreamReader>(m_file_data);
  
  // TODO: validate the minimum file data soze for flac files.

  return decodeFlacFile();
}

bool FlacFile::save([[maybe_unused]] const std::string &file_path) const { return false; }

std::vector<double> FlacFile::getPCMData() const { return m_pcm_data; }

int32_t FlacFile::getSampleRate() const { return m_sample_rate; }

int16_t FlacFile::getNumChannels() const { return m_num_channels; }

double FlacFile::getDurationSeconds() const { return double(getNumSamplesPerChannel()) / double(m_sample_rate); }

bool FlacFile::isMono() const { return getNumChannels() == 1; }

bool FlacFile::isStereo() const { return getNumChannels() == 2; }

int16_t FlacFile::getBitDepth() const { return m_bit_depth; }

int FlacFile::getNumSamplesPerChannel() const
{
  if (!m_pcm_data.empty()) {
    return int(m_pcm_data.size());
  } else {
    return 0;
  }
}

bool FlacFile::decodeFlacFile()
{
  // Process the FLAC marker
  [[maybe_unused]] const std::string flac_marker = { m_file_data.begin(), m_file_data.begin() + 4 };

  [[maybe_unused]] long position = 4;

  // TODO:explicitly check if the first metadata is the `STREAM_INFO`

  // process an unknown amount of metadata blocks
  while (true) {
    std::cout << "pos be4 reading: " << m_bit_reader->get_bit_position() << "\n";
    auto fs_header = m_bit_reader->read_bits(32);// NOLINT
    std::cout << "fs_header: " << fs_header << "\n";
    
    const int32_t first_header = convFourBytesToInt32(
      std::span(m_file_data.begin() + position, m_file_data.begin() + position + 4), std::endian::big);
    std::cout << "first_header: " << first_header << "\n";
    position += 4;

    std::bitset<32> fh_bits(static_cast<uint32_t>(first_header));// NOLINT
    std::cout << "fh_bits: " << fh_bits << "\n";

    [[maybe_unused]] auto header_size = static_cast<uint32_t>(extract_from_msb(fh_bits, 8, 24).to_ullong());// NOLINT
    [[maybe_unused]] const int first_bit = static_cast<int>(fh_bits[fh_bits.size() - 1]);
    auto rem_7_bits = static_cast<int>(extract_from_msb(fh_bits, 1, 7).to_ullong());// NOLINT

    switch (rem_7_bits) {
    case 0:
      std::cout << "Processin' the stream info header.\n";
      // decodeStreaminfo(header_size);
      break;
    case 1:
      decodePadding();
      break;
    case 2:
      decodeApplication();
      break;
    case 3:
      decodeSeektable();
      break;
    case 4:
      decodeVorbiscomment();
      break;
    case 5:// NOLINT
      decodeCuesheet();
      break;
    case 6:// NOLINT
      decodePicture();
      break;
    case 127:// NOLINT
      throw std::runtime_error("This metadata block type is forbidden.\n");
    default:
      std::cerr << "Unsupported/Reserved metadata block.\n";
      break;
    }

    break;
    // break if this metadata block is the last one
    if (first_bit == 1) { break; }
  }

  return false;
}

bool FlacFile::decodeStreaminfo([[maybe_unused]] uint32_t header_size, [[maybe_unused]] long position)// NOLINT
{
  // u(16) -> minimum block size


  // u(16) -> maximum block size
  // u(24) -> minimum frame size
  // u(24) -> maximum frame size
  // u(20) -> sample rate in Hz
  // u(3) -> number of channels - 1
  // u(5) -> bits per sample - 1
  // u(36) -> total number of interchannel samples
  // u(128) -> MD5 checksum

  return false;
}

bool FlacFile::decodePadding() { return false; }// NOLINT

bool FlacFile::decodeApplication() { return false; }// NOLINT

bool FlacFile::decodeSeektable() { return false; }// NOLINT

bool FlacFile::decodeVorbiscomment() { return false; }// NOLINT

bool FlacFile::decodeCuesheet() { return false; }// NOLINT

bool FlacFile::decodePicture() { return false; }// NOLINT

bool FlacFile::encodeFlacFile() { return false; }// NOLINT

/*
 * BitStreamReader class implementation
 */

BitStreamReader::BitStreamReader(std::span<const uint8_t> data) : m_data(data) {}

std::vector<uint8_t> BitStreamReader::extract_relevant_bytes(int num_bits)
{
  size_t start_byte = m_bit_position / 8;// NOLINT
  size_t end_byte = (m_bit_position + size_t(num_bits) - 1) / 8;// NOLINT
  [[maybe_unused]] const size_t num_bytes = end_byte - start_byte + 1;

  if (end_byte >= m_data.size()) { throw std::runtime_error("1. Not enough data"); }

  std::vector<uint8_t> bytes;
  for (size_t i = start_byte; i <= end_byte; ++i) { bytes.push_back(m_data[i]); }

  return bytes;
}

uint64_t BitStreamReader::read_bits(int num_bits, std::endian byte_order)
{
  assert(num_bits <= 64 && num_bits > 0);

  if ((m_bit_position % 8 == 0) && (num_bits % 8 == 0)) {// NOLINT
    return read_byte_aligned_bits(num_bits, byte_order);
  } else {
    return read_bit_aligned_bits(num_bits, byte_order);
  }
}

uint64_t BitStreamReader::read_byte_aligned_bits(int num_bits, std::endian byte_order)
{
  size_t start_byte = m_bit_position / 8;// NOLINT
  size_t num_bytes = size_t(num_bits) / 8;// NOLINT

  if (start_byte + num_bytes > m_data.size()) { throw std::runtime_error("2. Not enough data"); }

  uint64_t result = 0;

  if (byte_order == std::endian::little) {
    for (size_t i = 0; i < num_bytes; ++i) {
      result = (result << 8) | m_data[start_byte + i];// NOLINT
    }
  } else {
    for (size_t i = 0; i < num_bytes; ++i) {
      std::cout << "idx: " << start_byte + num_bytes - 1 - i << "\n\n";
      result = (result << 8) | m_data[start_byte + num_bytes - 1 - i];// NOLINT
    }
  }

  /*
  if (num_bits < 64) {// NOLINT
    result &= (1ULL << num_bits) - 1;// NOLINT
  }*/

  m_bit_position += size_t(num_bits);
  return result;
}

uint64_t BitStreamReader::read_bit_aligned_bits(int num_bits, std::endian byte_order)
{
  uint64_t result = 0;

std::cerr << "\n\nThe start of iteration loop through 0 -> num_bits.\n";
  for (int i = 0; i < num_bits; ++i) {
std::cerr << "i: " << i << ", num_bits: " << num_bits <<", m_bit_pos: " << m_bit_position << "\n";
    size_t byte_idx = m_bit_position / 8;// NOLINT
    size_t bit_in_byte = m_bit_position % 8;// NOLINT
std::cerr << "byte_idx: " << byte_idx << ", bit_in_byte: " << bit_in_byte << "\n";

std::cerr << "m_data.size(): " << m_data.size() << "\n";
    if (byte_idx >= m_data.size()) { throw std::runtime_error("3. Not enough data"); }

    bool bit = (m_data[byte_idx] >> (7 - bit_in_byte)) & 1;// NOLINT
std::cerr << "bit: " << (bit ? 1 : 0) << "\n";
    result = (result << 1) | bit;// NOLINT
std::cerr << "result: " << result << "\n";

std::cerr << "increment m_bit_pos.\n";
    ++m_bit_position;

std::cerr << "iteration done, next one inbound or not.\n\n";
  }

  if (byte_order == std::endian::little && num_bits >= 16 && (num_bits % 8) == 0) {// NOLINT
    result = swap_bytes(result, num_bits / 8);// NOLINT
  }

  return result;
}

uint64_t BitStreamReader::swap_bytes(uint64_t value, int num_bytes)// NOLINT
{
  uint64_t result = 0;
  for (int i = 0; i < num_bytes; ++i) {
    uint8_t byte = (value >> (i * 8)) & 0xFF;// NOLINT
    result = (result << 8) | byte;// NOLINT
  }
  return result;
}

void BitStreamReader::reset() { m_bit_position = 0; }

size_t BitStreamReader::get_bit_position() const { return m_bit_position; }
size_t BitStreamReader::get_remaining_bits() const { return (m_data.size() * 8) - m_bit_position; }// NOLINT

/*
 * Utility functions (temporary)
 */

std::bitset<THIRTY_TWO> extract_from_lsb(const std::bitset<THIRTY_TWO> &bits, size_t pos, int k)// NOLINT
{
  auto shifted = bits >> pos;
  std::bitset<THIRTY_TWO> mask((1 << k) - size_t(1));// NOLINT
  return shifted & mask;
}

std::bitset<THIRTY_TWO> extract_from_msb(const std::bitset<THIRTY_TWO> &bits, size_t pos, int k)// NOLINT
{
  const size_t lsb_pos = bits.size() - pos - size_t(k);
  auto shifted = bits >> lsb_pos;
  std::bitset<THIRTY_TWO> mask((1 << k) - size_t(1));// NOLINT
  return shifted & mask;
}

}// namespace afs
