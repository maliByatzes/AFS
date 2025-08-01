#include <algorithm>
#include <asfproject/signal.h>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace asf {

Wave Signal::makeWave(float duration, float start, int framerate) const
{
  float size = std::round(duration * float(framerate));
  std::vector<float> time_values = arange<float>(size);
  std::ranges::for_each(time_values, [&](float &val) { val = start + val / float(framerate); });
  std::vector<float> ys = evaluate(time_values);
  return Wave(ys, time_values, framerate);
}

void Signal::plot(int framerate) const
{
  float duration = period() * 3;
  Wave wave = makeWave(duration, 0.0f, framerate);
  wave.plot();
}

float Signal::period() const { return DEF_PERIOD; }// NOLINT


Sinusoid::Sinusoid(float freq, float amp, float offset, std::function<float(float)> func)// NOLINT
  : m_freq(freq), m_amp(amp), m_offset(offset), m_func(std::move(func))
{}

Sinusoid::Sinusoid(const Sinusoid &other)
  : m_freq(other.m_freq), m_amp(other.m_amp), m_offset(other.m_offset), m_func(other.m_func)
{}

std::unique_ptr<Signal> Sinusoid::clone() const { return std::make_unique<Sinusoid>(*this); }

std::vector<float> Sinusoid::evaluate(const std::vector<float> &ts) const// NOLINT
{
  std::vector<float> phases(ts.size());
  for (size_t i = 0; i < ts.size(); ++i) {
    phases[i] = float(2.0 * M_PI) * m_freq * ts[i] + m_offset;// NOLINT
  }
  std::vector<float> ys(phases.size());
  for (size_t i = 0; i < phases.size(); ++i) { ys[i] = m_amp * m_func(phases[i]); }
  return ys;
}

float Sinusoid::period() const { return 1.0F / m_freq; }


SumSignal::SumSignal(std::unique_ptr<Signal> sig1, std::unique_ptr<Signal> sig2)
{
  m_signals.push_back(std::move(sig1));
  m_signals.push_back(std::move(sig2));
}

SumSignal::SumSignal(std::vector<std::unique_ptr<Signal>> signals) : m_signals(std::move(signals))
{}

SumSignal::SumSignal(const SumSignal &other)
{
  for (const auto &sig_ptr : other.m_signals) {
    if (sig_ptr) {
      m_signals.push_back(sig_ptr->clone());
    }
  }
}

std::unique_ptr<Signal> SumSignal::clone() const
{
  return std::make_unique<SumSignal>(*this);  
}

std::vector<float> SumSignal::evaluate(const std::vector<float> &ts) const//NOLINT
{
  if (m_signals.empty()) {
    return {};
  }

  std::vector<float> sum = m_signals[0]->evaluate(ts);

  for (size_t i = 1; i < m_signals.size(); ++i) {
    if (m_signals[i]) {
      sum += m_signals[i]->evaluate(ts);
    }
  }

  return sum;
}

float SumSignal::period() const
{
  if (m_signals.empty()) {
    return DEF_PERIOD;//NOLINT
  }

  float maxp = 0.0;
  for (const auto &sig_ptr : m_signals) {
    if (sig_ptr) {
      maxp = std::max(maxp, sig_ptr->period());
    }
  }

  return maxp > 0 ? maxp : DEF_PERIOD;//NOLINT
}

/*============= Helping functions ======================*/

template<typename T> std::vector<T> arange(T stop)
{
  if (stop <= 0) { throw std::invalid_argument("stop value must be greater than zero."); }

  return arange<T>(0, stop, 1);
}

template<typename T> std::vector<T> arange(T start, T stop, T step)// NOLINT
{
  static_assert(std::is_arithmetic_v<T>, "Can only be used with arithmetic types.");

  if (step > 0 && stop < start) {
    throw std::invalid_argument("stop value must be larger than the start value for a positive step.");
  }

  if (step < 0 && stop > start) {
    throw std::invalid_argument("start value must be larger than the stop value for a negative step.");
  }

  std::vector<T> values;
  T value = start;
  auto counter = T{ 1 };

  if (step > 0) {
    while (value < stop) {
      values.push_back(value);
      value = start + step * counter++;
    }
  } else {
    while (value > stop) {
      values.push_back(value);
      value = start + step * counter++;
    }
  }

  return values;
}

}// namespace asf
