#include <asfproject/fft.h>
#include <asfproject/wave.h>
#include <NumCpp/Core/Constants.hpp>
#include <NumCpp/Functions/abs.hpp>
#include <NumCpp/Functions/angle.hpp>
#include <NumCpp/Functions/imag.hpp>
#include <NumCpp/Functions/max.hpp>
#include <NumCpp/Functions/power.hpp>
#include <NumCpp/Functions/real.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/spectrum.h>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <matplot/freestanding/plot.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace asf {

long findIndex(double value, const nc::NdArray<double> &arr)
{
  auto it = std::ranges::lower_bound(arr, value);// NOLINT
  if (it == arr.end()) { return arr.size() - 1; }
  return std::distance(arr.begin(), it);
}

// NOLINTBEGIN
SpectrumParent::SpectrumParent(const nc::NdArray<std::complex<double>> &hs,// NOLINT
  const nc::NdArray<double> &fs,// NOLINT
  int framerate,
  size_t orig_n,
  bool full)
  : m_hs(hs), m_fs(fs), m_framerate(framerate), m_orig_n(orig_n), m_full(full)
{}
// NOLINTEND

std::unique_ptr<SpectrumParent> SpectrumParent::clone() const { return std::make_unique<SpectrumParent>(*this); }

double SpectrumParent::maxFreq() const {
  return static_cast<double>(m_framerate) / 2.0;// NOLINT
}

nc::NdArray<double> SpectrumParent::amps() const { return nc::abs(m_hs); }

nc::NdArray<double> SpectrumParent::power() const { return amps() * amps(); }

double SpectrumParent::maxDiff(const SpectrumParent &other) const
{
  if (m_framerate != other.m_framerate || m_hs.size() != other.m_hs.size()) {
    throw std::invalid_argument("Spectrums must the have same framerate and size.");
  }
  return nc::max(nc::abs(m_hs - other.m_hs)).item();
}

SpectrumParent SpectrumParent::ratio(const SpectrumParent &denom, double thresh, double val) const// NOLINT
{
  if (m_framerate != denom.m_framerate || m_hs.size() != denom.m_hs.size()) {
    throw std::invalid_argument("Spectrums must have the same framerate and size.");
  }

  SpectrumParent new_spec = *this;
  new_spec.m_hs /= denom.m_hs;

  const nc::NdArray<bool> mask = denom.amps() < thresh;
  new_spec.m_hs[mask].fill(static_cast<std::complex<double>>(val));

  return new_spec;
}

SpectrumParent SpectrumParent::invert() const
{
  SpectrumParent new_spec = *this;
  new_spec.m_hs = 1.0 / new_spec.m_hs;
  return new_spec;
}

double SpectrumParent::freqRes() const
{
  return static_cast<double>(m_framerate) / 2.0 / (static_cast<double>(m_fs.size()) - 1.0);// NOLINT
}

std::vector<std::pair<double, double>> SpectrumParent::peaks() const
{
  std::vector<std::pair<double, double>> pairs;
  pairs.reserve(m_hs.size());
  for (int i = 0; i < int(m_hs.size()); ++i) { pairs.push_back({ nc::abs(m_hs[i]), m_fs[i] }); }// NOLINT

  std::ranges::sort(pairs, [](const auto &a, const auto &b) {// NOLINT
    return a.first > b.first;
  });

  return pairs;
}

std::pair<nc::NdArray<double>, nc::NdArray<double>> SpectrumParent::renderFull(std::optional<double> high) const
{
  if (!m_full) { throw std::runtime_error("renderFull should only be called on a full spectrum."); }

  nc::NdArray<std::complex<double>> hs = fftShift(m_hs);// NOLINT
  nc::NdArray<double> amps = nc::abs(hs);
  nc::NdArray<double> fs = fftShift(m_fs);// NOLINT

  long i = (high < 0) ? 0 : findIndex(-high.value(), fs);// NOLINT
  long j = (high < 0) ? fs.size() : findIndex(high.value(), fs) + 1;// NOLINT

  const nc::NdArray<double> fs_sliced(fs.begin() + i, fs.begin() + j);
  const nc::NdArray<double> amps_sliced(amps.begin() + i, amps.begin() + j);
  return { fs_sliced, amps_sliced };
}

void SpectrumParent::plot(std::optional<double> high)
{
  if (m_full) {
    auto [fs_full, amps_full] = renderFull(high);
    const std::vector<double> fs_vec = fs_full.toStlVector();
    const std::vector<double> amps_vec = amps_full.toStlVector();

    matplot::plot(fs_vec, amps_vec, "--");
  } else {
    long i = (high < 0) ? -1 : findIndex(high.value(), m_fs);// NOLINT

    const nc::NdArray<double> fs_slice(m_fs.begin(), m_fs.begin() + i);
    const nc::NdArray<double> amps_slice(amps().begin(), amps().begin() + i);

    matplot::plot(fs_slice.toStlVector(), amps_slice.toStlVector());
  }

  matplot::show();
}

void SpectrumParent::plotPower(std::optional<double> high)
{
  if (m_full) {
    auto [fs_full, amps_full] = renderFull(high);

    const nc::NdArray<double> power_full = amps_full * amps_full;

    matplot::plot(fs_full.toStlVector(), power_full.toStlVector());
  } else {
    long i = (high < 0) ? -1 : findIndex(high.value(), m_fs);// NOLINT

    const nc::NdArray<double> fs_slice(m_fs.begin(), m_fs.begin() + i);
    const nc::NdArray<double> power_slice(power().begin(), power().begin() + i);

    matplot::plot(fs_slice.toStlVector(), power_slice.toStlVector(), "--");
  }

  matplot::show();
}

