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
    const int32_t first_header = convFourBytesToInt32(
      std::span(m_file_data.begin() + position, m_file_data.begin() + position + 4), std::endian::big);
    position += 4;

    std::bitset<32> fh_bits(static_cast<uint32_t>(first_header));// NOLINT

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
