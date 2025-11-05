#ifndef afs_h_
#define afs_h_

#include <afsproject/audio_file.h>
#include <afsproject/db.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace afs {

using Matrix = std::vector<std::vector<std::pair<int, double>>>;
using Fingerprint = std::unordered_map<uint32_t, std::vector<uint64_t>>;

const double TIME_STEP = 0.046;
const double BIN_SIZE = 10.7;

const uint16_t NINE_BITS_MASK = 0x1FF;
const uint16_t FOURTEEN_BITS_MASK = 0x3FFF;
const uint32_t THIRTY_TWO_BITS_MASK = 0xFFFFFFFF;

class AFS// NOLINT
{
private:
  static void normalizePCMData(IAudioFile &);
  static void stereoToMono(IAudioFile &);
  static void applyLowPassFilter(IAudioFile &);
  static void downSampling(IAudioFile &);
  static Matrix shortTimeFourierTransform(IAudioFile &);
  static void filtering(Matrix &);
  static Fingerprint generateFingerprints(Matrix &, long long);

public:
  AFS() = default;

  static void storingFingerprints(IAudioFile &, long long, SQLiteDB &);
};

}// namespace afs

#endif
