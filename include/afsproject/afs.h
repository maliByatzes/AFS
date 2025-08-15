#ifndef afs_h_
#define afs_h_

#include <afsproject/audio_file.h>
#include <vector>

namespace afs {

class AFS// NOLINT
{
private:
  std::vector<std::vector<double>> m_matrix;

  static void normalizePCMData(IAudioFile &);
  static void stereoToMono(IAudioFile &);
  static void applyLowPassFilter(IAudioFile &);
  static void downSampling(IAudioFile &);
public:
  AFS() = default;
  ~AFS() = default;
  
  void shortTimeFourierTransform(IAudioFile &);
};

}// namespace afs

#endif
