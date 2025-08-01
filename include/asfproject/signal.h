#ifndef SIGNAL_H_
#define SIGNAL_H_

#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace asf {

class Wave;

constexpr int DEF_FRAMERATE = 11025;
constexpr float DEF_FREQ = 440.0;
constexpr float DEF_AMP = 1.0;

class Signal// NOLINT
{
public:
  virtual ~Signal() = default;

  [[nodiscard]] virtual std::unique_ptr<Signal> clone() const = 0;
  [[nodiscard]] Wave makeWave(float duration = 1.0, float start = 0, int framerate = DEF_FRAMERATE) const;
  void plot(int framerate = DEF_FRAMERATE);
  [[nodiscard]] virtual float period() const;
  virtual std::vector<float> evaluate(const std::vector<float> &ts) const = 0;// NOLINT
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
  std::vector<float> evaluate(const std::vector<float> &ts) const override;//NOLINT
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
  std::vector<float> evaluate(const std::vector<float> &ts) const override;//NOLINT 
  [[nodiscard]] float period() const override;
};

// NOTE: Just throw these around here, I'll find a suitable spot for them.
template<typename T>
std::vector<T> arange(T stop);

template<typename T>
std::vector<T> arange(T start, T stop, T step = 1);

}// namespace asf

#endif
