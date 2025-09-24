#include "afsproject/wave_file.h"
#include <afsproject/flac_file.h>
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

  m_file_data.reserve(static_cast<size_t>(file_length));

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

  long position = 4;

  // TODO:explicitly check if the first metadata is the `STREAM_INFO`

  
  // process an unknown amount of metadata blocks
  while (true) {
    int32_t first_header =
      convFourBytesToInt32(std::span(m_file_data.begin() + position, m_file_data.begin() + position + 4));
    position += 4;

    uint8_t *first_byte_ptr = reinterpret_cast<uint8_t *>(&first_header);// NOLINT
    const uint8_t first_byte = *first_byte_ptr;
    uint8_t first_bit = (first_byte >> 7) & 0x01;// NOLINT
    uint8_t rem_7_bits = first_byte & 0x7F;// NOLINT

    uint8_t second_byte = *(first_byte_ptr + 1);// NOLINT
    uint8_t third_byte = *(first_byte_ptr + 2);// NOLINT
    uint8_t fourth_byte = *(first_byte_ptr + 3);// NOLINT

    uint32_t header_size = (static_cast<uint32_t>(second_byte) << 16) | (static_cast<uint32_t>(third_byte) << 8)// NOLINT
                           | static_cast<uint32_t>(fourth_byte);
    position += header_size + 1;// NOTE: not sure abt this +1, test later

    switch (static_cast<int>(rem_7_bits)) {
    case 0:
      std::cout << "Processin' the stream info header.\n";
      decodeStreaminfo(header_size);
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
    if (static_cast<int>(first_bit) == 1) { break; }
  }

  return false;
}

bool FlacFile::decodeStreaminfo(uint32_t header_size)
{
  
}

bool FlacFile::decodePadding() { return false; }// NOLINT

bool FlacFile::decodeApplication() { return false; }// NOLINT

bool FlacFile::decodeSeektable() { return false; }// NOLINT

bool FlacFile::decodeVorbiscomment() { return false; }// NOLINT

bool FlacFile::decodeCuesheet() { return false; }// NOLINT

bool FlacFile::decodePicture() { return false; }// NOLINT

bool FlacFile::encodeFlacFile() { return false; }// NOLINT
}// namespace afs
