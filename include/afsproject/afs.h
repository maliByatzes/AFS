#ifndef afs_h_
#define afs_h_

#include <afsproject/audio_file.h>
#include <map>
#include <vector>

namespace afs {

class AFS// NOLINT
{
private:
  std::vector<std::vector<std::pair<int, double>>> m_matrix;

  static void normalizePCMData(IAudioFile &);
  static void stereoToMono(IAudioFile &);
  static void applyLowPassFilter(IAudioFile &);
  static void downSampling(IAudioFile &);

public:
  AFS() = default;

  void shortTimeFourierTransform(IAudioFile &);
  std::vector<std::vector<std::pair<int, double>>> filtering();
  void generateTargetZones(std::vector<std::vector<std::pair<int, double>>> &);
};

}// namespace afs

#endif
