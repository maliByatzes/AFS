#include <afsproject/flac_file.h>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <etl/bit_stream.h>
#include <etl/endianness.h>
#include <etl/span.h>
#include <fstream>
#include <ios>
#include <iostream>
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
  const etl::span<const uint8_t> data_span(m_file_data.data(), m_file_data.size());
  etl::bit_stream_reader reader(data_span, etl::endian::big);

  auto flac_marker = reader.read<uint32_t>(32).value();// NOLINT

  std::string flac_marker_str{};
  // NOLINTBEGIN
  flac_marker_str += static_cast<char>((flac_marker >> 24) & 0xFF);
  flac_marker_str += static_cast<char>((flac_marker >> 16) & 0xFF);
  flac_marker_str += static_cast<char>((flac_marker >> 8) & 0xFF);
  flac_marker_str += static_cast<char>(flac_marker & 0xFF);
  // NOLINTEND

  if (flac_marker_str != "fLaC") {
    std::cerr << "Invalid FLAC marker.\n";
    return false;
  }

  // TODO:explicitly check if the first metadata is the `STREAM_INFO`

  // process an unknown amount of metadata blocks
  while (true) {
    auto is_last = reader.read<uint8_t>(1).value();
    auto block_type = reader.read<uint8_t>(7).value();// NOLINT
    auto block_size = reader.read<uint32_t>(24).value();// NOLINT

    std::cout << "Current metadata block - Last: " << static_cast<int>(is_last)
              << ", Type: " << static_cast<int>(block_type) << ", Size: " << block_size << "\n";

    switch (static_cast<int>(block_type)) {
    case 0:
      std::cout << "Processin' the stream info header.\n";
      if (!decodeStreaminfo(reader, block_size)) { return false; }
      break;
    case 1:
      // decodePadding();
      std::cout << "Skippin' padding metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 2:
      // decodeApplication();
      std::cout << "Skippin' application metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 3:
      // decodeSeektable();
      std::cout << "Skippin' seek table metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 4:
      // decodeVorbiscomment();
      std::cout << "Skippin' vorbis comment metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 5:// NOLINT
           // decodeCuesheet();
      std::cout << "Skippin' cue sheet metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 6:// NOLINT
           // decodePicture();
      std::cout << "Skippin' picture metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    case 127:// NOLINT
      throw std::runtime_error("This metadata block type is forbidden.\n");
    default:
      std::cerr << "Unsupported/Reserved metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      break;
    }

    if (is_last == 1) {
      std::cout << "Last metadata block processed.\n";
      break;
    }
  }

  return false;
}

bool FlacFile::decodeStreaminfo([[maybe_unused]] etl::bit_stream_reader &reader,// NOLINT
  [[maybe_unused]] uint32_t block_size)
{
  if (block_size != 34) { std::cerr << "Invalid STREAMINFO block size: " << block_size << " (expected 34)\n"; }// NOLINT

  // u(16) -> minimum block size
  auto min_block_size = reader.read<uint16_t>(16).value();// NOLINT
  // u(16) -> maximum block size
  auto max_block_size = reader.read<uint16_t>(16).value();// NOLINT
  // u(24) -> minimum frame size
  auto min_frame_size = reader.read<uint32_t>(24).value();// NOLINT
  // u(24) -> maximum frame size
  auto max_frame_size = reader.read<uint32_t>(24).value();// NOLINT
  // u(20) -> sample rate in Hz
  auto sample_rate = reader.read<uint32_t>(20).value();// NOLINT
  // u(3) -> number of channels - 1
  auto num_channels = reader.read<uint8_t>(3).value();
  // u(5) -> bits per sample - 1
  auto bits_per_samples = reader.read<uint8_t>(5).value();// NOLINT
  // u(36) -> total number of interchannel samples
  auto total_samples = reader.read<uint64_t>(36).value();// NOLINT

  // u(128) -> MD5 checksum -> skip
  reader.skip(128);// NOLINT

  std::cout << "STREAMINFO:\n"
            << " Min block size: " << min_block_size << "\n"
            << " Max block size: " << max_block_size << "\n"
            << " Min frame size: " << min_frame_size << "\n"
            << " Max frame size: " << max_frame_size << "\n"
            << " Sample rate: " << sample_rate << "\n"
            << " Channels: " << num_channels << "\n"
            << " Bits per sample: " << bits_per_samples << "\n"
            << " Total samples: " << total_samples << "\n";

  return true;
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
