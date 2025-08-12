#include <NumCpp/Functions/hamming.hpp>
#include <asfproject/fft.h>
#include <asfproject/signal.h>
#include <asfproject/wave_file.h>
#include <asfproject/spectrum.h>
#include <asfproject/spectrogram.h>
#include <NumCpp/Core/Enums.hpp>
#include <NumCpp/Functions/abs.hpp>
#include <NumCpp/Functions/arange.hpp>
#include <NumCpp/Functions/concatenate.hpp>
#include <NumCpp/Functions/flip.hpp>
#include <NumCpp/Functions/linspace.hpp>
#include <NumCpp/Functions/max.hpp>
#include <NumCpp/Functions/ones.hpp>
#include <NumCpp/Functions/real.hpp>
#include <NumCpp/Functions/round.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/wave.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <matplot/freestanding/axes_functions.h>
#include <matplot/freestanding/plot.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace asf {

Wave::Wave(const nc::NdArray<double> &ys) : m_ys(ys), m_framerate(DEF_FRAMERATE)
{
  m_ts = nc::arange<double>(ys.size()) / double(m_framerate);
}

Wave::Wave(const nc::NdArray<double> &ys, int framerate) : m_ys(ys), m_framerate(framerate)
{
  m_ts = nc::arange<double>(ys.size()) / double(m_framerate);
}

Wave::Wave(const nc::NdArray<double> &ys, const nc::NdArray<double> &ts, int framerate)// NOLINT
  : m_ys(ys), m_ts(ts), m_framerate(framerate)
{}

Wave::Wave(const nc::NdArray<std::complex<double>> &ys, const nc::NdArray<double> &ts, int framerate)// NOLINT
  : m_ys(nc::real(ys)), m_ts(ts), m_framerate(framerate)
{}

double Wave::getXFactor(std::map<std::string, double> &options)
{
  double xfactor = 1.0;
  auto it = options.find("xfactor");// NOLINT
  if (it != options.end()) {
    xfactor = it->second;
    options.erase(it);
  }
  return xfactor;
}

void Wave::hamming()
{
  m_ys *= nc::hamming(int32_t(m_ys.size()));
}

void Wave::window(nc::NdArray<double> &window)
{
  m_ys *= window;  
}

Spectrogram Wave::makeSpectrogram(int seg_length, bool win_flag)
{
  nc::NdArray<double> window;
  
  if (win_flag) {
   window = nc::hamming(seg_length);
  }

  int i = 0;// NOLINT
  int j = seg_length;// NOLINT
  const int step = seg_length / 2;

  // map time from Spectrum
  std::map<double, Spectrum> spec_map = {};

  while (j < int(m_ys.size())) {// NOLINT
    Wave segment = this->slice(i, j);
    if (win_flag) {
      segment.window(window);
    }

    // the nominal time for this segment is the midpoint
    const double midpoint = (segment.start() + segment.end()) / 2;
    spec_map.at(midpoint) = segment.makeSpectrum();

    i += step;
    j += step;
  }

  return { spec_map, seg_length };
}

void Wave::plot(std::map<std::string, double> options) const
{
  const double xfactor = getXFactor(options);
  nc::NdArray<double> ts_scaled = xfactor * m_ts;

  const std::vector<double> ts_vec(ts_scaled.begin(), ts_scaled.end());
  const std::vector<double> ys_vec(m_ys.begin(), m_ys.end());

  matplot::plot(ts_vec, ys_vec);
  matplot::xlabel("Time (s)");
  matplot::show();
  // NOTE: Ignore the options for now...
}

int Wave::findIndex(double time) const
{
  const size_t len = m_ts.size();
  const double start = this->start();
  const double end = this->end();
  auto idx = std::round(double(len - 1) * (time - start) / (end - start));
  return int(idx);
}

Wave Wave::segment(std::optional<double> start, std::optional<double> duration) const// NOLINT
{
  int idx = -1;

  if (!start.has_value()) {
    start = m_ts[0];
    idx = -1;
  } else {
    idx = this->findIndex(start.value());
  }

  int j;// NOLINT
  if (!duration.has_value()) {
    j = -1;
  } else {
    j = this->findIndex(start.value() + duration.value());
  }

  return this->slice(idx, j);
}

