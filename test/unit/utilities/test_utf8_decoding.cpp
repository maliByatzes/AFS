#include <afsproject/flac_file.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

namespace afs::test {

class BitStreamHelper
{
public:
  static etl::bit_stream_reader create(const std::vector<uint8_t> &data)
  {
    etl::span<const uint8_t> span(data.data(), data.size());
    return etl::bit_stream_reader(span, etl::endian::big);
  }
};

TEST_CASE("UTF-8 Decoding - Single Byte", "[utf8]")
{
  auto flac_file = std::make_unique<FlacFile>();

  std::vector<uint8_t> data = { 0x7F };
  auto reader = BitStreamHelper::create(data);

  uint64_t result = flac_file->readUTF8(reader);
  REQUIRE(result == 127);
}

}// namespace afs::test
