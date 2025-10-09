#include <afsproject/flac_file.h>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <etl/bit_stream.h>
#include <etl/endianness.h>
#include <etl/span.h>
#include <exception>
#include <fstream>
#include <ios>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
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

bool FlacFile::decodeFlacFile()
{
  const etl::span<const uint8_t> data_span(m_file_data.data(), m_file_data.size());
  etl::bit_stream_reader reader(data_span, etl::endian::big);

  auto flac_marker = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT

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
    m_bits_read += 1;
    auto block_type = reader.read<uint8_t>(7).value();// NOLINT
    m_bits_read += 7;// NOLINT
    auto block_size = reader.read<uint32_t>(24).value();// NOLINT
    m_bits_read += 24;// NOLINT

    std::cout << "Current metadata block - Last: " << static_cast<int>(is_last)
              << ", Type: " << static_cast<int>(block_type) << ", Size: " << block_size << "\n";

    switch (static_cast<int>(block_type)) {
    case 0:
      std::cout << "Processin' the streaminfo block.\n";
      if (!decodeStreaminfo(reader, block_size, is_last)) { return false; }
      break;
    case 1:
      std::cout << "Processin' the padding block.\n";
      decodePadding(reader, block_size);
      break;
    case 2:
      std::cout << "Processin' the application block.\n";
      if (!decodeApplication(reader, block_size)) { return false; }
      break;
    case 3:
      std::cout << "Processin' the seektable block.\n";
      if (!decodeSeektable(reader, block_size)) { return false; }
      break;
    case 4:
      std::cout << "Processin' the vorbis comment block.\n";
      if (!decodeVorbiscomment(reader, block_size)) { return false; }
      break;
    case 5:// NOLINT
      std::cout << "Processin' the cue sheet block.\n";
      if (!decodeCuesheet(reader, block_size)) { return false; }
      break;
    case 6:// NOLINT
      std::cout << "Processin' the picture block.\n";
      if (!decodePicture(reader, block_size)) { return false; }
      break;
    case 127:// NOLINT
      throw std::runtime_error("This metadata block type is forbidden.\n");
    default:
      std::cerr << "Unsupported/Reserved metadata block.\n";
      reader.skip(block_size * 8);// NOLINT
      m_bits_read = (block_size * 8);// NOLINT
      break;
    }

    if (is_last == 1) {
      std::cout << "Last metadata block processed.\n";
      break;
    }
  }

  // decode frames
  return decodeFrames(reader);
}

