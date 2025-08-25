#ifndef afs_h_
#define afs_h_

#include <afsproject/audio_file.h>
#include <cstdint>
#include <vector>

namespace afs {

using Matrix = std::vector<std::vector<std::pair<int, double>>>;
using Fingerprint = std::vector<std::tuple<int, int, int, int, int>>;

const double TIME_STEP = 0.046;
const double BIN_SIZE = 10.7;

const uint16_t NINE_BITS_MASK = 0x1FF;
const uint16_t FOURTEEN_BITS_MASK = 0x3FFF;

class AFS// NOLINT
{
private:
  static void normalizePCMData(IAudioFile &);
  static void stereoToMono(IAudioFile &);
  static void applyLowPassFilter(IAudioFile &);
  static void downSampling(IAudioFile &);
  static Matrix shortTimeFourierTransform(IAudioFile &);
  static void filtering(Matrix &);
  static Fingerprint generateFingerprints(Matrix &);

public:
  AFS() = default;

  static void storingFingerprints(IAudioFile &);
};

}// namespace afs

#endif
