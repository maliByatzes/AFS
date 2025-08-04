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
constexpr float DEF_FREQ = 440.0;
constexpr float DEF_AMP = 1.0;
constexpr float DEF_PERIOD = 0.1F;

class Signal// NOLINT
{
public:
  virtual ~Signal() = default;

  [[nodiscard]] virtual std::unique_ptr<Signal> clone() const = 0;
  [[nodiscard]] Wave makeWave(float duration = 1.0, float start = 0, int framerate = DEF_FRAMERATE) const;
  void plot(int framerate = DEF_FRAMERATE) const;
  [[nodiscard]] virtual float period() const;
  virtual nc::NdArray<float> evaluate(const nc::NdArray<float> &ts) const = 0;// NOLINT
};

class Sinusoid : public Signal//NOLINT
{
private:
  float m_freq;
  float m_amp;
  float m_offset;
  std::function<float(float)> m_func;

public:
  explicit Sinusoid(float freq = DEF_FREQ,
    float amp = DEF_AMP,
    float offset = 0,
    std::function<float(float)> func = static_cast<float (*)(float)>(std::sin));
  Sinusoid(const Sinusoid &other);

  [[nodiscard]] std::unique_ptr<Signal> clone() const override;
  nc::NdArray<float> evaluate(const nc::NdArray<float> &ts) const override;//NOLINT
  [[nodiscard]] float period() const override;
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
  nc::NdArray<float> evaluate(const nc::NdArray<float> &ts) const override;//NOLINT 
  [[nodiscard]] float period() const override;
};

}// namespace asf

#endif
