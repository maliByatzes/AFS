#include <NumCpp/Core/Constants.hpp>
#include <NumCpp/Functions/arange.hpp>
#include <NumCpp/Functions/cos.hpp>
#include <NumCpp/Functions/sin.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/signal.h>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace asf {

Wave Signal::makeWave(float duration, float start, int framerate) const//NOLINT
{
  const float size = std::round(duration * float(framerate));
  nc::NdArray<float> time_values = nc::arange<float>(size);
  std::ranges::for_each(time_values, [&](float &val) { val = start + val / float(framerate); });
  const nc::NdArray<float> ys = evaluate(time_values);
  return { ys, time_values, framerate };
}

void Signal::plot(int framerate) const
{
  const float duration = period() * 3;
  const Wave wave = makeWave(duration, 0.0F, framerate);
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

nc::NdArray<float> Sinusoid::evaluate(const nc::NdArray<float> &ts) const// NOLINT
{
  nc::NdArray<float> phases(ts.size());
  for (int i = 0; i < int(ts.size()); ++i) {
    phases[i] = float(nc::constants::twoPi) * m_freq * ts[i] + m_offset;// NOLINT
  }
  nc::NdArray<float> ys(phases.size());
  for (int i = 0; i < int(phases.size()); ++i) { ys[i] = m_amp * m_func(phases[i]); }
  return ys;
}

float Sinusoid::period() const { return 1.0F / m_freq; }

SumSignal::SumSignal(std::unique_ptr<Signal> sig1, std::unique_ptr<Signal> sig2)
{
  m_signals.push_back(std::move(sig1));
  m_signals.push_back(std::move(sig2));
}

SumSignal::SumSignal(std::vector<std::unique_ptr<Signal>> signals) : m_signals(std::move(signals)) {}

SumSignal::SumSignal(const SumSignal &other)
{
  for (const auto &sig_ptr : other.m_signals) {
    if (sig_ptr) { m_signals.push_back(sig_ptr->clone()); }
  }
}

std::unique_ptr<Signal> SumSignal::clone() const { return std::make_unique<SumSignal>(*this); }

nc::NdArray<float> SumSignal::evaluate(const nc::NdArray<float> &ts) const// NOLINT
{
  if (m_signals.empty()) { return {}; }

  nc::NdArray<float> sum = m_signals[0]->evaluate(ts);

  for (size_t i = 1; i < m_signals.size(); ++i) {
    if (m_signals[i]) { sum += m_signals[i]->evaluate(ts); }
  }

  return sum;
}

float SumSignal::period() const
{
  if (m_signals.empty()) {
    return DEF_PERIOD;// NOLINT
  }

  float maxp = 0.0;
  for (const auto &sig_ptr : m_signals) {
    if (sig_ptr) { maxp = std::max(maxp, sig_ptr->period()); }
  }

  return maxp > 0 ? maxp : DEF_PERIOD;// NOLINT
}

std::unique_ptr<Sinusoid> cosSignal(float freq, float amp, float offset)
{
  return std::make_unique<Sinusoid>(freq, amp, offset, static_cast<float (*)(float)>(nc::cos));  
}

std::unique_ptr<Sinusoid> sinSignal(float freq, float amp, float offset)
{
  return std::make_unique<Sinusoid>(freq, amp, offset, static_cast<float (*)(float)>(nc::sin));  
}

std::unique_ptr<Signal> operator+(const Signal &sig1, const Signal &sig2)
{
  return std::make_unique<SumSignal>(sig1.clone(), sig2.clone());
}

}// namespace asf