// NOLINTBEGIN
Spectrum::Spectrum(const nc::NdArray<std::complex<double>> &hs,// NOLINT
  const nc::NdArray<double> &fs,// NOLINT
  int framerate,
  size_t orig_n,
  bool full)
  : SpectrumParent(hs, fs, framerate, orig_n, full)
{}
// NOLINTEND

size_t Spectrum::size() const { return m_hs.size(); }

Spectrum Spectrum::operator+(const Spectrum &other) const
{
  if (m_framerate != other.m_framerate || m_hs.size() != other.m_hs.size()) {
    throw std::invalid_argument("Spectrums must have the same framerate and size.");
  }

  nc::NdArray<std::complex<double>> hs = m_hs + other.m_hs;// NOLINT
  return { hs, m_fs, m_framerate, m_orig_n, m_full };
}

Spectrum Spectrum::operator+(int val) const
{
  if (val != 0) { throw std::invalid_argument("Can only add 0 to a Spectrum."); }
  return *this;
}

Spectrum Spectrum::operator*(const Spectrum &other) const
{
  if (m_framerate != other.m_framerate || m_hs.size() != other.m_hs.size()) {
    throw std::invalid_argument("Spectrums must have the same framerate and size.");
  }

  nc::NdArray<std::complex<double>> hs = m_hs * other.m_hs;// NOLINT
  return { hs, m_fs, m_framerate, m_orig_n, m_full };
}


// Spectrum Spectrum::convolve(const Spectrum &other) const {}

nc::NdArray<double> Spectrum::real() const { return nc::real(m_hs); }

nc::NdArray<double> Spectrum::imag() const { return nc::imag(m_hs); }

nc::NdArray<double> Spectrum::angles() const { return nc::angle(m_hs); }

void Spectrum::scale(const std::complex<double> &factor) { m_hs *= factor; }

void Spectrum::lowPass(double cutoff, double factor)// NOLINT
{
  const nc::NdArray<bool> mask = nc::abs(m_fs) > cutoff;
  m_hs[mask] = m_hs[mask] * static_cast<std::complex<double>>(factor);
}

void Spectrum::highPass(double cutoff, double factor)// NOLINT
{
  const nc::NdArray<bool> mask = nc::abs(m_fs) < cutoff;
  m_hs[mask] = m_hs[mask] * static_cast<std::complex<double>>(factor);
}

void Spectrum::bandStop(double low_cutoff, double high_cutoff, double factor)// NOLINT
{
  const nc::NdArray<double> fs_abs = nc::abs(m_fs);
  const nc::NdArray<bool> mask = (fs_abs > low_cutoff) & (fs_abs < high_cutoff);
  m_hs[mask] = m_hs[mask] * static_cast<std::complex<double>>(factor);
}

void Spectrum::pinkFilter(double beta)
{
  nc::NdArray<double> denom = nc::power(m_fs, uint8_t(beta / 2.0));// NOLINT
  if (denom.size() > 0 && denom[0] == 0.0) { denom[0] = 1.0; }
  m_hs /= denom;
}

Spectrum Spectrum::differentiate() const
{
  Spectrum new_spec = *this;
  const std::complex<double> imag_unit(0.0, 1.0);
  new_spec.m_hs *= 2.0 * nc::constants::pi * imag_unit * new_spec.m_fs;// NOLINT
  return new_spec;
}

Spectrum Spectrum::integrate() const
{
  const Spectrum new_spec = *this;
  const std::complex<double> imag_unit(0.0, 1.0);

  const nc::NdArray<bool> zero_mask = new_spec.m_fs == 0.0;

  nc::NdArray<std::complex<double>> non_zero_hs = new_spec.m_hs[~zero_mask];
  const nc::NdArray<double> non_zero_fs = new_spec.m_fs[~zero_mask];
  non_zero_hs /= (2.0 * nc::constants::pi * imag_unit * non_zero_fs);// NOLINT

  new_spec.m_hs[~zero_mask] = non_zero_hs;
  new_spec.m_hs[zero_mask].fill(std::numeric_limits<double>::infinity());

  return new_spec;
}

Wave Spectrum::makeWave() const
{
  VecComplexDoub hs_vec = m_hs.toStlVector();
  std::vector<double> ys_vec = FFT::convertToTimeDomain(hs_vec, m_orig_n);
  nc::NdArray<double> ys = nc::NdArray<double>(1, uint32_t(ys_vec.size()));
  std::ranges::copy(ys_vec, ys.begin());
  return { ys, m_framerate };
}

template<typename T> nc::NdArray<T> fftShift(const nc::NdArray<T> &arr)
{
  long n = arr.size();// NOLINT
  if (n <= 1) { return arr; }

  long k = (n + 1) / 2;// NOLINT
  nc::NdArray<T> shifted_arr(1, uint32_t(n));

  std::copy(arr.begin() + k, arr.end(), shifted_arr.begin());
  std::copy(arr.begin(), arr.begin() + k, shifted_arr.begin() + n - k);

  return shifted_arr;
}

template<typename T> nc::NdArray<T> ifftShift(const nc::NdArray<T> &arr)
{
  long n = arr.size();// NOLINT
  if (n <= 1) { return arr; }

  long k = n / 2;// NOLINT
  nc::NdArray<T> shifted_arr(1, uint32_t(n));

  std::copy(arr.begin() + k, arr.end(), shifted_arr.begin());
  std::copy(arr.begin(), arr.begin() + k, shifted_arr.begin() + n - k);

  return shifted_arr;
}

}// namespace asf
