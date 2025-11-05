#include <afsproject/audio_file.h>
#include <afsproject/flac_file.h>
#include <afsproject/md5.h>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <etl/bit_stream.h>
#include <etl/endianness.h>
#include <etl/span.h>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/types.h>
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

  file.read(reinterpret_cast<char *>(m_file_data.data()), file_length);
  file.close();

  if (file.gcount() != file_length) {
    std::cerr << "Uh oh that did not read the entire file data.\n";
    return false;
  }

  m_bits_read = 0;

  // TODO: validate the minimum file data soze for flac files.

  return decodeFlacFile();
}

bool FlacFile::save([[maybe_unused]] const std::string &file_path) const { return false; }

std::vector<double> FlacFile::getPCMData() const { return m_pcm_data; }

uint32_t FlacFile::getSampleRate() const { return m_sample_rate; }

uint16_t FlacFile::getNumChannels() const { return m_num_channels; }

double FlacFile::getDurationSeconds() const { return double(getNumSamplesPerChannel()) / double(m_sample_rate); }

bool FlacFile::isMono() const { return getNumChannels() == 1; }

bool FlacFile::isStereo() const { return getNumChannels() == 2; }

uint16_t FlacFile::getBitDepth() const { return m_bit_depth; }

int FlacFile::getNumSamplesPerChannel() const
{
  if (!m_pcm_data.empty()) {
    return int(m_pcm_data.size());
  } else {
    return 0;
  }
}

Metadata FlacFile::getMetadata() const { return m_metadata; }

bool FlacFile::decodeFlacFile()
{
  const etl::span<const uint8_t> data_span(m_file_data.data(), m_file_data.size());
  etl::bit_stream_reader reader(data_span, etl::endian::big);

  auto flac_marker = reader.read<uint32_t>(32).value();
  m_bits_read += 32;

  std::string flac_marker_str{};
  flac_marker_str += static_cast<char>((flac_marker >> 24U) & 0xFFU);
  flac_marker_str += static_cast<char>((flac_marker >> 16U) & 0xFFU);
  flac_marker_str += static_cast<char>((flac_marker >> 8U) & 0xFFU);
  flac_marker_str += static_cast<char>(flac_marker & 0xFFU);

  if (flac_marker_str != "fLaC") {
    std::cerr << "Invalid FLAC marker.\n";
    return false;
  }

  // TODO:explicitly check if the first metadata is the `STREAM_INFO`

  // process an unknown amount of metadata blocks
  while (true) {
    auto is_last = reader.read<uint8_t>(1).value();
    m_bits_read += 1;
    auto block_type = reader.read<uint8_t>(7).value();
    m_bits_read += 7;
    auto block_size = reader.read<uint32_t>(24).value();
    m_bits_read += 24;

    // std::cout << "Current metadata block - Last: " << static_cast<int>(is_last)
    //          << ", Type: " << static_cast<int>(block_type) << ", Size: " << block_size << "\n";

    switch (static_cast<int>(block_type)) {
    case 0:
      // std::cout << "Processin' the streaminfo block.\n";
      if (!decodeStreaminfo(reader, block_size, is_last)) { return false; }
      break;
    case 1:
      // std::cout << "Processin' the padding block.\n";
      if (!decodePadding(reader, block_size)) { return false; }
      break;
    case 2:
      // std::cout << "Processin' the application block.\n";
      if (!decodeApplication(reader, block_size)) { return false; }
      break;
    case 3:
      // std::cout << "Processin' the seektable block.\n";
      if (!decodeSeektable(reader, block_size)) { return false; }
      break;
    case 4:
      // std::cout << "Processin' the vorbis comment block.\n";
      if (!decodeVorbiscomment(reader, block_size)) { return false; }
      break;
    case 5:
      // std::cout << "Processin' the cue sheet block.\n";
      if (!decodeCuesheet(reader, block_size)) { return false; }
      break;
    case 6:
      // std::cout << "Processin' the picture block.\n";
      if (!decodePicture(reader, block_size)) { return false; }
      break;
    case 127:
      throw std::runtime_error("This metadata block type is forbidden.\n");
    default:
      std::cerr << "Unsupported/Reserved metadata block.\n";
      auto nbits = block_size * 8;
      reader.skip(nbits);
      m_bits_read = nbits;
      break;
    }

    if (is_last == 1) {
      // std::cout << "Last metadata block processed.\n";
      break;
    }
  }

  // decode frames
  return decodeFrames(reader);
}

bool FlacFile::decodeStreaminfo(etl::bit_stream_reader &reader, uint32_t block_size, uint8_t is_last)
{
  if (block_size != 34) { std::cerr << "Invalid STREAMINFO block size: " << block_size << " (expected 34)\n"; }

  // u(16) -> minimum block size
  auto min_block_size = reader.read<uint16_t>(16).value();
  m_bits_read += 16;
  // u(16) -> maximum block size
  auto max_block_size = reader.read<uint16_t>(16).value();
  m_bits_read += 16;
  // u(24) -> minimum frame size
  [[maybe_unused]] auto min_frame_size = reader.read<uint32_t>(24).value();
  m_bits_read += 24;
  // u(24) -> maximum frame size
  [[maybe_unused]] auto max_frame_size = reader.read<uint32_t>(24).value();
  m_bits_read += 24;
  // u(20) -> sample rate in Hz
  auto sample_rate = reader.read<uint32_t>(20).value();
  m_bits_read += 20;
  // u(3) -> number of channels - 1
  auto num_channels = static_cast<int>(reader.read<uint8_t>(3).value()) + 1;
  m_bits_read += 3;
  // u(5) -> bits per sample - 1
  auto bits_per_samples = static_cast<int>(reader.read<uint8_t>(5).value()) + 1;
  m_bits_read += 5;
  // u(36) -> total number of interchannel samples
  auto total_samples = reader.read<uint64_t>(36).value();
  m_total_samples = total_samples;
  m_bits_read += 36;

  // u(128) -> MD5 checksum -> skip
  for (size_t i = 0; i < 16; ++i) {
    auto byte = reader.read<uint8_t>(8).value();
    m_md5_checksum.at(i) = byte;
  }
  m_bits_read += 128;

  m_has_md5_signature = false;
  for (size_t i = 0; i < 16; ++i) {
    if (m_md5_checksum.at(i) != 0) {
      m_has_md5_signature = true;
      break;
    }
  }

  /*
  std::cout << "STREAMINFO:\n"
            << " Block size: " << block_size << "\n"
            << " Min block size: " << min_block_size << "\n"
            << " Max block size: " << max_block_size << "\n"
            << " Min frame size: " << min_frame_size << "\n"
            << " Max frame size: " << max_frame_size << "\n"
            << " Sample rate: " << sample_rate << "\n"
            << " Channels: " << num_channels << "\n"
            << " Bits per sample: " << bits_per_samples << "\n"
            << " Total samples: " << total_samples << "\n";

  if (m_has_md5_signature) {
    std::cout << " MD5 signature: " << MD5::toHex(m_md5_checksum) << "\n";
    m_md5 = std::make_unique<MD5>();
  } else {
    std::cout << " MD5 signature: (not set)\n";
  }*/

  if (min_block_size < 16 || max_block_size < 16 || min_block_size > max_block_size) {
    std::cerr << "Invalid minimum/maxmimum block sizes.\n";
    return false;
  }

  if (min_block_size != max_block_size) {
    if (is_last == 0) {
      if (block_size < min_block_size || block_size > max_block_size) {
        std::cerr << "Invalid block size for not the last frame.\n";
        return false;
      }
    } else if (is_last == 1) {
      if (block_size > max_block_size) {
        std::cerr << "Invalid block size for the last frame.\n";
        return false;
      }
    }
  }

  if (sample_rate == 0) {
    std::cerr << "Invalid sample rate: " << sample_rate << ".\n";
    return false;
  }

  if (num_channels < 1 || num_channels > 8) {
    std::cerr << "Invalid number of channels: " << num_channels << ".\n";
    return false;
  }

  if (bits_per_samples < 4 || bits_per_samples > 32) {
    std::cerr << "Invalid bits per samples (bit depth): " << bits_per_samples << ".\n";
    return false;
  }

  m_sample_rate = sample_rate;
  m_num_channels = uint16_t(num_channels);
  m_bit_depth = uint16_t(bits_per_samples);

  return true;
}