bool FlacFile::decodeStreaminfo(etl::bit_stream_reader &reader, uint32_t block_size, uint8_t is_last)// NOLINT
{
  if (block_size != 34) { std::cerr << "Invalid STREAMINFO block size: " << block_size << " (expected 34)\n"; }// NOLINT

  // u(16) -> minimum block size
  auto min_block_size = reader.read<uint16_t>(16).value();// NOLINT
  m_bits_read += 16;// NOLINT
  // u(16) -> maximum block size
  auto max_block_size = reader.read<uint16_t>(16).value();// NOLINT
  m_bits_read += 16;// NOLINT
  // u(24) -> minimum frame size
  auto min_frame_size = reader.read<uint32_t>(24).value();// NOLINT
  m_bits_read += 24;// NOLINT
  // u(24) -> maximum frame size
  auto max_frame_size = reader.read<uint32_t>(24).value();// NOLINT
  m_bits_read += 24;// NOLINT
  // u(20) -> sample rate in Hz
  auto sample_rate = reader.read<uint32_t>(20).value();// NOLINT
  m_bits_read += 20;// NOLINT
  // u(3) -> number of channels - 1
  auto num_channels = static_cast<int>(reader.read<uint8_t>(3).value());
  m_bits_read += 3;// NOLINT
  // u(5) -> bits per sample - 1
  auto bits_per_samples = static_cast<int>(reader.read<uint8_t>(5).value());// NOLINT
  m_bits_read += 5;// NOLINT
  // u(36) -> total number of interchannel samples
  auto total_samples = reader.read<uint64_t>(36).value();// NOLINT
  m_bits_read += 36;// NOLINT

  // u(128) -> MD5 checksum -> skip
  reader.skip(128);// NOLINT
  m_bits_read += 128;// NOLINT

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

  if (min_block_size < 16 || max_block_size < 16 || min_block_size > max_block_size) {// NOLINT
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

  if (num_channels < 1 || num_channels > 8) {// NOLINT
    std::cerr << "Invalid number of channels: " << num_channels << ".\n";
    return false;
  }

  if (bits_per_samples < 4 || bits_per_samples > 32) {// NOLINT
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
  uint n = block_size * 8;// NOLINT

  if (n % 8 != 0) {// NOLINT
    std::cerr << "n is not a multiple of 8: " << n << ".\n";
    return false;
  }

  // u(n) -> n "0" bits
  // auto space = reader.read<uint64_t>(static_cast<uint_least8_t>(n)).value();

  std::cout << "PADDING:\n"
            << " Space: " << n << "\n";

  reader.skip(n);
  m_bits_read += n;

  return true;
}

bool FlacFile::decodeApplication(etl::bit_stream_reader &reader, uint32_t block_size)
{
  // u(32) -> registered application ID.
  auto app_id = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  uint n = (block_size * 8) - 32;// NOLINT

  std::cout << "APPLICATION:\n"
            << " n: " << n << "\n"
            << " Application ID: " << app_id << "\n";

  reader.skip(n);
  m_bits_read += n;

  return true;
}

bool FlacFile::decodeSeektable(etl::bit_stream_reader &reader, uint32_t block_size)
{
  uint32_t num_of_seek_points = block_size / 18;// NOLINT
  std::unordered_map<uint64_t, std::pair<uint64_t, uint16_t>> seek_points;

  std::cout << "SEEKTABLE:\n";
  std::cout << " Number of seek points: " << num_of_seek_points << "\n";

  std::cout << " Seek points:\n";
  for (size_t i = 0; i < size_t(num_of_seek_points); ++i) {
    auto sample_number = reader.read<uint64_t>(64).value();// NOLINT
    m_bits_read += 64;// NOLINT

    if (sample_number == 0xFFFFFFFFFFFFFFFF) { continue; }// NOLINT

    auto offset = reader.read<uint64_t>(64).value();// NOLINT
    m_bits_read += 64;// NOLINT
    auto num_samples = reader.read<uint16_t>(16).value();// NOLINT
    m_bits_read += 16;// NOLINT

    std::cout << "\tSeekpoint " << i << ": sample=" << sample_number << ", offset=" << offset
              << ", samples=" << num_samples << "\n";

    // TODO: store these if needed, later.
    seek_points.insert({ sample_number, std::make_pair(offset, num_samples) });
  }

  return true;
}

bool FlacFile::decodeVorbiscomment(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // 4 bytes of vendor string length (little endian)
  auto temp_str_len = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  uint32_t vendor_str_len = ((temp_str_len >> 24) & 0xFF) | ((temp_str_len >> 16) & 0xFF)// NOLINT
                            | ((temp_str_len >> 8) & 0xFF)// NOLINT
                            | (temp_str_len & 0xFF);// NOLINT

  std::string vendor_str;
  vendor_str.reserve(vendor_str_len);
  for (uint32_t i = 0; i < vendor_str_len; ++i) {
    auto chr = reader.read<uint8_t>(8).value();// NOLINT
    m_bits_read += 8;// NOLINT
    vendor_str += static_cast<char>(chr);
  }

  std::cout << "VORBIS COMMENT:\n";
  std::cout << "Vendor: " << vendor_str << "\n";

  // 4 bytes -> number of fields (little endian)
  auto temp_num_fields = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  uint32_t num_of_fields = ((temp_num_fields >> 24) & 0xFF) | ((temp_num_fields >> 16) & 0xFF)// NOLINT
                           | ((temp_num_fields >> 8) & 0xFF) | (temp_num_fields & 0xFF);// NOLINT

  std::cout << "number of fields: " << num_of_fields << "\n";

  for (uint32_t i = 0; i < num_of_fields; ++i) {
    // 4 bytes -> comment/field length (little endian)
    auto temp_field_len = reader.read<uint32_t>(32).value();// NOLINT
    m_bits_read += 32;// NOLINT
    uint32_t field_len = ((temp_field_len >> 24) & 0xFF) | ((temp_field_len >> 16) & 0xFF)// NOLINT
                         | ((temp_field_len >> 8) & 0xFF) | (temp_field_len & 0xFF);// NOLINT
    std::cout << "field length: " << field_len << "\n";

    std::string field;
    field.reserve(field_len);
    for (uint32_t j = 0; j < field_len; ++j) {
      auto chr = reader.read<uint8_t>(8).value();// NOLINT
      m_bits_read += 8;// NOLINT
      field += static_cast<char>(chr);
    }

    const size_t equals_pos = field.find('=');
    if (equals_pos != std::string::npos) {
      const std::string tag = field.substr(0, equals_pos);
      const std::string value = field.substr(equals_pos + 1);
      std::cout << "\t" << tag << " = " << value << "\n";

      if (tag == "WAVEFORMATEXTENSIBLE_CHANNEL_MASK") { m_channel_mask = static_cast<uint32_t>(std::stoi(value)); }
    } else {
      std::cout << "\t" << field << "\n";
    }
  }

  return true;
}

bool FlacFile::decodeCuesheet(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // u(128 * 8) -> media catalog number (ASCII)
  std::string media_catalog_number;
  media_catalog_number.reserve(128);// NOLINT
  for (int i = 0; i < 128; ++i) {// NOLINT
    auto chr = reader.read<uint8_t>(8).value();// NOLINT
    m_bits_read += 8;// NOLINT
    if (chr != 0) { media_catalog_number += static_cast<char>(chr); }
  }

  std::cout << "CUESHEET:\n";
  std::cout << " Media catalog number: " << media_catalog_number << "\n";

  // u(64) -> number of lead-in samples
  auto num_lead_in = reader.read<uint64_t>(64).value();// NOLINT
  m_bits_read += 64;// NOLINT
  std::cout << " Number of lead-in samples: " << num_lead_in << "\n";

  // u(1) -> if the cuesheet corresponds to CD-DA
  auto is_cd = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;// NOLINT
  std::cout << " Does cuesheet corresponds to CD-DA: " << (is_cd == 1 ? "yes" : "no") << "\n";

  // u(7 + 258 * 8) -> reserved (Skip)
  reader.skip(2071);// NOLINT
  m_bits_read += 2071;// NOLINT

  // u(8) -> number of tracks in this cuesheet
  auto num_tracks = static_cast<int>(reader.read<uint8_t>(8).value());// NOLINT
  m_bits_read += 8;// NOLINT
  std::cout << " Number of tracks: " << num_tracks << "\n";

  std::cout << " Tracks:\n";
  // cuesheet tracks
  for (int i = 0; i < num_tracks; ++i) {
    // u(64) -> track offset
    auto track_offset = reader.read<uint64_t>(64).value();// NOLINT
    m_bits_read += 64;// NOLINT
    std::cout << "\tTrack offset: " << track_offset << "\n";

    // u(8) -> track number
    auto track_number = reader.read<uint8_t>(8).value();// NOLINT
    m_bits_read += 8;// NOLINT
    std::cout << "\tTrack number: " << static_cast<int>(track_number) << "\n";

    // u(12 * 8) -> track ISRC
    std::string track_isrc;
    for (int j = 0; j < 12; ++j) {// NOLINT
      auto chr = reader.read<uint8_t>(8).value();// NOLINT
      m_bits_read += 8;// NOLINT
      if (chr != 0) { track_isrc += static_cast<char>(chr); }
    }
    std::cout << "\tTrack ISRC: " << track_isrc << "\n";
  }

  // u(1) -> track type
  auto track_type = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;// NOLINT
  std::cout << "\tTrack type: " << (track_type == 0 ? "audio" : "non-audio") << "\n";

  // u(1) -> pre-emphasis flag
  auto pre_emphasis_flag = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  std::cout << "\tPre-emphasis flag: " << (pre_emphasis_flag == 0 ? "no pre-emphasis" : "pre-emphasis") << "\n";

  // u(6 + 13 * 8) -> reserved (Skip)
  reader.skip(110);// NOLINT
  m_bits_read += 110;// NOLINT

  // u(8) -> number of track index points
  auto num_indices = static_cast<int>(reader.read<uint8_t>(8).value());// NOLINT
  m_bits_read += 8;// NOLINT
  std::cout << "\tNumber of track index points: " << num_indices << "\n";

  // index points
  std::cout << "\tIndex points:\n";
  for (int j = 0; j < num_indices; ++j) {
    // u(64) -> offset in samples
    auto index_offset = reader.read<uint64_t>(64).value();// NOLINT
    m_bits_read += 64;// NOLINT

    // u(8) -> track index point number
    auto index_number = static_cast<int>(reader.read<uint8_t>(8).value());// NOLINT
    m_bits_read += 8;// NOLINT

    // u(3 * 8) -> reserved (skip)
    reader.skip(24);// NOLINT
    m_bits_read += 24;// NOLINT

    std::cout << "\t\tIndex " << index_number << ": offset " << index_offset << " samples\n";
  }

  return true;
}

bool FlacFile::decodePicture(etl::bit_stream_reader &reader, [[maybe_unused]] uint32_t block_size)
{
  // u(32) -> picture type
  auto picture_type = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  const std::string picture_type_str = determinePictureTypeStr(picture_type);

  std::cout << "PICTURE:\n"
            << " Picture Type: " << picture_type << " (" << picture_type_str << ")\n";

  // u(32) -> length of media type string in bytes.
  auto mime_length = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Mime length: " << mime_length << "\n";

  // u(n * 8) -> media type string
  std::string mime_type;
  mime_type.reserve(mime_length);
  for (uint32_t i = 0; i < mime_length; ++i) {
    auto chr = reader.read<uint8_t>(8).value();// NOLINT
    m_bits_read += 8;// NOLINT
    mime_type += static_cast<char>(chr);
  }

  std::cout << " Mime type: " << mime_type << "\n";

  // u(32) -> length of description string
  auto desc_length = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Description length: " << desc_length << "\n";

  // u(n * 8) -> description string
  std::string description;
  description.reserve(desc_length);
  for (uint32_t i = 0; i < desc_length; ++i) {
    auto chr = reader.read<uint8_t>(8).value();// NOLINT
    m_bits_read += 8;// NOLINT
    description += static_cast<char>(chr);
  }
  std::cout << " Description: " << description << "\n";

  // u(32) -> picture width
  auto width = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Picture width: " << width << "\n";

  // u(32) -> picture height
  auto height = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Picture height: " << height << "\n";

  // u(32) -> color depth
  auto color_depth = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Color depth: " << color_depth << "\n";

  // u(32) -> number of colors
  auto num_colors = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Number of colors used: " << num_colors << "\n";

  // u(32) -> length of picture data
  auto data_length = reader.read<uint32_t>(32).value();// NOLINT
  m_bits_read += 32;// NOLINT
  std::cout << " Length of picture data: " << data_length << "\n";

  // u(n * 8) -> binary picture data
  // NOTE: skip it for now, we could encode the data to a picture we can view.
  reader.skip(data_length * 8);// NOLINT
  m_bits_read += (data_length * 8);// NOLINT

  return true;
}

bool FlacFile::decodeFrames(etl::bit_stream_reader &reader)
{
  std::cout << "Decoding frames...\n";

  while (m_bits_read < reader.size_bits()) {
    try {
      if (!decodeFrame(reader)) {
        std::cerr << "Failed to decode frame.\n";
        return false;
      }
    } catch (const std::exception &e) {
      std::cerr << "Exception while decoding frame: " << e.what() << "\n";
      return false;
    }
  }

  std::cout << "Successfully decoded all frames.\n";

  return true;
}

bool FlacFile::decodeFrame(etl::bit_stream_reader &reader)
{
  // decode frame header
  auto frame_header = decodeFrameHeader(reader);
  if (!frame_header.has_value()) { return false; }

  // TODO: decode subframes for each channel
  if (!decodeSubframes(reader, frame_header.value())) { return false; }

  // TODO: handle stereo decorrelation for letf/side, right/side, mid/side

  // TODO: zero-pad to byte alignment

  // TODO: read frame footer

  return true;
}

std::optional<FrameHeader> FlacFile::decodeFrameHeader(etl::bit_stream_reader &reader)
{
  FrameHeader frame_header{};

  std::cout << " Decoding frame header.\n";
  // u(15) -> frame sync code (0b111111111111100)
  auto frame_sync_code = reader.read<uint16_t>(15).value();// NOLINT
  m_bits_read += 15;// NOLINT
  if (frame_sync_code != 0x7ffc) {// NOLINT
    std::cerr << "Invalid frame sync code: 0x" << std::hex << frame_sync_code << std::dec << "\n";
    return std::nullopt;
  }
  frame_header.frame_sync_code = frame_sync_code;
  std::cout << "\tFrame sync code: 0x" << std::hex << frame_sync_code << std::dec << "\n";

  // u(1) -> blocking strategy bit
  auto strategy_bit = static_cast<int>(reader.read<uint8_t>(1).value());// NOLINT
  m_bits_read += 1;// NOLINT
  frame_header.strategy_bit = strategy_bit;
  std::cout << "\tBlocking strategy: " << (strategy_bit == 0 ? "fixed" : "variable") << "\n";

  // u(4) -> block size bits
  auto block_size_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;// NOLINT
  frame_header.block_size_bits = block_size_bits;
  uint32_t block_size = determineBlockSize(block_size_bits);
  frame_header.block_size = block_size;

  // u(4) -> sample rate bits
  auto sample_rate_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;// NOLINT
  frame_header.sample_rate_bits = sample_rate_bits;
  uint32_t sample_rate = determineSampleRate(sample_rate_bits);

  if (sample_rate == 0) { sample_rate = m_sample_rate; }

  // u(4) -> channel bits
  auto channel_bits = static_cast<int>(reader.read<uint8_t>(4).value());
  m_bits_read += 4;// NOLINT
  frame_header.channel_bits = channel_bits;
  const uint16_t num_channels = determineChannels(channel_bits);
  frame_header.num_channels = num_channels;
  std::cout << "\tNum of channels: " << num_channels << " (" << channel_bits << ")\n";

  // u(3) -> bit depth bits
  auto bit_depth_bits = static_cast<int>(reader.read<uint8_t>(3).value());
  frame_header.bit_depth_bits = bit_depth_bits;
  m_bits_read += 3;// NOLINT
  uint16_t bit_depth = determineBitDepth(bit_depth_bits);

  if (bit_depth == 0) { bit_depth = m_bit_depth; }

  frame_header.bit_depth = bit_depth;
  std::cout << "\tBit depth: " << bit_depth << " (" << bit_depth_bits << ")\n";

  // u(1) -> reserved bit
  auto reserved_bit = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;// NOLINT
  if (reserved_bit != 0) {
    std::cerr << "\tReserved bit is not 0.\n";
    return std::nullopt;
  }

  // UTF-8 coded sampl/frame number (coded number)
  uint64_t coded_number = 0;
  if (strategy_bit == 0) {
    // fixed
    auto res = readUTF8(reader);
    if (res.has_value()) {
      coded_number = res.value();
    } else {
      throw std::runtime_error("Error decoding UTF-8 bytes");
    }
    std::cout << "\tFrame number: " << coded_number << "\n";
  } else {
    // variable
    auto res = readUTF8(reader);
    if (res.has_value()) {
      coded_number = res.value();
    } else {
      throw std::runtime_error("Error decoding UTF-8 bytes");
    }
    std::cout << "\tSample number: " << coded_number << "\n";
  }
  frame_header.coded_number = coded_number;

  // NOLINTBEGIN
  if (block_size_bits == 6) {
    block_size = reader.read<uint8_t>(8).value() + 1;
    m_bits_read += 8;// NOLINT
  } else if (block_size_bits == 7) {
    block_size = reader.read<uint16_t>(16).value() + 1;
    m_bits_read += 16;// NOLINT
  }
  frame_header.block_size = block_size;
  std::cout << "\tBlock size: " << block_size << " (" << block_size_bits << ")\n";

  if (sample_rate_bits == 12) {
    sample_rate = reader.read<uint8_t>(8).value() * 1000;
    m_bits_read += 8;// NOLINT
  } else if (sample_rate_bits == 13) {
    sample_rate = reader.read<uint16_t>(16).value();
    m_bits_read += 16;// NOLINT
  } else if (sample_rate_bits == 14) {
    sample_rate = reader.read<uint16_t>(16).value() * 10;
    m_bits_read += 16;// NOLINT
  }
  // NOLINTEND
  frame_header.sample_rate = sample_rate;
  std::cout << "\tSample rate: " << sample_rate << " (" << sample_rate_bits << ")\n";

  // u(8) -> CRC-8 of the frame header
  auto crc8 = static_cast<int>(reader.read<uint8_t>(8).value());// NOLINT
  m_bits_read += 8;// NOLINT

  frame_header.crc8 = crc8;
  std::cout << "\tCRC-8: 0x" << std::hex << crc8 << std::dec << "\n";

  return frame_header;
}

bool FlacFile::decodeSubframes(etl::bit_stream_reader &reader, FrameHeader &frame_header)
{
  std::cout << "Decoding subframes.\n";

  std::vector<std::vector<int32_t>> channel_data(frame_header.num_channels);

  for (uint16_t i = 0; i < frame_header.num_channels; ++i) {
    channel_data[i].resize(frame_header.block_size);

    uint16_t subframe_bit_depth = frame_header.bit_depth;
    if ((frame_header.channel_bits == 8 && i == 1) || (frame_header.channel_bits == 9 && i == 0)// NOLINT
        || (frame_header.channel_bits == 10 && i == 1)) {// NOLINT
      subframe_bit_depth = frame_header.bit_depth + 1;
    }

    if (!decodeSubframe(reader, channel_data[i], frame_header.block_size, subframe_bit_depth)) {
      std::cerr << "Failed to decode subframe.\n";
      return false;
    }
  }

  return true;
}

// NOLINTBEGIN
bool FlacFile::decodeSubframe(etl::bit_stream_reader &reader,
  std::vector<int32_t> &ch_data,
  uint32_t block_size,
  uint16_t subframe_bit_depth)
{
  std::cout << "Decoding a subframe.\n";

  // decode subframe header
  std::cout << "Subframe Header:\n";
  // u(1) -> reserved bit (must be 0)
  auto reserved_bit = reader.read<uint8_t>(1).value();
  m_bits_read += 1;

  if (static_cast<int>(reserved_bit) != 0) { throw std::runtime_error("The reserved bit must be 0.\n"); }
  std::cout << "\tReserved bit: " << static_cast<int>(reserved_bit) << "\n";

  // u(6) -> subframe type bits
  auto subframe_type_bits = static_cast<int>(reader.read<uint8_t>(6).value());// NOLINT
  m_bits_read += 6;// NOLINT
  auto subframe_type = determineSubframeType(subframe_type_bits);
  std::cout << "\tSubframe type: " << subframe_type << " (" << subframe_type_bits << ")\n";

  // u(1) -> does subframe uses wasted bits
  auto is_wasted_bits = static_cast<int>(reader.read<uint8_t>(1).value());
  m_bits_read += 1;
  std::cout << "\tWasted bits: " << (is_wasted_bits == 0 ? "no" : "yes") << "\n";

  // u(n) -> wasted bits per sample
  uint8_t wasted_bits = 0;
  if (is_wasted_bits == 1) {
    wasted_bits = 1;

    while (static_cast<int>(reader.read<uint8_t>(1).value()) == 0) {
      m_bits_read += 1;
      wasted_bits++;
    }

    std::cout << "\tWasted bits: " << wasted_bits << "\n";
  }

  uint16_t adjusted_bit_depth = subframe_bit_depth - wasted_bits;

  if (subframe_type_bits == 0) {
    std::cout << "\tDecoding " << subframe_type << ":\n";
    return decodeConstantSubframe(reader, ch_data, block_size, adjusted_bit_depth, wasted_bits);
  } else if (subframe_type_bits == 1) {
    std::cout << "\tDecoding " << subframe_type << ":\n";
    return decodeVerbatimSubframe(reader, ch_data, block_size, adjusted_bit_depth, wasted_bits);
  } else if (subframe_type_bits >= 8 && subframe_type_bits <= 12) {// NOLINT
    uint8_t order = subframe_type_bits & 0x07;// NOLINT
    std::cout << "\tDecoding " << subframe_type << " (" << static_cast<int>(order) << "):\n";
    return decodeFixedSubframe(reader, ch_data, block_size, adjusted_bit_depth, order, wasted_bits);
  } else if (subframe_type_bits >= 32) {// NOLINT
    uint8_t order = (subframe_type_bits & 0x1F) + 1;// NOLINT
    std::cout << "\tDecoding " << subframe_type << " (" << static_cast<int>(order) << "):\n";
    return decodeLPCSubframe(reader, ch_data, block_size, adjusted_bit_depth, order, wasted_bits);
  } else {
    std::cerr << "Reserved subframe type: " << subframe_type_bits << "\n";
    return false;
  }
}
// NOLINTEND

bool FlacFile::decodeConstantSubframe([[maybe_unused]] etl::bit_stream_reader &reader,
  [[maybe_unused]] std::vector<int32_t> &samples,
  [[maybe_unused]] uint32_t block_size,// NOLINT
  [[maybe_unused]] uint16_t bit_depth,
  [[maybe_unused]] uint8_t wasted_bits)
{
  return false;
}

bool FlacFile::decodeVerbatimSubframe([[maybe_unused]] etl::bit_stream_reader &reader,
  [[maybe_unused]] std::vector<int32_t> &samples,
  [[maybe_unused]] uint32_t block_size,// NOLINT
  [[maybe_unused]] uint16_t bit_depth,
  [[maybe_unused]] uint8_t wasted_bits)
{
  return false;
}

bool FlacFile::decodeFixedSubframe([[maybe_unused]] etl::bit_stream_reader &reader,
  [[maybe_unused]] std::vector<int32_t> &samples,
  [[maybe_unused]] uint32_t block_size,// NOLINT
  [[maybe_unused]] uint16_t bit_depth,
  [[maybe_unused]] uint8_t order,
  [[maybe_unused]] uint8_t wasted_bits)
{
  return false;
}

bool FlacFile::decodeLPCSubframe([[maybe_unused]] etl::bit_stream_reader &reader,
  [[maybe_unused]] std::vector<int32_t> &samples,
  [[maybe_unused]] uint32_t block_size,// NOLINT
  [[maybe_unused]] uint16_t bit_depth,
  [[maybe_unused]] uint8_t order,
  [[maybe_unused]] uint8_t wasted_bits)
{
  return false;
}

bool FlacFile::encodeFlacFile() { return false; }// NOLINT

// NOLINTBEGIN
std::optional<uint64_t> FlacFile::readUTF8(etl::bit_stream_reader &reader)
{
  auto first_byte = reader.read<uint8_t>(8).value();// NOLINT
  m_bits_read += 8;// NOLINT
  int num_bytes = utf8SequenceLength(first_byte);

  if (num_bytes == 0) { return std::nullopt; }

  uint64_t result = 0;

  switch (num_bytes) {
  case 1:
    result = first_byte & 0x7F;
    break;
  case 2: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(((first_byte & 0x1F) << 6) | (byte2 & 0x3F));

    break;
  }
  case 3: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(((first_byte & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F));

    break;
  }
  case 4: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80) { return std::nullopt; }

    result = static_cast<uint64_t>(
      ((first_byte & 0x07) << 18) | ((byte2 & 0x3F) << 12) | ((byte3 & 0x3F) << 6) | (byte4 & 0x3F));

    break;
  }
  case 5: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((first_byte & 0x03) << 24) | ((byte2 & 0x3F) << 18) | ((byte3 & 0x3F) << 12)
                                   | ((byte4 & 0x3F) << 6) | (byte5 & 0x3F));

    break;
  }
  case 6: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte6 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80
        || (byte6 & 0xC0) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((first_byte & 0x01) << 30) | ((byte2 & 0x3F) << 24) | ((byte3 & 0x3F) << 18)
                                   | ((byte4 & 0x3F) << 12) | ((byte5 & 0x3F) << 6) | (byte6 & 0x3F));

    break;
  }
  case 7: {
    auto byte2 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte3 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte4 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte5 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte6 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT
    auto byte7 = reader.read<uint8_t>(8).value();
    m_bits_read += 8;// NOLINT

    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80
        || (byte6 & 0xC0) != 0x80 || (byte7 & 0xC0) != 0x80) {
      return std::nullopt;
    }

    result = static_cast<uint64_t>(((byte2 & 0x3F) << 30) | ((byte3 & 0x3F) << 24) | ((byte4 & 0x3F) << 18)
                                   | ((byte5 & 0x3F) << 12) | ((byte6 & 0x3F) << 6) | (byte7 & 0x3F));

    break;
  }
  default:
    return std::nullopt;
  }

  return result;
}
// NOLINTEND

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

