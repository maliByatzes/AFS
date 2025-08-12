#ifndef wave_h_
#define wave_h_

#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <complex>
#include <map>
#include <optional>

namespace asf {

class Spectrum;
class Spectrogram;

class Wave
{
private:
  nc::NdArray<double> m_ys;
  nc::NdArray<double> m_ts;
  int m_framerate;

public:
  explicit Wave(const nc::NdArray<double> &ys);
  Wave(const nc::NdArray<double> &ys, int framerate);
  Wave(const nc::NdArray<double> &ys, const nc::NdArray<double> &ts, int framerate);// NOLINT
  Wave(const nc::NdArray<std::complex<double>> &ys, const nc::NdArray<double> &ts, int framerate);// NOLINT

  Wave &operator+(Wave &other);
  Wave &operator*(Wave &other);
  Wave convolve(Wave &other) const;
  double corr(Wave &other) const;
  Wave cov(Wave &other) const;
  double maxDiff(Wave &other) const;

  [[nodiscard]] size_t len() const;
  void apodize(double denom = 20, double duration = 0.1);// NOLINT
  [[nodiscard]] double cosCov(double freq) const;
  void cosTransform() const;
  void covMat(Wave &other) const;
  [[nodiscard]] Wave cumsum() const;
  [[nodiscard]] Wave diff() const;
  [[nodiscard]] int findIndex(double time) const;
  static double getXFactor(std::map<std::string, double> &options);
  void hamming();
  void window(nc::NdArray<double> &window);
  void makeAudio() const;
  void makeDCT() const;
  Spectrogram makeSpectrogram(int seg_length, bool win_flag = true);
  void normalize(double amp = 1.0);
  void play(std::string &filename) const;
  void plot(std::map<std::string, double> options = {}) const;
  void plotVLines(std::map<std::string, double> &options) const;
  template<typename dtype> [[nodiscard]] nc::NdArray<double> quantize(double bound) const;
  void roll(int roll) const;
  void scale(double factor) const;
  [[nodiscard]] Wave segment(std::optional<double> start, std::optional<double> duration) const;
  void shift(double shift) const;
  [[nodiscard]] Wave slice(int i, int j) const;// NOLINT
  void truncate(int index) const;
  void unbias() const;
  void write(const std::string &filename) const;
  void zeroPad(int n) const;// NOLINT
  [[nodiscard]] double period() const;
  [[nodiscard]] double start() const;
  [[nodiscard]] double end() const;
  Spectrum makeSpectrum();

  [[nodiscard]] nc::NdArray<double> getYs() const;
  [[nodiscard]] nc::NdArray<double> getTs() const;
  [[nodiscard]] int getFramerate() const;
};

nc::NdArray<double> normalize(const nc::NdArray<double> &ys, double amp = 1.0);
nc::NdArray<double>
  apodize(const nc::NdArray<double> &ys, int framerate, double denom = 20, double duration = 0.1);// NOLINT
template<typename dtype> nc::NdArray<dtype> quantize(const nc::NdArray<double> &ys, double bound);// NOLINT

}// namespace asf

#endif