Wave Wave::slice(int i, int j) const// NOLINT
{
  std::vector<double> ys(m_ys.begin() + i, m_ys.begin() + j);
  std::vector<double> ts(m_ts.begin() + i, m_ts.begin() + j);// NOLINT
  return { ys, ts, m_framerate };
}

void Wave::apodize(double denom, double duration)// NOLINT
{
  m_ys = ::asf::apodize(m_ys, m_framerate, denom, duration);
}

void Wave::normalize(double amp) { m_ys = ::asf::normalize(m_ys, amp); }

template<typename dtype> nc::NdArray<double> Wave::quantize(double bound) const
{
  return ::asf::quantize<dtype>(m_ys, bound);
}

void Wave::write(const std::string &filename) const
{
  std::cout << "Writing " << filename << "\n";
  WaveFile wfile;
  wfile.setPCMData(m_ys.toStlVector(), 44100, 1);// NOLINT
  if (!wfile.save(filename)) { throw std::runtime_error("Could not save the wav file."); }
}

double Wave::start() const { return m_ts.front(); }

double Wave::end() const { return m_ts.back(); }

nc::NdArray<double> Wave::getYs() const { return m_ys; }

nc::NdArray<double> Wave::getTs() const { return m_ts; }

int Wave::getFramerate() const { return m_framerate; }

nc::NdArray<double> normalize(const nc::NdArray<double> &ys, double amp)
{
  const double high = std::abs(*std::ranges::max_element(ys));
  const double low = std::abs(*std::ranges::min_element(ys));
  return amp * ys / std::max(high, low);
}

nc::NdArray<double> apodize(const nc::NdArray<double> &ys, int framerate, double denom, double duration)// NOLINT
{
  const long len = ys.size();

  if (len < 2
              * std::min(
                static_cast<long>(double(len) / denom), static_cast<long>(std::round(duration * double(framerate))))) {
    throw std::invalid_argument("Signal is too short to be apodized with the given parameters.");
  }

  const long frac_seg = len / static_cast<long>(denom);
  const long time_seg = static_cast<long>(std::round(duration * double(framerate)));
  const long min_seg = std::min(frac_seg, time_seg);

  if (min_seg == 0) { return ys; }

  const nc::NdArray<double> arr1 = nc::linspace<double>(0.0, 1.0, uint32_t(min_seg));
  const nc::NdArray<double> arr2 = nc::ones<double>(uint32_t(double(len) - 2.0 * double(min_seg)));// NOLINT
  const nc::NdArray<double> tmp_arr3 = nc::linspace<double>(0.0, 1.0, uint32_t(min_seg));
  const nc::NdArray<double> arr3 = nc::flip(tmp_arr3, nc::Axis::ROW);

  const std::vector<nc::NdArray<double>> win_components = { arr1, arr2, arr3 };
  const nc::NdArray<double> window = nc::concatenate(win_components);

  return ys * window;
}

template<typename dtype> nc::NdArray<dtype> quantize(const nc::NdArray<double> &ys, double bound)// NOLINT
{
  const double max_abs_val = nc::max(nc::abs(ys)).item();

  if (max_abs_val > 1.0) {
    std::cerr << "Warning: normalizing before quantizing.\n";
    const nc::NdArray<double> norm_ys = ::asf::normalize(ys);
    return nc::round(norm_ys * bound).astype<dtype>();
  }

  return nc::round(ys * bound).astype<dtype>();
}

Spectrum Wave::makeSpectrum()
{
  std::vector<double> ys_vec = m_ys.toStlVector();
  VecComplexDoub hs_vec = FFT::convertToFrequencyDomain(ys_vec);
  nc::NdArray<std::complex<double>> hs = nc::NdArray<std::complex<double>>(1, uint32_t(hs_vec.size()));// NOLINT
  std::ranges::copy(hs_vec, hs.begin());
  long n = m_ys.size();// NOLINT
  nc::NdArray<double> fs = nc::linspace<double>(0.0, static_cast<double>(m_framerate) / 2.0, hs.size());// NOLINT
  return { hs, fs, m_framerate, size_t(n) };
}

}// namespace asf