bool FlacFile::decodePadding(etl::bit_stream_reader &reader, uint32_t block_size)
{
  const uint num = block_size * 8;

  // u(n) -> n "0" bits
  // auto space = reader.read<uint64_t>(uint_least8_t(num)).value();
  reader.skip(num);
  m_bits_read += num;

  // std::cout << "PADDING:\n"
  //          << " Space: " << num << "\n";

  return true;
}

bool FlacFile::decodeApplication(etl::bit_stream_reader &reader, uint32_t block_size)
{
  // u(32) -> registered application ID.
  [[maybe_unused]] auto app_id = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  const uint num = (block_size * 8) - 32;

  /*std::cout << "APPLICATION:\n"
            << " n: " << num << "\n"
            << " Application ID: " << app_id << "\n";*/

  reader.skip(num);
  m_bits_read += num;

  return true;
}

bool FlacFile::decodeSeektable(etl::bit_stream_reader &reader, uint32_t block_size)
{
  const uint32_t num_of_seek_points = block_size / 18;
  // std::unordered_map<uint64_t, std::pair<uint64_t, uint16_t>> seek_points;

  // std::cout << "SEEKTABLE:\n";
  // std::cout << " Number of seek points: " << num_of_seek_points << "\n";

  // std::cout << " Seek points:\n";
  for (size_t i = 0; i < size_t(num_of_seek_points); ++i) {
    auto sample_number = reader.read<uint64_t>(64).value();
    m_bits_read += 64;

    if (sample_number == 0xFFFFFFFFFFFFFFFF) { continue; }

    [[maybe_unused]] auto offset = reader.read<uint64_t>(64).value();
    m_bits_read += 64;
    [[maybe_unused]] auto num_samples = reader.read<uint16_t>(16).value();
    m_bits_read += 16;

    // std::cout << "\tSeekpoint " << i << ": sample=" << sample_number << ", offset=" << offset
    //          << ", number of samples=" << num_samples << "\n";

    // TODO: store these if needed, later.
    // seek_points.insert({ sample_number, std::make_pair(offset, num_samples) });
  }

  return true;
}

bool FlacFile::decodeVorbiscomment(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // 4 bytes of vendor string length (little endian)
  auto temp_str_len = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  const uint32_t vendor_str_len = ((temp_str_len >> 24U) & 0xFFU) | ((temp_str_len >> 16U) & 0xFFU)
                                  | ((temp_str_len >> 8U) & 0xFFU) | (temp_str_len & 0xFFU);

  std::string vendor_str;
  vendor_str.reserve(vendor_str_len);
  for (uint32_t i = 0; i < vendor_str_len; ++i) {
    auto chr = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    vendor_str += static_cast<char>(chr);
  }

  // std::cout << "VORBIS COMMENT:\n";
  // std::cout << "Vendor: " << vendor_str << "\n";

  // 4 bytes -> number of fields (little endian)
  auto temp_num_fields = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  const uint32_t num_of_fields = ((temp_num_fields >> 24U) & 0xFFU) | ((temp_num_fields >> 16U) & 0xFFU)
                                 | ((temp_num_fields >> 8U) & 0xFFU) | (temp_num_fields & 0xFFU);

  // std::cout << "number of fields: " << num_of_fields << "\n";

  auto to_lower = [](std::string str) -> std::string {
    std::ranges::transform(str, str.begin(), [](unsigned char chr) { return std::tolower(chr); });
    return str;
  };

  for (uint32_t i = 0; i < num_of_fields; ++i) {
    // 4 bytes -> comment/field length (little endian)
    auto temp_field_len = reader.read<uint32_t>(32).value();
    m_bits_read += 32;
    const uint32_t field_len = ((temp_field_len >> 24U) & 0xFFU) | ((temp_field_len >> 16U) & 0xFFU)
                               | ((temp_field_len >> 8U) & 0xFFU) | (temp_field_len & 0xFFU);
    // std::cout << "field length: " << field_len << "\n";

    std::string field;
    field.reserve(field_len);
    for (uint32_t j = 0; j < field_len; ++j) {
      auto chr = reader.read<uint8_t>(8).value();
      m_bits_read += 8;
      field += static_cast<char>(chr);
    }

    const size_t equals_pos = field.find('=');
    if (equals_pos != std::string::npos) {
      const std::string tag = field.substr(0, equals_pos);
      const std::string value = field.substr(equals_pos + 1);
      // std::cout << "\t" << tag << " = " << value << "\n";

      if (to_lower(tag) == "title") { m_metadata.title = value; }
      if (to_lower(tag) == "artist" || to_lower(tag) == "albumartist") { m_metadata.artist = value; }
      if (to_lower(tag) == "album") { m_metadata.album = value; }
      if (to_lower(tag) == "genre") { m_metadata.genre = value; }
      if (to_lower(tag) == "date") { m_metadata.date = value; }

      if (tag == "WAVEFORMATEXTENSIBLE_CHANNEL_MASK") { m_channel_mask = static_cast<uint32_t>(std::stoi(value)); }
    } else {
      // std::cout << "\t" << field << "\n";
    }
  }

  return true;
}

