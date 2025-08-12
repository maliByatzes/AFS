#include <NumCpp/Core/Constants.hpp>
#include <NumCpp/Functions/arange.hpp>
#include <NumCpp/Functions/cos.hpp>
#include <NumCpp/Functions/cumsum.hpp>
#include <NumCpp/Functions/diff.hpp>
#include <NumCpp/Functions/roll.hpp>
#include <NumCpp/Functions/sin.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/signal.h>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace asf {

Wave Signal::makeWave(double duration, double start, int framerate) const// NOLINT
{
  const double size = std::round(duration * double(framerate));
  nc::NdArray<double> time_values = nc::arange<double>(size);
  std::ranges::for_each(time_values, [&](double &val) { val = start + val / double(framerate); });
  const nc::NdArray<double> ys = evaluate(time_values);
  return { ys, time_values, framerate };
}

void Signal::plot(int framerate) const
{
  const double duration = period() * 3;
  const Wave wave = makeWave(duration, 0.0, framerate);
  wave.plot();
}

double Signal::period() const { return DEF_PERIOD; }// NOLINT


Sinusoid::Sinusoid(double freq, double amp, double offset, std::function<double(double)> func)// NOLINT
  : m_freq(freq), m_amp(amp), m_offset(offset), m_func(std::move(func))
{}

Sinusoid::Sinusoid(const Sinusoid &other)
  : m_freq(other.m_freq), m_amp(other.m_amp), m_offset(other.m_offset), m_func(other.m_func)
{}

std::unique_ptr<Signal> Sinusoid::clone() const { return std::make_unique<Sinusoid>(*this); }

nc::NdArray<double> Sinusoid::evaluate(const nc::NdArray<double> &ts) const// NOLINT
{
  std::vector<double> phases(ts.size());
  for (size_t i = 0; i < ts.size(); ++i) {
    phases[i] = double(nc::constants::twoPi) * m_freq * ts[int(i)] + m_offset;// NOLINT
  }
  std::vector<double> ys(phases.size());
  for (size_t i = 0; i < phases.size(); ++i) { ys[i] = m_amp * m_func(phases[i]); }
  return static_cast<nc::NdArray<double>>(ys);
}

double Sinusoid::period() const { return 1.0 / m_freq; }

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

nc::NdArray<double> SumSignal::evaluate(const nc::NdArray<double> &ts) const// NOLINT
{
  if (m_signals.empty()) { return {}; }

  nc::NdArray<double> sum = m_signals[0]->evaluate(ts);

  for (size_t i = 1; i < m_signals.size(); ++i) {
    if (m_signals[i]) { sum += m_signals[i]->evaluate(ts); }
  }

  return sum;
}

double SumSignal::period() const
{
  if (m_signals.empty()) {
    return DEF_PERIOD;// NOLINT
  }

  double maxp = 0.0;
  for (const auto &sig_ptr : m_signals) {
    if (sig_ptr) { maxp = std::max(maxp, sig_ptr->period()); }
  }

  return maxp > 0 ? maxp : DEF_PERIOD;// NOLINT
}

Chirp::Chirp(double start, double end, double amp) : m_start(start), m_end(end), m_amp(amp)// NOLINT
{}

nc::NdArray<double> Chirp::evaluate(const nc::NdArray<double> &ts) const// NOLINT
{
  auto interpolate = [](const nc::NdArray<double> &ts, double f0, double f1) {// NOLINT
    double t0 = ts.front();// NOLINT
    double t1 = ts.back();// NOLINT
    return (f0 + (f1 - f0)) * (ts - t0) / (t1 - t0);
  };

  // compute the frequencies
  nc::NdArray<double> freqs = interpolate(ts, m_start, m_end);

  // compute the time intervals
  nc::NdArray<double> dts = nc::diff(ts);// append ts.back()

  // compute the changes in phase
  nc::NdArray<double> dphis1 = nc::constants::twoPi * freqs * dts; 
  nc::NdArray<double> dphis2 = nc::roll(dphis1, 1);

  // compute phase
  nc::NdArray<double> phases = nc::cumsum(dphis2);

  // compute the amplitudes
  nc::NdArray<double> ys = m_amp * nc::cos(phases);
  return ys;
}

double Chirp::period() const { throw std::runtime_error("Non-periodic signal."); }

std::unique_ptr<Sinusoid> cosSignal(double freq, double amp, double offset)
{
  return std::make_unique<Sinusoid>(freq, amp, offset, static_cast<double (*)(double)>(nc::cos));
}

std::unique_ptr<Sinusoid> sinSignal(double freq, double amp, double offset)
{
  return std::make_unique<Sinusoid>(freq, amp, offset, static_cast<double (*)(double)>(nc::sin));
}

std::unique_ptr<Signal> operator+(const Signal &sig1, const Signal &sig2)
{
  return std::make_unique<SumSignal>(sig1.clone(), sig2.clone());
}

}// namespace asf
