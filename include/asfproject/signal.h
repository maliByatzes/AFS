#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <asfproject/wave.h>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include <NumCpp.hpp>

namespace asf {

class Wave;

constexpr int DEF_FRAMERATE = 11025;
constexpr double DEF_FREQ = 440.0;
constexpr double DEF_AMP = 1.0;
constexpr double DEF_PERIOD = 0.1;

class Signal// NOLINT
{
public:
  virtual ~Signal() = default;

  [[nodiscard]] virtual std::unique_ptr<Signal> clone() const = 0;
  [[nodiscard]] Wave makeWave(double duration = 1.0, double start = 0, int framerate = DEF_FRAMERATE) const;
  void plot(int framerate = DEF_FRAMERATE) const;
  [[nodiscard]] virtual double period() const;
  virtual nc::NdArray<double> evaluate(const nc::NdArray<double> &ts) const = 0;// NOLINT
};

class Sinusoid : public Signal//NOLINT
{
private:
  double m_freq;
  double m_amp;
  double m_offset;
  std::function<double(double)> m_func;

public:
  explicit Sinusoid(double freq = DEF_FREQ,
    double amp = DEF_AMP,
    double offset = 0,
    std::function<double(double)> func = static_cast<double (*)(double)>(std::sin));
  Sinusoid(const Sinusoid &other);

  [[nodiscard]] std::unique_ptr<Signal> clone() const override;
  nc::NdArray<double> evaluate(const nc::NdArray<double> &ts) const override;//NOLINT
  [[nodiscard]] double period() const override;
};

class SumSignal : public Signal//NOLINT
{
private:
  std::vector<std::unique_ptr<Signal>> m_signals;

public:
  SumSignal(std::unique_ptr<Signal> sig1, std::unique_ptr<Signal> sig2);
  explicit SumSignal(std::vector<std::unique_ptr<Signal>> signals);
  SumSignal(const SumSignal &other);

  [[nodiscard]] std::unique_ptr<Signal> clone() const override;
  nc::NdArray<double> evaluate(const nc::NdArray<double> &ts) const override;//NOLINT 
  [[nodiscard]] double period() const override;
};

// Standalone functions

std::unique_ptr<Sinusoid> cosSignal(double freq = DEF_FREQ, double amp = DEF_AMP, double offset = 0);
std::unique_ptr<Sinusoid> sinSignal(double freq = DEF_FREQ, double amp = DEF_AMP, double offset = 0);
std::unique_ptr<Signal> operator+(const Signal &sig1, const Signal &sig2);

}// namespace asf

#endif
