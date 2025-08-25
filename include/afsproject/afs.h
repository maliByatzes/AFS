#ifndef afs_h_
#define afs_h_

#include <afsproject/audio_file.h>
#include <vector>

namespace afs {

using Matrix = std::vector<std::vector<std::pair<int, double>>>;
using Fingerprint = std::vector<std::tuple<double, double, int, int, int>>;

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
