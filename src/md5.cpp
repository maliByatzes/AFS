#include <afsproject/md5.h>
#include <cstdint>
#include <string>

namespace afs {

// NOLINTBEGIN
MD5::MD5() { init(); }

void MD5::update(const std::string &input) { update(reinterpret_cast<const uint8_t *>(input.c_str()), input.length()); }

void MD5::update(const std::vector<uint8_t> &input) { update(input.data(), input.size()); }

void MD5::update(const uint8_t *input_buffer, size_t input_len)
{
  uint32_t offset = m_size % 64;
  m_size += input_len;

  for (size_t i = 0; i < input_len; ++i) {
    m_input.at(offset++) = input_buffer[i];

    if (offset % 64 == 0) {
      std::array<uint32_t, 16> input_block{};
      for (unsigned int j = 0; j < 16; ++j) {
        input_block.at(j) =
          static_cast<uint32_t>(m_input.at((j * 4) + 3)) << 24U | static_cast<uint32_t>(m_input.at((j * 4) + 2)) << 16U
          | static_cast<uint32_t>(m_input.at((j * 4) + 1)) << 8U | static_cast<uint32_t>(m_input.at((j * 4)));
      }
      step(input_block);
      offset = 0;
    }
  }
}

std::array<uint8_t, 16> MD5::finalize()
{
  unsigned int offset = m_size % 64;
  unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

  update(PADDING.data(), padding_length);
  m_size -= padding_length;

  std::array<uint32_t, 16> input_block{};
  for (unsigned int j = 0; j < 14; ++j) {
    input_block.at(j) =
      static_cast<uint32_t>(m_input.at((j * 4) + 3)) << 24U | static_cast<uint32_t>(m_input.at((j * 4) + 2)) << 16U
      | static_cast<uint32_t>(m_input.at((j * 4) + 1)) << 8U | static_cast<uint32_t>(m_input.at((j * 4)));
  }
  input_block.at(14) = static_cast<uint32_t>(m_size * 8);
  input_block.at(15) = static_cast<uint32_t>((m_size * 8) >> 32U);

  step(input_block);

  std::array<uint8_t, 16> digest{};
  for (unsigned int i = 0; i < 4; ++i) {
    digest.at((i * 4) + 0) = static_cast<uint8_t>((m_buffer.at(i) & 0x000000FFU));
    digest.at((i * 4) + 1) = static_cast<uint8_t>((m_buffer.at(i) & 0x0000FF00U) >> 8U);
    digest.at((i * 4) + 2) = static_cast<uint8_t>((m_buffer.at(i) & 0x00FF0000U) >> 16U);
    digest.at((i * 4) + 3) = static_cast<uint8_t>((m_buffer.at(i) & 0xFF000000U) >> 24U);
  }

  return digest;
}

std::array<uint8_t, 16> MD5::compute(const std::string &input)
{
  MD5 md5;
  md5.update(input);
  return md5.finalize();
}

std::string MD5::to_hex(const std::array<uint8_t, 16> &digest)
{
  const char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve(32);

  for (uint8_t byte : digest) {
    result += hex_chars[(byte >> 4U) & 0x0FU];
    result += hex_chars[byte & 0x0FU];
  }

  return result;
}

constexpr uint32_t MD5::F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }

constexpr uint32_t MD5::G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }

constexpr uint32_t MD5::H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }

constexpr uint32_t MD5::I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

constexpr uint32_t MD5::rotate_left(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }// NOLINT

void MD5::init()
{
  m_size = 0;
  m_buffer[0] = A;
  m_buffer[1] = B;
  m_buffer[2] = C;
  m_buffer[3] = D;
}