bool FlacFile::decodeCuesheet(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // u(128 * 8) -> media catalog number (ASCII)
  std::string media_catalog_number;
  media_catalog_number.reserve(128);
  for (int i = 0; i < 128; ++i) {
    auto chr = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    if (chr != 0) { media_catalog_number += static_cast<char>(chr); }
  }

  // std::cout << "CUESHEET:\n";
  // std::cout << " Media catalog number: " << media_catalog_number << "\n";

  // u(64) -> number of lead-in samples
  [[maybe_unused]] auto num_lead_in = reader.read<uint64_t>(64).value();
  m_bits_read += 64;
  // std::cout << " Number of lead-in samples: " << num_lead_in << "\n";

  // u(1) -> if the cuesheet corresponds to CD-DA
  [[maybe_unused]] auto is_cd = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  // std::cout << " Does cuesheet corresponds to CD-DA: " << (is_cd == 1 ? "yes" : "no") << "\n";

  // u(7 + 258 * 8) -> reserved (Skip)
  reader.skip(2071);
  m_bits_read += 2071;

  // u(8) -> number of tracks in this cuesheet
  auto num_tracks = static_cast<int>(reader.read<uint8_t>(8).value());
  m_bits_read += 8;
  // std::cout << " Number of tracks: " << num_tracks << "\n";

  // std::cout << " Tracks:\n";
  //  cuesheet tracks
  for (int i = 0; i < num_tracks; ++i) {
    // u(64) -> track offset
    [[maybe_unused]] auto track_offset = reader.read<uint64_t>(64).value();
    m_bits_read += 64;
    // std::cout << "\tTrack offset: " << track_offset << "\n";

    // u(8) -> track number
    [[maybe_unused]] auto track_number = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    // std::cout << "\tTrack number: " << static_cast<int>(track_number) << "\n";

    // u(12 * 8) -> track ISRC
    std::string track_isrc;
    for (int j = 0; j < 12; ++j) {
      auto chr = reader.read<uint8_t>(8).value();
      m_bits_read += 8;
      if (chr != 0) { track_isrc += static_cast<char>(chr); }
    }
    // std::cout << "\tTrack ISRC: " << track_isrc << "\n";
  }

  // u(1) -> track type
  [[maybe_unused]] auto track_type = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  // std::cout << "\tTrack type: " << (track_type == 0 ? "audio" : "non-audio") << "\n";

  // u(1) -> pre-emphasis flag
  [[maybe_unused]] auto pre_emphasis_flag = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  // std::cout << "\tPre-emphasis flag: " << (pre_emphasis_flag == 0 ? "no pre-emphasis" : "pre-emphasis") << "\n";

  // u(6 + 13 * 8) -> reserved (Skip)
  reader.skip(110);
  m_bits_read += 110;

  // u(8) -> number of track index points
  auto num_indices = static_cast<int>(reader.read<uint8_t>(8).value());
  m_bits_read += 8;
  // std::cout << "\tNumber of track index points: " << num_indices << "\n";

  // index points
  // std::cout << "\tIndex points:\n";
  for (int j = 0; j < num_indices; ++j) {
    // u(64) -> offset in samples
    [[maybe_unused]] auto index_offset = reader.read<uint64_t>(64).value();
    m_bits_read += 64;

    // u(8) -> track index point number
    [[maybe_unused]] auto index_number = static_cast<int>(reader.read<uint8_t>(8).value());
    m_bits_read += 8;

    // u(3 * 8) -> reserved (skip)
    reader.skip(24);
    m_bits_read += 24;

    // std::cout << "\t\tIndex " << index_number << ": offset " << index_offset << " samples\n";
  }

  return true;
}

bool FlacFile::decodePicture(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // u(32) -> picture type
  auto picture_type = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  [[maybe_unused]] const std::string picture_type_str = determinePictureTypeStr(picture_type);

  // std::cout << "PICTURE:\n"
  //           << " Picture Type: " << picture_type << " (" << picture_type_str << ")\n";

  // u(32) -> length of media type string in bytes.
  auto mime_length = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Mime length: " << mime_length << "\n";

  // u(n * 8) -> media type string
  std::string mime_type;
  mime_type.reserve(mime_length);
  for (uint32_t i = 0; i < mime_length; ++i) {
    auto chr = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    mime_type += static_cast<char>(chr);
  }

  // std::cout << " Mime type: " << mime_type << " (" << mime_type.size() << ")\n";

  // u(32) -> length of description string
  auto desc_length = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Description length: " << desc_length << "\n";

  // u(n * 8) -> description string
  std::string description;
  description.reserve(desc_length);
  for (uint32_t i = 0; i < desc_length; ++i) {
    auto chr = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    description += static_cast<char>(chr);
  }
  // std::cout << " Description: " << description << "\n";

  // u(32) -> picture width
  [[maybe_unused]] auto width = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Picture width: " << width << "\n";

  // u(32) -> picture height
  [[maybe_unused]] auto height = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Picture height: " << height << "\n";

  // u(32) -> color depth
  [[maybe_unused]] auto color_depth = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Color depth: " << color_depth << "\n";

  // u(32) -> number of colors
  [[maybe_unused]] auto num_colors = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Number of colors used: " << num_colors << "\n";

  // u(32) -> length of picture data
  auto data_length = reader.read<uint32_t>(32).value();
  m_bits_read += 32;
  // std::cout << " Length of picture data: " << data_length << "\n";

  // u(n * 8) -> binary picture data
  // NOTE: skip it for now, we could encode the data to a picture we can view.
  auto nbits = data_length * 8;
  reader.skip(nbits);
  m_bits_read += nbits;

  return true;
}

bool FlacFile::decodeFrames(etl::bit_stream_reader &reader)
{
  int frame_count = 0;

  while (m_bits_read < reader.size_bits()) {
    // std::cout << "\n=== Decoding Frame " << frame_count << " ===\n";

    try {
      if (!decodeFrame(reader)) {
        std::cerr << "Failed to decode frame " << frame_count << "\n";
        break;
      }

      frame_count++;
    } catch (const std::exception &e) {
      std::cerr << "Exception while decoding frame: " << frame_count << ": " << e.what() << "\n";
      return false;
    }
  }

  // std::cout << "\nSuccessfully decoded " << frame_count << " frames.\n";
  // std::cout << "Total PCM samples: " << m_pcm_data.size() << "\n";

  /*
  if (m_has_md5_signature) {
    if (!validateMD5Checksum()) {
      std::cerr << "⚠️ WARNING: MD5 checksum validation failed.\n";
      return false;
    }
    std::cout << "MD5 checksum validation: ✅ PASSED\n";
  }*/

  return frame_count > 0;
}

bool FlacFile::seekToNextFrame(etl::bit_stream_reader &reader)
{
  std::cout << "Attempting to find next frame sync code.\n";

  const uint32_t bits_to_align = (8 - (m_bits_read % 8)) % 8;
  if (bits_to_align > 0) {
    reader.skip(bits_to_align);
    m_bits_read += bits_to_align;
  }

  auto max_search = static_cast<size_t>(1024 * 16);
  size_t searched = 0;

  while ((reader.size_bits() - m_bits_read) >= 16 && searched < max_search) {
    auto byte = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if (byte == 0xFF) {
      if ((reader.size_bits() - m_bits_read) >= 8) {
        const uint32_t pos_before = m_bits_read;

        auto next_bits = reader.read<uint8_t>(7).value();
        m_bits_read += 7;
        if (next_bits == 0x7C) {
          reader.restart();
          reader.skip(pos_before - 15);
          m_bits_read = pos_before - 15;
          std::cout << "Found sync code at position " << (pos_before - 15) << "\n";
          return true;
        } else {
          reader.restart();
          reader.skip(pos_before);
          m_bits_read = pos_before;
        }
      }
    }

    searched += 8;
  }

  std::cerr << "Could not find sync code within search limits.\n";
  return false;
}

