#ifndef spectrogram_h_
#define spectrogram_h_

#include <afsproject/spectrum.h>
#include <afsproject/wave.h>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <map>
#include <optional>
#include <vector>

namespace afs {

class Spectrogram
{
private:
  std::map<double, Spectrum> m_spec_map;
  int m_seg_length;

public:
  Spectrogram(std::map<double, Spectrum> spec_map, int seg_length);

  [[nodiscard]] const Spectrum &anySpectrum() const;
  [[nodiscard]] double timeRes() const;
  [[nodiscard]] double freqRes() const;
  [[nodiscard]] std::vector<double> times() const;
  [[nodiscard]] nc::NdArray<double> frequencies() const;
  void plot(std::optional<double> high = std::nullopt) const;
  void getData(std::optional<double> high = std::nullopt) const;
  [[nodiscard]] Wave makeWave() const;
};

}// namespace afs

#endif