void MD5::step(const std::array<uint32_t, 16> &input)
{
  uint32_t AA = m_buffer[0];
  uint32_t BB = m_buffer[1];
  uint32_t CC = m_buffer[2];
  uint32_t DD = m_buffer[3];

  for (unsigned int i = 0; i < 64; ++i) {
    uint32_t E;
    unsigned int j;

    switch (i / 16) {
    case 0:
      E = F(BB, CC, DD);
      j = i;
      break;
    case 1:
      E = G(BB, CC, DD);
      j = ((i * 5) + 1) % 16;
      break;
    case 2:
      E = H(BB, CC, DD);
      j = ((i * 3) + 5) % 16;
      break;
    default:
      E = I(BB, CC, DD);
      j = (i * 7) % 16;
      break;
    }

    uint32_t temp = DD;
    DD = CC;
    CC = BB;
    BB = BB + rotate_left(AA + E + K.at(i) + input.at(j), S.at(i));
    AA = temp;
  }

  m_buffer[0] += AA;
  m_buffer[1] += BB;
  m_buffer[2] += CC;
  m_buffer[3] += DD;
}
// NOLINTEND
/*
std::string MD5::to_hex_string(const std::vector<uint8_t> &bytes)
{
  std::string hex_string{};
  std::stringstream stream;
  for (const uint8_t &byte : bytes) {
    stream << std::setfill('0') << std::setw(2) << std::hex << (byte & 0xffU);
    hex_string += stream.str();
    stream.str("");
  }
  return hex_string;
}

std::vector<uint8_t> MD5::to_byte_vector(const std ::string &text)
{
  std::vector<uint8_t> bytes;
  bytes.reserve(text.size());
  std::ranges::transform(
    text, std::back_inserter(bytes), [](char chr) -> uint8_t { return static_cast<uint8_t>(chr); });
  return bytes;
}

std::vector<uint8_t> MD5::string(const std::string &message)
{
  update(to_byte_vector(message));
  finalize();

  std::vector<uint8_t> result(m_ctx->digest.begin(), m_ctx->digest.end());
  return result;
}

std::vector<uint8_t> computeMD5(const std::vector<uint8_t> &message)
{
  uint64_t message_length_bytes = message.size();
  uint32_t number_blocks = static_cast<uint32_t>((message_length_bytes + 8) >> 6U) + 1;
  uint32_t total_length = number_blocks << 6U;
  std::vector<uint8_t> padding_bytes(total_length - message_length_bytes);
  padding_bytes[0] = static_cast<uint8_t>(0x80);
  uint64_t message_length_bits = message_length_bytes << 3U;
  for (uint32_t i = 0; i < 8; ++i) {
    padding_bytes[padding_bytes.size() - 8 + i] = static_cast<uint8_t>(message_length_bits);
    message_length_bits >>= 8U;
  }

  uint32_t a = INITIAL_A;
  uint32_t b = INITIAL_B;
  uint32_t c = INITIAL_C;
  uint32_t d = INITIAL_D;

  std::vector<uint32_t> buffer(16);
  for (uint32_t i = 0; i < number_blocks; ++i) {
    uint32_t index = i << 6U;
    for (uint32_t j = 0; j < 64; index++, ++j) {
      buffer[j >> 2U] =
        (static_cast<uint32_t>(
           (index < message_length_bytes) ? message[index] : padding_bytes[index - message_length_bytes])
          << 24U)
        | (buffer[j >> 2U] >> 8U);
    }

    uint32_t original_A = a;
    uint32_t original_B = b;
    uint32_t original_C = c;
    uint32_t original_D = d;

    for (uint32_t j = 0; j < 64; ++j) {
      uint32_t div16 = j >> 4U;
      uint32_t f = 0;
      uint32_t buffer_index = j;
      switch (div16) {
      case 0:
        f = (b & c) | (~b & d);
        break;
      case 1:
        f = (b & d) | (c & ~d);
        buffer_index = (buffer_index * 5 + 1) & 0x0FU;
        break;
      case 2:
        f = b ^ c ^ d;
        buffer_index = (buffer_index * 3 + 5) & 0x0FU;
        break;
      case 3:
        f = c ^ (b | ~d);
        buffer_index = (buffer_index * 7) & 0x0FU;
        break;
      }
      uint32_t temp =
        b + std::rotl(a + f + buffer[buffer_index] + K[j], static_cast<int>(SHIFT_AMOUNTS[(div16 << 2U) | (j & 3U)]));

      a = d;
      d = c;
      c = b;
      b = temp;
    }

    a += original_A;
    b += original_B;
    c += original_C;
    d += original_D;
  }

  std::vector<uint8_t> md5(16);
  uint32_t count = 0;
  for (uint32_t i = 0; i < 4; ++i) {
    uint32_t n = (i == 0) ? a : ((i == 1) ? b : ((i == 2) ? c : d));
    for (uint32_t j = 0; j < 4; ++j) {
      md5[count++] = static_cast<uint8_t>(n);
      n >>= 8U;
    }
  }

  return md5;
}*/
}// namespace afs
