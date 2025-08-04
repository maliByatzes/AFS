#ifndef WAVE_H_
#define WAVE_H_

#include <asfproject/signal.h>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <complex>
#include <map>

namespace asf {

class Signal;

class Wave
{
private:
  nc::NdArray<float> m_ys;
  nc::NdArray<float> m_ts;
  int m_framerate;

public:
  Wave(const nc::NdArray<float> &ys, const nc::NdArray<float> &ts, int framerate);// NOLINT
  Wave(const nc::NdArray<std::complex<float>> &ys, const nc::NdArray<float> &ts, int framerate);// NOLINT

  Wave &operator+(Wave &other);
  Wave &operator*(Wave &other);
  Wave convolve(Wave &other) const;
  float corr(Wave &other) const;
  Wave cov(Wave &other) const;
  float maxDiff(Wave &other) const;

  [[nodiscard]] size_t len() const;
  void apodize(float denom=20, float duration = 0.1F) const;//NOLINT
  [[nodiscard]] float cosCov(float freq) const;
  void cosTransform() const;
  void covMat(Wave &other) const;
  [[nodiscard]] Wave cumsum() const;
  [[nodiscard]] Wave diff() const;
  void findIndex(float time) const;
  float getXFactor(std::map<std::string, float> &options) const;
  void hamming() const;
  void makeAudio() const;
  void makeDCT() const;
  void normalize(float amp) const;
  void play(std::string &filename) const;
  void plot(std::map<std::string, float> options={}) const;
  void plotVLines(std::map<std::string, float> &options) const;
  [[nodiscard]] Signal quantize(float bound, float dtype) const;
  void roll(int roll) const;
  void scale(float factor) const;
  [[nodiscard]] Wave segment(float start, float duration) const;
  void shift(float shift) const;
  void slice(int i, int j) const;//NOLINT
  void truncate(int index) const;
  void unbias() const;
  void window(int window) const;
  void write(std::string &filename) const;
  void zeroPad(int n) const;//NOLINT
  [[nodiscard]] float period() const;
  [[nodiscard]] float start() const;
  [[nodiscard]] float end() const; 
};

}// namespace asf

#endif
