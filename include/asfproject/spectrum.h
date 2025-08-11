#ifndef spectrum_h_
#define spectrum_h_

#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <complex>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace asf {

class Wave;

long findIndex(double value, const nc::NdArray<double> &arr);

class SpectrumParent// NOLINT
{
protected:
  nc::NdArray<std::complex<double>> m_hs;
  nc::NdArray<double> m_fs;
  int m_framerate;
  size_t m_orig_n;
  bool m_full;

  [[nodiscard]] std::pair<nc::NdArray<double>, nc::NdArray<double>> renderFull(std::optional<double> high) const;

public:
  SpectrumParent(const nc::NdArray<std::complex<double>> &hs,// NOLINT
    const nc::NdArray<double> &fs,// NOLINT
    int framerate,
    size_t orig_n,
    bool full = false);
  virtual ~SpectrumParent() = default;

  [[nodiscard]] std::unique_ptr<SpectrumParent> clone() const;

  [[nodiscard]] double maxFreq() const;
  [[nodiscard]] nc::NdArray<double> amps() const;
  [[nodiscard]] nc::NdArray<double> power() const;

  [[nodiscard]] double maxDiff(const SpectrumParent &other) const;
  [[nodiscard]] SpectrumParent ratio(const SpectrumParent &denom, double thresh = 1.0, double val = 0.0) const;
  [[nodiscard]] SpectrumParent invert() const;

  [[nodiscard]] double freqRes() const;

  void plot(std::optional<double> high);
  void plotPower(std::optional<double> high);

  [[nodiscard]] std::vector<std::pair<double, double>> peaks() const;
};

class Spectrum : public SpectrumParent// NOLINT
{
public:
  Spectrum(const nc::NdArray<std::complex<double>> &hs,// NOLINT
    const nc::NdArray<double> &fs,// NOLINT
    int framerate,
    size_t orig_n,
    bool full = false);

  Spectrum(const Spectrum &other) = default;

  [[nodiscard]] size_t size() const;

  [[nodiscard]] Spectrum operator+(const Spectrum &other) const;
  [[nodiscard]] Spectrum operator+(int val) const;
  [[nodiscard]] Spectrum operator*(const Spectrum &other) const;

  [[nodiscard]] Spectrum convolve(const Spectrum &other) const;

  [[nodiscard]] nc::NdArray<double> real() const;
  [[nodiscard]] nc::NdArray<double> imag() const;
  [[nodiscard]] nc::NdArray<double> angles() const;

  void scale(const std::complex<double> &factor);
  void lowPass(double cutoff, double factor = 0.0);
  void highPass(double cutoff, double factor = 0.0);
  void bandStop(double low_cutoff, double high_cutoff, double factor = 0.0);
  void pinkFilter(double beta = 1.0);

  [[nodiscard]] Spectrum differentiate() const;
  [[nodiscard]] Spectrum integrate() const;

  // IntegratedSpectrum makeIntegratedSpectrum() const;

  [[nodiscard]] Wave makeWave() const;
};

[[nodiscard]] inline Spectrum operator+(int val, const Spectrum &spec) { return spec + val; }
template<typename T> nc::NdArray<T> fftShift(const nc::NdArray<T> &arr);
template<typename T> nc::NdArray<T> ifftShift(const nc::NdArray<T> &arr);

}// namespace asf

#endif