bool FlacFile::decodeFrame(etl::bit_stream_reader &reader)
{
  // decode frame header
  auto tframe_header = decodeFrameHeader(reader);
  if (!tframe_header.has_value()) { return false; }
  auto frame_header = tframe_header.value();

  // decode subframes for each channel
  auto tchannel_data = decodeSubframes(reader, frame_header);
  if (!tchannel_data.has_value()) { return false; }
  auto samples = tchannel_data.value();

  if (frame_header.channel_bits >= 8 && frame_header.channel_bits <= 10) {
    if (!decorrelateChannels(samples, frame_header.channel_bits)) {
      std::cerr << "Failed to decorrelate channels.\n";
      return false;
    }
  }

  /*
  const auto num_channels = samples.size();
  const auto num_samples = samples[0].size();
  const uint32_t num_bytes = (m_bit_depth + 7U) / 8U;
  std::vector<uint8_t> buffer(num_channels * num_bytes * std::min(num_samples, 2048UL));

  for (uint32_t i = 0, l = 0; i < num_samples; ++i) {// NOLINT
    for (uint32_t j = 0; j < num_channels; ++j) {
      auto val = samples[j][i];
      auto uval = static_cast<uint32_t>(val);
      for (uint32_t k = 0; k < num_bytes; ++k, ++l) { buffer.at(l) = static_cast<uint8_t>(uval >> (k << 3U)); }

      if (l >= buffer.size() || (i == num_samples - 1 && j == num_channels - 1)) {
        m_md5->update(buffer.data(), l);
        l = 0;
      }
    }
  }*/

  storeSamples(samples);

  const uint32_t bits_to_align = (8 - (m_bits_read % 8)) % 8;
  if (bits_to_align > 0) {
    // std::cout << "We must byte align.\n";
    reader.skip(bits_to_align);
    m_bits_read += bits_to_align;
  }

  if (!decodeFrameFooter(reader)) {
    std::cerr << "Failed to decode frame footer.\n";
    return false;
  }

  return true;
}

std::optional<FrameHeader> FlacFile::decodeFrameHeader(etl::bit_stream_reader &reader)
{
  FrameHeader frame_header{};

  // std::cout << " Decoding frame header.\n";
  //  u(15) -> frame sync code (0b111111111111100)
  auto frame_sync_code = reader.read<uint16_t>(15).value();
  m_bits_read += 15;
  if (frame_sync_code != 0x7ffc) {
    std::cerr << "Invalid frame sync code: 0x" << std::hex << frame_sync_code << std::dec << "\n";
    return std::nullopt;
  }
  frame_header.frame_sync_code = frame_sync_code;
  // std::cout << "\tFrame sync code: 0x" << std::hex << frame_sync_code << std::dec << "\n";

  // u(1) -> blocking strategy bit
  auto strategy_bit = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  frame_header.strategy_bit = strategy_bit;
  // std::cout << "\tBlocking strategy: " << (strategy_bit == 0 ? "fixed" : "variable") << "\n";

  // u(4) -> block size bits
  auto block_size_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;
  frame_header.block_size_bits = block_size_bits;
  uint32_t block_size = determineBlockSize(block_size_bits);
  frame_header.block_size = block_size;

  // u(4) -> sample rate bits
  auto sample_rate_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;
  frame_header.sample_rate_bits = sample_rate_bits;
  uint32_t sample_rate = determineSampleRate(sample_rate_bits);

  if (sample_rate == 0) { sample_rate = m_sample_rate; }

  // u(4) -> channel bits
  auto channel_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;
  frame_header.channel_bits = channel_bits;
  const uint16_t num_channels = determineChannels(channel_bits);
  frame_header.num_channels = num_channels;
  // std::cout << "\tNum of channels: " << num_channels << " (" << channel_bits << ")\n";

  // u(3) -> bit depth bits
  auto bit_depth_bits = static_cast<int>(reader.read<uint8_t>(3).value());
  frame_header.bit_depth_bits = bit_depth_bits;
  m_bits_read += 3;
  uint16_t bit_depth = determineBitDepth(bit_depth_bits);

  if (bit_depth == 0) { bit_depth = m_bit_depth; }

  frame_header.bit_depth = bit_depth;
  // std::cout << "\tBit depth: " << bit_depth << " (" << bit_depth_bits << ")\n";

  // u(1) -> reserved bit
  auto reserved_bit = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  if (reserved_bit != 0) {
    std::cerr << "\tReserved bit is not 0.\n";
    return std::nullopt;
  }

  // UTF-8 coded sample/frame number (coded number)
  uint64_t coded_number = 0;
  if (strategy_bit == 0) {
    // fixed
    auto res = readUTF8(reader);
    if (res.has_value()) {
      coded_number = res.value();
    } else {
      throw std::runtime_error("Error decoding UTF-8 bytes");
    }
    // std::cout << "\tFrame number: " << coded_number << "\n";
  } else {
    // variable
    auto res = readUTF8(reader);
    if (res.has_value()) {
      coded_number = res.value();
    } else {
      throw std::runtime_error("Error decoding UTF-8 bytes");
    }
    // std::cout << "\tSample number: " << coded_number << "\n";
  }
  frame_header.coded_number = coded_number;

  if (block_size_bits == 6) {
    block_size = reader.read<uint8_t>(8).value() + 1;
    m_bits_read += 8;
  } else if (block_size_bits == 7) {
    block_size = reader.read<uint16_t>(16).value() + 1;
    m_bits_read += 16;
  }
  frame_header.block_size = block_size;
  // std::cout << "\tBlock size: " << block_size << " (" << block_size_bits << ")\n";

  if (sample_rate_bits == 12) {
    sample_rate = reader.read<uint8_t>(8).value() * 1000;
    m_bits_read += 8;
  } else if (sample_rate_bits == 13) {
    sample_rate = reader.read<uint16_t>(16).value();
    m_bits_read += 16;
  } else if (sample_rate_bits == 14) {
    sample_rate = reader.read<uint16_t>(16).value() * 10;
    m_bits_read += 16;
  }

  frame_header.sample_rate = sample_rate;
  // std::cout << "\tSample rate: " << sample_rate << " (" << sample_rate_bits << ")\n";

  // u(8) -> CRC-8 of the frame header
  auto crc8 = static_cast<int>(reader.read<uint8_t>(8).value());
  m_bits_read += 8;

  frame_header.crc8 = crc8;
  // std::cout << "\tCRC-8: 0x" << std::hex << crc8 << std::dec << "\n";

  return frame_header;
}