std::string determinePictureTypeStr(uint32_t picture_type)
{
  std::string picture_type_str = "Unknown";

  // NOLINTBEGIN
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
  }
  // NOLINTEND

  return picture_type_str;
}

uint32_t determineBlockSize(int block_size_bits)
{
  uint32_t block_size{};

  if (block_size_bits == 0) {
    throw std::runtime_error("Invalid block size bits (reserved)");
  } else if (block_size_bits == 1) {
    block_size = 192;// NOLINT
  } else if (block_size_bits >= 2 && block_size_bits <= 5) {// NOLINT
    block_size = static_cast<uint32_t>(144 * (1 << block_size_bits));// NOLINT
  } else if (block_size_bits >= 8 && block_size_bits <= 15) {// NOLINT
    block_size = static_cast<uint32_t>(1 << block_size_bits);// NOLINT
  }

  return block_size;
}
uint32_t determineSampleRate(int sample_rate_bits)
{
  uint32_t sample_rate{};

  // NOLINTBEGIN
  switch (sample_rate_bits) {
  case 0:
    std::cout << "Sample rate is stored in the streaminfo metadata block.\n";
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
  // NOLINTEND

  return sample_rate;
}

uint16_t determineChannels(int channel_bits)
{
  uint16_t channels{};

  // NOLINTBEGIN
  if (channel_bits <= 7) {
    channels = static_cast<uint16_t>(channel_bits + 1);
  } else if (channel_bits == 8 || channel_bits == 9 || channel_bits == 10) {
    channels = 2;
  } else {
    throw std::runtime_error("Reserved channel bits.\n");
  }
  // NOLINTEND

  return channels;
}

uint16_t determineBitDepth(int bit_depth_bits)
{
  uint16_t bit_depth{};

  // NOLINTBEGIN
  switch (bit_depth_bits) {
  case 0:
    std::cout << "Bit depth is stored in the streaminfo metadata block.\n";
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
    std::runtime_error("Unsupported bit depth bits.\n");
  }
  // NOLINTEND

  return bit_depth;
}

// NOLINTBEGIN
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
    subframe_type = "Subframe with a fixed predictor of order " + std::to_string(subframe_type_bits - 31);
  }

  return subframe_type;
}
// NOLINTEND

// NOLINTBEGIN
int utf8SequenceLength(uint8_t first_byte)
{
  if ((first_byte & 0x80) == 0x00) return 1;
  if ((first_byte & 0xE0) == 0xC0) return 2;
  if ((first_byte & 0xF0) == 0xE0) return 3;
  if ((first_byte & 0xF8) == 0xF0) return 4;
  if ((first_byte & 0xFC) == 0xF8) return 5;
  if ((first_byte & 0xFE) == 0xFC) return 6;
  if (first_byte == 0xFE) return 7;
  return 0;
}
// NOLINTEND

}// namespace afs
