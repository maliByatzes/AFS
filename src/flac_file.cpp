#include <afsproject/flac_file.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
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

bool FlacFile::save([[maybe_unused]] const std::string &file_path) const { return true; }

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

bool FlacFile::decodeFlacFile() { return true; }// NOLINT

bool FlacFile::encodeFlacFile() { return true; }// NOLINT
}// namespace afs