std::optional<Subframes> FlacFile::decodeSubframes(etl::bit_stream_reader &reader, FrameHeader &frame_header)
{
  std::vector<std::vector<int32_t>> channel_data(frame_header.num_channels);

  for (uint16_t i = 0; i < frame_header.num_channels; ++i) {
    channel_data[i].resize(frame_header.block_size);

    uint16_t subframe_bit_depth = frame_header.bit_depth;
    if (frame_header.channel_bits == 8 && i == 1) {// NOLINT
      subframe_bit_depth = frame_header.bit_depth + 1;
    } else if (frame_header.channel_bits == 9 && i == 0) {
      subframe_bit_depth = frame_header.bit_depth + 1;
    } else if (frame_header.channel_bits == 10 && i == 1) {
      subframe_bit_depth = frame_header.bit_depth + 1;
    }

    // std::cout << "\n=== Decoding a subframe " << i << " ===\n";
    if (!decodeSubframe(reader, channel_data[i], frame_header.block_size, subframe_bit_depth)) {
      std::cerr << "Failed to decode subframe " << i << "\n";
      return std::nullopt;
    }
  }

  return channel_data;
}

bool FlacFile::decodeSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &samples,
  uint32_t block_size,
  uint16_t subframe_bit_depth)
{
  // decode subframe header
  // std::cout << "Subframe Header:\n";
  // u(1) -> reserved bit (must be 0)
  auto reserved_bit = reader.read<uint8_t>(1).value();
  m_bits_read += 1;

  if (static_cast<int>(reserved_bit) != 0) { throw std::runtime_error("The reserved bit must be 0.\n"); }
  // std::cout << "\tReserved bit: " << static_cast<int>(reserved_bit) << "\n";

  // u(6) -> subframe type bits
  auto subframe_type_bits = static_cast<int>(reader.read<uint8_t>(6).value());
  m_bits_read += 6;
  [[maybe_unused]] auto subframe_type = determineSubframeType(subframe_type_bits);
  // std::cout << "\tSubframe type: " << subframe_type << " (" << subframe_type_bits << ")\n";

  // u(1) -> does subframe uses wasted bits
  auto is_wasted_bits = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  // std::cout << "\tAre there wasted bits: " << (is_wasted_bits == 0 ? "no" : "yes") << "\n";

  // u(n) -> wasted bits per sample
  uint8_t wasted_bits = 0;
  if (is_wasted_bits == 1) {
    wasted_bits = 1;

    while (static_cast<int>(reader.read<uint8_t>(1).value()) == 0) { wasted_bits++; }

    m_bits_read += wasted_bits;

    // std::cout << "\tWasted bits: " << static_cast<int>(wasted_bits) << "\n";
  }

  const uint16_t adjusted_bit_depth = subframe_bit_depth - wasted_bits;

  // std::cout << "\tAdjusted bit depth: " << adjusted_bit_depth << "\n";

  if (subframe_type_bits == 0) {
    // std::cout << "\tDecoding " << subframe_type << ":\n";

    return decodeConstantSubframe(reader, samples, adjusted_bit_depth, wasted_bits);
  } else if (subframe_type_bits == 1) {
    // std::cout << "\tDecoding " << subframe_type << ":\n";

    return decodeVerbatimSubframe(reader, samples, block_size, adjusted_bit_depth, wasted_bits);
  } else if (subframe_type_bits >= 8 && subframe_type_bits <= 12) {
    const uint8_t order = uint(subframe_type_bits) & 0x07U;
    // std::cout << "\tDecoding " << subframe_type << " (" << static_cast<int>(order) << "):\n";

    return decodeFixedSubframe(reader, samples, block_size, adjusted_bit_depth, order, wasted_bits);
  } else if (subframe_type_bits >= 32) {
    const uint8_t order = (uint(subframe_type_bits) & 0x1FU) + 1;
    // std::cout << "\tDecoding " << subframe_type << " (" << static_cast<int>(order) << "):\n";

    return decodeLPCSubframe(reader, samples, block_size, adjusted_bit_depth, order, wasted_bits);
  } else {
    std::cerr << "Reserved subframe type: " << subframe_type_bits << "\n";

    return false;
  }
}

bool FlacFile::decodeConstantSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &samples,
  uint16_t bit_depth,
  uint8_t wasted_bits)
{
  int32_t value = readSignedValue(reader, bit_depth);

  if (wasted_bits > 0) { value <<= wasted_bits; }// NOLINT

  for (auto &sample : samples) { sample = value; }

  // std::cout << "\t\tConstant value: " << value << "\n";
  return true;
}

bool FlacFile::decodeVerbatimSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &samples,
  uint32_t block_size,
  uint16_t bit_depth,
  uint8_t wasted_bits)
{
  for (uint32_t i = 0; i < block_size; ++i) {
    int32_t value = readSignedValue(reader, bit_depth);

    if (wasted_bits > 0) { value <<= wasted_bits; }// NOLINT

    samples[i] = value;
  }

  // std::cout << "\t\tRead " << block_size << " verbatim samples\n";
  return true;
}

bool FlacFile::decodeFixedSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &samples,
  uint32_t block_size,
  uint16_t bit_depth,
  uint8_t order,
  uint8_t wasted_bits)
{
  // std::cout << "\t\tDecoding FIXED Predictor Subframe:\n";
  // std::cout << "\t\tReading unencoded warm-up samples.\n";
  //  s(n) -> unencoded warm-up samples
  for (uint8_t i = 0; i < order; ++i) {
    int32_t value = readSignedValue(reader, bit_depth);

    if (wasted_bits > 0) { value <<= wasted_bits; }// NOLINT

    // std::cout << "\t\t\tsample = " << value << "\n";
    samples[i] = value;
  }

  // std::cout << "\t\tDecoding coded residual.\n";
  std::vector<int32_t> residual(block_size - order);
  if (!decodeResidual(reader, residual, block_size, order)) { return false; }

  // std::cout << "\t\tLooping through block_size starting from order.\n";
  for (uint32_t i = order; i < block_size; ++i) {
    int32_t prediction = 0;
    const uint32_t idx = i - 1;

    switch (order) {
    case 0:
      prediction = 0;
      break;
    case 1:
      prediction = samples[idx];
      break;
    case 2:
      prediction = (2 * samples[idx]) - samples[idx - 1];
      break;
    case 3:
      prediction = (3 * samples[idx]) - (3 * samples[idx - 1]) + samples[idx - 2];
      break;
    case 4:
      prediction = (4 * samples[idx]) - (6 * samples[idx - 1]) + (4 * samples[idx - 2]) - samples[idx - 3];
      break;
    default:
      std::cerr << "Unsupported order: " << static_cast<int>(order) << "\n";
      return false;
    }

    // std::cout << "\n\t\tresidual = " << residual[i - order] << "\n";

    samples[i] = residual[i - order] + prediction;

    // std::cout << "\t\tsample be4 shift = " << samples[i] << "\n";

    if (wasted_bits > 0) { samples[i] <<= wasted_bits; }// NOLINT

    // std::cout << "\t\tsample after shift = " << samples[i] << "\n";
  }

  // std::cout << "\t\tDecoded FIXED order " << static_cast<int>(order) << "\n";

  return true;
}

