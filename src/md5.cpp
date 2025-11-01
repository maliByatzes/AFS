#include <afsproject/md5.h>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace afs {

MD5::MD5()
{
  m_ctx = std::make_unique<MD5Context>();
  m_ctx->size = 0UL;

  m_ctx->buffer[0] = INITIAL_A;
  m_ctx->buffer[1] = INITIAL_B;
  m_ctx->buffer[2] = INITIAL_C;
  m_ctx->buffer[3] = INITIAL_D;
}

void MD5::update(const std::vector<uint8_t> &message)
{
  const uint64_t message_length = message.size();
  std::vector<uint32_t> input(16);
  uint32_t offset = m_ctx->size % 64;
  m_ctx->size += message_length;

  for (uint32_t i = 0; i < message_length; ++i) {
    m_ctx->input.at(offset++) = message.at(i);

    if (offset % 64 == 0) {
      for (uint64_t j = 0; j < 16; ++j) {
        input.at(j) = (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 3)) << 24U)
                      | (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 2)) << 16U)
                      | (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 1)) << 8U)
                      | static_cast<uint32_t>(m_ctx->input.at(j * 4));
      }
      step(input);
      offset = 0;
    }
  }
}

void MD5::finalize()
{
  std::vector<uint32_t> input(16);
  const uint32_t offset = m_ctx->size % 64;
  const uint32_t padding_length = offset < 56 ? 56 - offset : (56 + 24) - offset;

  std::vector<uint8_t> padding_bytes(padding_length);
  padding_bytes[0] = static_cast<uint8_t>(0x80);
  for (auto &byte : padding_bytes) { byte = static_cast<uint8_t>(0x00); }

  update(padding_bytes);
  m_ctx->size -= static_cast<uint64_t>(padding_length);

  for (uint64_t j = 0; j < 14; ++j) {
    input.at(j) = (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 3)) << 24U)
                  | (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 2)) << 16U)
                  | (static_cast<uint32_t>(m_ctx->input.at((j * 4) + 1)) << 8U)
                  | static_cast<uint32_t>(m_ctx->input.at((j * 4)));
  }

  input.at(14) = static_cast<uint32_t>(m_ctx->size * 8);
  input.at(15) = static_cast<uint32_t>((m_ctx->size * 8) >> 32U);

  step(input);

  for (uint32_t i = 0; i < 4; ++i) {
    m_ctx->digest.at((i * 4) + 0) = static_cast<uint8_t>((m_ctx->buffer.at(i) & 0x000000FFU));
    m_ctx->digest.at((i * 4) + 1) = static_cast<uint8_t>((m_ctx->buffer.at(i) & 0x0000FF00U) >> 8U);
    m_ctx->digest.at((i * 4) + 2) = static_cast<uint8_t>((m_ctx->buffer.at(i) & 0x00FF0000U) >> 16U);
    m_ctx->digest.at((i * 4) + 3) = static_cast<uint8_t>((m_ctx->buffer.at(i) & 0xFF000000U) >> 24U);
  }
}

void MD5::step(std::vector<uint32_t> &input)
{
  // NOLINTBEGIN
  uint32_t AA = m_ctx->buffer[0];
  uint32_t BB = m_ctx->buffer[1];
  uint32_t CC = m_ctx->buffer[2];
  uint32_t DD = m_ctx->buffer[3];

  uint32_t E;
  uint32_t j;
  // NOLINTEND

  for (uint32_t i = 0; i < 64; ++i) {
    switch (i / 16) {
    case 0:
      E = (BB & CC) | (~BB & DD);
      j = i;
      break;
    case 1:
      E = (BB & DD) | (CC & ~DD);
      j = ((i * 5) + 1) % 16;
      break;
    case 2:
      E = BB ^ CC ^ DD;
      j = ((i * 3) + 5) % 16;
      break;
    default:
      E = CC ^ (BB | ~DD);
      j = (i * 7) % 16;
      break;
    }

    const uint32_t temp = DD;
    DD = CC;
    CC = BB;
    BB = BB + std::rotl(AA + E + K[i] + input[j], static_cast<int>(SHIFT_AMOUNTS[i]));
    AA = temp;
  }

  m_ctx->buffer[0] += AA;
  m_ctx->buffer[1] += BB;
  m_ctx->buffer[2] += CC;
  m_ctx->buffer[3] += DD;
}

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
/*
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
