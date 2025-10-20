#include <afsproject/flac_file.h>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace afs::test {

class FlacDecoderFixture
{
protected:
  std::string fixture_base = std::string(TEST_FIXTURES_DIR);

  std::string get_stereo_fixture()
  {
    const std::string dir = fixture_base + "/subset/stereo";
    return find_first_flac(dir);
  }

private:
  static std::string find_first_flac(const std::string &dir)
  {
    if (!std::filesystem::exists(dir)) { return ""; }

    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".flac") { return entry.path().string(); }
    }

    return "";
  }
};

TEST_CASE_METHOD(FlacDecoderFixture, "Load valid stereo FLAC file", "[flac][stereo]")
{
  const std::string path = get_stereo_fixture();
  REQUIRE(!path.empty());

  auto flac = std::make_unique<afs::FlacFile>();
  const bool result = flac->load(path);
  REQUIRE(!result);
}

}// namespace afs::test