bool FlacFile::decodeLPCSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &samples,
  uint32_t block_size,
  uint16_t bit_depth,
  uint8_t order,
  uint8_t wasted_bits)
{
  // std::cout << "\t\tDecoding Linear Predictor Subframe:\n"
  //           << "\t\tReading unencoded warm-up samples.\n";
  //  u(n) -> unencoded warm-up samples
  for (uint8_t i = 0; i < order; ++i) {
    int32_t value = readSignedValue(reader, bit_depth);

    if (wasted_bits > 0) { value <<= wasted_bits; }// NOLINT

    // std::cout << "\t\t\tsample = " << value << "\n";
    samples[i] = value;
  }

  // u(4) -> predictor coefficient precision in bits
  auto precision = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;

  if (precision == 15) {
    std::cerr << "LPC precision of value 15 is invalid\n";
    return false;
  }
  precision++;

  // std::cout << "\t\tLPC precision value: " << precision << "\n";

  // s(5) -> prediction right shift needed in bits
  auto shift = static_cast<int8_t>(readSignedValue(reader, 5));

  if (shift < 0) {
    std::cerr << "LCP shift must not be negative.\n";
    return false;
  }

  // std::cout << "\t\tLPC shift: " << static_cast<int>(shift) << "\n";

  // std::cout << "\t\tReading coefficients.\n";
  //  s(n) -> predictor coefficients
  std::vector<int32_t> coefficients(order);
  for (uint8_t i = 0; i < order; ++i) {
    coefficients[i] = readSignedValue(reader, uint16_t(precision));
    // std::cout << "\t\tcoffecient=" << coefficients[i] << "\n";
  }

  // std::cout << "\t\tProcessing coded residuals.\n";
  std::vector<int32_t> residual(block_size - order);
  if (!decodeResidual(reader, residual, block_size, order)) { return false; }

  // std::cout << "\t\tLooping to block_size from order.\n";
  for (uint32_t i = order; i < block_size; ++i) {
    int64_t prediction = 0;

    for (uint8_t j = 0; j < order; ++j) { prediction += static_cast<int64_t>(coefficients[j]) * samples[i - j - 1]; }

    prediction >>= uint8_t(shift);// NOLINT

    if (prediction > INT32_MAX || prediction < INT32_MIN) {
      std::cerr << "Warning: LPC prediction overflow after shift: " << prediction << "\n";
    }
    // std::cout << "\n\t\tresidual = " << residual[i - order] << "\n";

    samples[i] = residual[i - order] + static_cast<int32_t>(prediction);

    // std::cout << "\t\tsample be4 shift = " << samples[i] << "\n";
    if (wasted_bits > 0) { samples[i] <<= wasted_bits; }// NOLINT

    // std::cout << "\t\tsample after shift = " << samples[i] << "\n";
  }

  // std::cout << "\t\tDecoded LPC order " << static_cast<int>(order) << ", precision " << precision << ", shift "
  //           << static_cast<int>(shift) << "\n";

  return true;
}

bool FlacFile::decodeResidual(etl::bit_stream_reader &reader,
  std::vector<int32_t> &residual,
  uint32_t block_size,
  uint8_t predictor_order)
{
  // u(2) -> coding method bits
  auto coding_method = static_cast<int>(reader.read<uint8_t>(2).value());
  m_bits_read += 2;

  if (coding_method > 1) {
    std::cerr << "Reserved residual coding method.\n";
    return false;
  }
  // std::cout << "\t\t\tCoding method: " << coding_method << "\n";

  // u(4) -> partition order
  auto partition_order = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;

  const uint32_t num_partitions = 1U << uint(partition_order);
  // std::cout << "\t\t\tPartition order: " << partition_order << " (" << num_partitions << " partitions)\n";

  if (block_size % num_partitions != 0) {
    std::cerr << "Invalid data stream: block_size " << block_size << " is not divisible by " << num_partitions
              << " partitions\n";
    return false;
  }

  const uint32_t samples_per_partition = block_size >> uint(partition_order);

  if (samples_per_partition <= predictor_order) {
    std::cerr << "Invalid data stream: block_size " << block_size << " >> partition_order " << partition_order
              << " is less than predictor_order " << static_cast<int>(predictor_order) << "\n";
    return false;
  }

  const uint32_t total_res_samples = block_size - predictor_order;

  if (residual.size() != total_res_samples) {
    std::cerr << "Residual buffer size mismatch: " << residual.size() << " vs expected " << total_res_samples << "\n";
    return false;
  }

  uint32_t sample_idx = 0;

  // std::cout << "\t\t\tProcessing all partitions.\n";
  for (uint32_t part_idx = 0; part_idx < num_partitions; ++part_idx) {
    uint32_t partition_samples{};

    if (part_idx == 0) {
      partition_samples = samples_per_partition - predictor_order;
    } else {
      partition_samples = samples_per_partition;
    }

    if (partition_samples == 0) {
      std::cerr << "⚠️ Warning: partition " << part_idx << " has 0 samples.\n";
      continue;
    }

    // std::cout << "\n\t\t\tNumber of residual samples: " << partition_samples << "\n";

    const uint8_t rice_param_bits = (coding_method == 0) ? 4 : 5;
    auto rice_param = reader.read<uint32_t>(uint_least8_t(rice_param_bits)).value();
    m_bits_read += rice_param_bits;

    // std::cout << "\t\t\tRice parameter: " << rice_param << " (" << static_cast<int>(rice_param_bits) << ").\n";

    const uint32_t escape_code = (coding_method == 0) ? 15 : 31;

    // std::cout << "\t\t\tEscape code: " << escape_code << "\n";

    if (rice_param == escape_code) {
      // u(5) -> unencoded binary partition
      auto bps = reader.read<uint8_t>(5).value();
      m_bits_read += 5;
      // std::cout << "\t\t\tUnencoded partition: " << static_cast<int>(bps) << " bps\n";

      // std::cout << "\t\t\tDecoding residual coding samples with signed value.\n";
      for (uint32_t i = 0; i < partition_samples; ++i) {
        if (sample_idx >= residual.size()) {
          std::cerr << "Residual buffer overflow at sample " << sample_idx << "\n";
          return false;
        }
        residual[sample_idx++] = readSignedValue(reader, bps);
      }
    } else {
      // std::cout << "\t\t\tDecoding residual coding with rice signed value.\n";
      // std::cout << "\t\t\tRice parameter: " << rice_param << "\n";

      for (uint32_t i = 0; i < partition_samples; ++i) {
        if (sample_idx >= residual.size()) {
          std::cerr << "Residual buffer overflow at sample " << sample_idx << "\n";
          return false;
        }
        residual[sample_idx++] = readRiceSignedValue(reader, rice_param);
      }
    }
  }

  if (sample_idx != total_res_samples) {
    std::cerr << "Residual sample count mismatch: decoded " << sample_idx << ", expected " << total_res_samples << "\n";
    return false;
  }

  // std::cout << "\t\t\tSuccessfully decoded " << total_res_samples << " residual samples.\n";
  return true;
}

bool FlacFile::decodeFrameFooter(etl::bit_stream_reader &reader)
{
  [[maybe_unused]] auto crc16 = reader.read<uint16_t>(16).value();
  m_bits_read += 16;

  // std::cout << "Frame footer CRC-16: 0x" << std::hex << crc16 << std::dec << "\n";

  return true;
}

bool FlacFile::encodeFlacFile() { return false; }

std::optional<uint64_t> FlacFile::readUTF8(etl::bit_stream_reader &reader)
{
  auto first_byte = reader.read<uint8_t>(8).value();
  m_bits_read += 8;
  const int num_bytes = utf8SequenceLength(first_byte);

  if (num_bytes == 0) { return std::nullopt; }

  uint64_t result = 0;

  switch (num_bytes) {
  case 1:
    result = first_byte & 0x7FU;
    break;
  case 2: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(((first_byte & 0x1FU) << 6U) | (byte2 & 0x3FU));

    break;
  }
  case 3: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80 || (byte3 & 0xC0U) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(((first_byte & 0x0FU) << 12U) | ((byte2 & 0x3FU) << 6U) | (byte3 & 0x3FU));

    break;
  }
  case 4: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80 || (byte3 & 0xC0U) != 0x80 || (byte4 & 0xC0U) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(
      ((first_byte & 0x07U) << 18U) | ((byte2 & 0x3FU) << 12U) | ((byte3 & 0x3FU) << 6U) | (byte4 & 0x3FU));

    break;
  }
  case 5: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80 || (byte3 & 0xC0U) != 0x80 || (byte4 & 0xC0U) != 0x80 || (byte5 & 0xC0U) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((first_byte & 0x03U) << 24U) | ((byte2 & 0x3FU) << 18U) | ((byte3 & 0x3FU) << 12U)
                                   | ((byte4 & 0x3FU) << 6U) | (byte5 & 0x3FU));

    break;
  }
  case 6: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte6 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80 || (byte3 & 0xC0U) != 0x80 || (byte4 & 0xC0U) != 0x80 || (byte5 & 0xC0U) != 0x80
        || (byte6 & 0xC0U) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((first_byte & 0x01U) << 30U) | ((byte2 & 0x3FU) << 24U) | ((byte3 & 0x3FU) << 18U)
                                   | ((byte4 & 0x3FU) << 12U) | ((byte5 & 0x3FU) << 6U) | (byte6 & 0x3FU));

    break;
  }
  case 7: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte6 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;
    auto byte7 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;

    if ((byte2 & 0xC0U) != 0x80 || (byte3 & 0xC0U) != 0x80 || (byte4 & 0xC0U) != 0x80 || (byte5 & 0xC0U) != 0x80
        || (byte6 & 0xC0U) != 0x80 || (byte7 & 0xC0U) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((byte2 & 0x3FU) << 30U) | ((byte3 & 0x3FU) << 24U) | ((byte4 & 0x3FU) << 18U)
                                   | ((byte5 & 0x3FU) << 12U) | ((byte6 & 0x3FU) << 6U) | (byte7 & 0x3FU));

    break;
  }
  default:
    return std::nullopt;
  }

  return result;
}

int32_t FlacFile::readSignedValue(etl::bit_stream_reader &reader, uint16_t bits)
{
  // std::cout << "\n\t\t\tRead signed values.\n";
  if (bits == 0) { return 0; }

  if (bits < 1 || bits > 32) { throw std::invalid_argument("bits must be between 1 and 32"); }

  const auto value = reader.read<uint32_t>(static_cast<uint_least8_t>(bits)).value();
  m_bits_read += bits;
  // std::cout << "\t\t\tvalue=" << value << "\n";

  if (bits == 32) { return static_cast<int32_t>(value); }

  const uint32_t sign_bit_mask = 1U << (bits - 1U);
  const uint32_t two_power = 1U << bits;

  if (bool(value & sign_bit_mask)) {
    const int32_t result = static_cast<int32_t>(value) - static_cast<int32_t>(two_power);
    // std::cout << "\t\t\tresult1=" << result << "\n";
    return result;
  } else {
    // std::cout << "\t\t\tresult2=" << static_cast<int32_t>(value) << "\n";
    return static_cast<int32_t>(value);
  }
}

int32_t FlacFile::readRiceSignedValue(etl::bit_stream_reader &reader, uint32_t param)
{
  // std::cout << "\n\t\t\tRead rice signed values.\n";
  uint32_t quotient = 0;
  while (static_cast<int>(reader.read<uint8_t>(1).value()) == 0) { quotient++; }
  m_bits_read += (quotient + 1);
  // std::cout << "\t\t\tqoutient = " << quotient << "\n";

  uint32_t remainder = 0;
  if (param > 0) {
    remainder = reader.read<uint32_t>(uint_least8_t(param)).value();
    m_bits_read += param;
  }

  // std::cout << "\t\t\tremainder = " << remainder << "\n";

  const uint32_t value = (quotient << param) | remainder;

  // std::cout << "\t\t\tzigzag unencoded value = " << value << "\n";

  if (bool(value & 1U)) {
    auto value2 = -static_cast<int32_t>((value + 1) >> 1U);
    // std::cout << "\t\t\tresidual sample value1 = " << value2 << "\n";
    return value2;
  } else {
    auto value2 = static_cast<int32_t>(value >> 1U);
    // std::cout << "\t\t\tresidual sample value2 = " << value2 << "\n";
    return value2;
  }
}

bool FlacFile::decorrelateChannels(std::vector<std::vector<int32_t>> &channels, int channel_assignment)
{
  if (channels.size() != 2) {
    std::cerr << "Decorrelation requires exactly 2 channels.\n";
    return false;
  }

  const size_t num_samples = channels[0].size();

  switch (channel_assignment) {
  case 8:
    for (size_t i = 0; i < num_samples; ++i) { channels[1][i] = channels[0][i] - channels[1][i]; }
    break;
  case 9:
    for (size_t i = 0; i < num_samples; ++i) { channels[0][i] = channels[1][i] + channels[0][i]; }
    break;
  case 10:
    for (size_t i = 0; i < num_samples; ++i) {
      const int32_t mid = channels[0][i];
      const int32_t side = channels[1][i];

      int32_t left = mid + (side >> 1);// NOLINT
      int32_t right = mid - (side >> 1);// NOLINT

      if (bool(side & 1)) {// NOLINT
        left += (side > 0) ? 0 : 1;
        right += (side > 0) ? -1 : 0;
      }

      channels[0][i] = left;
      channels[1][i] = right;
    }
    break;
  default:
    break;
  }

  // std::cout << "\t\tDecorrelated channels (mode " << channel_assignment << ")\n";
  return true;
}

bool FlacFile::isSyncCode(etl::bit_stream_reader &reader) const
{
  const size_t current_pos = m_bits_read;

  if ((reader.size_bits() - m_bits_read) < 15) { return false; }

  auto sync = reader.read<uint16_t>(15);
  if (!sync.has_value()) { return false; }

  reader.restart();
  reader.skip(current_pos);

  return sync.value() == 0x7FFC;
}

void FlacFile::storeSamples(const std::vector<std::vector<int32_t>> &channel_data)
{
  const size_t num_samples = channel_data[0].size();
  const size_t num_channels = channel_data.size();

  const double norm_factor = 1.0 / (1U << (m_bit_depth - 1U));

  for (size_t i = 0; i < num_samples; ++i) {
    for (size_t chn = 0; chn < num_channels; ++chn) {
      const double normalized_sample = channel_data[chn][i] * norm_factor;
      m_pcm_data.push_back(normalized_sample);
    }
  }

  // std::cout << "Stored " << num_samples << " samples x " << num_channels << " channels.\n";
}

bool FlacFile::validateMD5Checksum()
{
  auto computed_md5 = m_md5->finalize();

  const bool match = std::equal(computed_md5.begin(), computed_md5.end(), m_md5_checksum.begin());

  if (!match) {
    std::cout << "Expected MD5: " << MD5::toHex(m_md5_checksum) << "\n";
    std::cout << "Computed MD5: " << MD5::toHex(computed_md5) << "\n";
  }

  return match;
}

/*
 * Utility functions
 */

std::string determinePictureTypeStr(uint32_t picture_type)
{
  std::string picture_type_str = "Unknown";

  switch (picture_type) {
  case 0:
    picture_type_str = "Other";
    break;
  case 1:
    picture_type_str = "PNG file icon of 32x32 pixels";
    break;
  case 2:
    picture_type_str = "General file icon";
    break;
  case 3:
    picture_type_str = "Front cover";
    break;
  case 4:
    picture_type_str = "Back cover";
    break;
  case 5:
    picture_type_str = "Liner notes page";
    break;
  case 6:
    picture_type_str = "Media label";
    break;
  case 7:
    picture_type_str = "Lead artist";
    break;
  case 8:
    picture_type_str = "Artist or performer";
    break;
  case 9:
    picture_type_str = "Conductor";
    break;
  case 10:
    picture_type_str = "Band or orchestra";
    break;
  case 11:
    picture_type_str = "Composer";
    break;
  case 12:
    picture_type_str = "Lyricist or text writer";
    break;
  case 13:
    picture_type_str = "Recording location";
    break;
  case 14:
    picture_type_str = "During recording";
    break;
  case 15:
    picture_type_str = "During performance";
    break;
  case 16:
    picture_type_str = "Movie or video screen capture";
    break;
  case 17:
    picture_type_str = "A bright colored fish";
    break;
  case 18:
    picture_type_str = "Illustration";
    break;
  case 19:
    picture_type_str = "Band or artist logotype";
    break;
  case 20:
    picture_type_str = "Publisher or studio logotype";
    break;
  default:
    std::cerr << "Unsupported picture type.\n";
    return "";
  }

  return picture_type_str;
}

uint32_t determineBlockSize(int block_size_bits)
{
  uint32_t block_size{};

  if (block_size_bits == 0) {
    throw std::runtime_error("Invalid block size bits (reserved)");
  } else if (block_size_bits == 1) {
    block_size = 192;
  } else if (block_size_bits >= 2 && block_size_bits <= 5) {
    block_size = 144 * (1U << uint(block_size_bits));
  } else if (block_size_bits >= 8 && block_size_bits <= 15) {
    block_size = 1U << uint(block_size_bits);
  }

  return block_size;
}

uint32_t determineSampleRate(int sample_rate_bits)
{
  uint32_t sample_rate{};

  switch (sample_rate_bits) {
  case 0:
    // std::cout << "Sample rate is stored in the streaminfo metadata block.\n";
    break;
  case 1:
    sample_rate = 88'200;
    break;
  case 2:
    sample_rate = 176'400;
    break;
  case 3:
    sample_rate = 192'000;
    break;
  case 4:
    sample_rate = 8'000;
    break;
  case 5:
    sample_rate = 16'000;
    break;
  case 6:
    sample_rate = 22'050;
    break;
  case 7:
    sample_rate = 24'000;
    break;
  case 8:
    sample_rate = 32'000;
    break;
  case 9:
    sample_rate = 44'100;
    break;
  case 10:
    sample_rate = 48'000;
    break;
  case 11:
    sample_rate = 96'000;
    break;
  case 15:
    throw std::runtime_error("Sample rate bits are forbidden.\n");
  default:
    throw std::runtime_error("Unsupported sample rate bits.\n");
  }

  return sample_rate;
}

uint16_t determineChannels(int channel_bits)
{
  uint16_t channels{};

  if (channel_bits <= 7) {
    channels = static_cast<uint16_t>(channel_bits + 1);
  } else if (channel_bits == 8 || channel_bits == 9 || channel_bits == 10) {
    channels = 2;
  } else {
    throw std::runtime_error("Reserved channel bits.\n");
  }

  return channels;
}

uint16_t determineBitDepth(int bit_depth_bits)
{
  uint16_t bit_depth{};

  switch (bit_depth_bits) {
  case 0:
    // std::cout << "Bit depth is stored in the streaminfo metadata block.\n";
    break;
  case 1:
    bit_depth = 8;
    break;
  case 2:
    bit_depth = 12;
    break;
  case 3:
    std::cerr << "Reserved space for bit depth bits.\n";
    break;
  case 4:
    bit_depth = 16;
    break;
  case 5:
    bit_depth = 20;
    break;
  case 6:
    bit_depth = 24;
    break;
  case 7:
    bit_depth = 32;
    break;
  default:
    throw std::runtime_error("Unsupported bit depth bits.\n");
  }

  return bit_depth;
}

std::string determineSubframeType(int subframe_type_bits)
{
  std::string subframe_type{};

  if (subframe_type_bits == 0) {
    subframe_type = "Constant subframe";
  } else if (subframe_type_bits == 1) {
    subframe_type = "Verbatim subframe";
  } else if ((subframe_type_bits >= 2 && subframe_type_bits <= 7)
             || (subframe_type_bits >= 13 && subframe_type_bits <= 31)) {
    subframe_type = "Reserved";
  } else if (subframe_type_bits >= 8 && subframe_type_bits <= 12) {
    subframe_type = "Subframe with a fixed predictor of order " + std::to_string(subframe_type_bits - 8);
  } else if (subframe_type_bits >= 32 && subframe_type_bits <= 63) {
    subframe_type = "Subframe with a linear predictor of order " + std::to_string(subframe_type_bits - 31);
  }

  return subframe_type;
}

int utf8SequenceLength(uint8_t first_byte)
{
  if ((first_byte & 0x80U) == 0x00) { return 1; }
  if ((first_byte & 0xE0U) == 0xC0) { return 2; }
  if ((first_byte & 0xF0U) == 0xE0) { return 3; }
  if ((first_byte & 0xF8U) == 0xF0) { return 4; }
  if ((first_byte & 0xFCU) == 0xF8) { return 5; }
  if ((first_byte & 0xFEU) == 0xFC) { return 6; }
  if (first_byte == 0xFE) { return 7; }
  return 0;
}

}// namespace afs
