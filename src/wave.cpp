#include <NumCpp/Core/Enums.hpp>
#include <NumCpp/Functions/concatenate.hpp>
#include <NumCpp/Functions/flip.hpp>
#include <NumCpp/Functions/linspace.hpp>
#include <NumCpp/Functions/ones.hpp>
#include <NumCpp/Functions/real.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <algorithm>
#include <asfproject/wave.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <matplot/freestanding/plot.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace asf {

Wave::Wave(const nc::NdArray<float> &ys, const nc::NdArray<float> &ts, int framerate)// NOLINT
  : m_ys(ys), m_ts(ts), m_framerate(framerate)
{}

Wave::Wave(const nc::NdArray<std::complex<float>> &ys, const nc::NdArray<float> &ts, int framerate)// NOLINT
  : m_ys(nc::real(ys)), m_ts(ts), m_framerate(framerate)
{}

float Wave::getXFactor(std::map<std::string, float> &options)
{
  float xfactor = 1.0;
  auto it = options.find("xfactor");// NOLINT
  if (it != options.end()) {
    xfactor = it->second;
    options.erase(it);
  }
  return xfactor;
}

void Wave::plot(std::map<std::string, float> options) const
{
  const float xfactor = getXFactor(options);
  nc::NdArray<float> ts_scaled = xfactor * m_ts;

  const std::vector<float> ts_vec(ts_scaled.begin(), ts_scaled.end());
  const std::vector<float> ys_vec(m_ys.begin(), m_ys.end());

  matplot::plot(ts_vec, ys_vec, "--");
  matplot::show();
  // NOTE: Ignore the options for now...
}

int Wave::findIndex(float time) const
{
  const size_t len = m_ts.size();
  const float start = this->start();
  const float end = this->end();
  auto idx = std::round(float(len - 1) * (time - start) / (end - start));
  return int(idx);
}

Wave Wave::segment(std::optional<float> start, std::optional<float> duration) const// NOLINT
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
  std::vector<float> ys(m_ys.begin() + i, m_ys.begin() + j);
  std::vector<float> ts(m_ts.begin() + i, m_ts.begin() + j);// NOLINT
  return { ys, ts, m_framerate };
}

void Wave::apodize(float denom, float duration)// NOLINT
{
  m_ys = ::asf::apodize(m_ys, m_framerate, denom, duration);
}

void Wave::normalize(float amp) { m_ys = ::asf::normalize(m_ys, amp); }

float Wave::start() const { return m_ts.front(); }

float Wave::end() const { return m_ts.back(); }

nc::NdArray<float> Wave::getYs() const { return m_ys; }

nc::NdArray<float> Wave::getTs() const { return m_ts; }

int Wave::getFramerate() const { return m_framerate; }

nc::NdArray<float> normalize(const nc::NdArray<float> &ys, float amp)
{
  const float high = std::abs(*std::ranges::max_element(ys));
  const float low = std::abs(*std::ranges::min_element(ys));
  return amp * ys / std::max(high, low);
}

nc::NdArray<float> apodize(const nc::NdArray<float> &ys, int framerate, float denom, float duration)// NOLINT
{
  const long len = ys.size();

  if (len < 2
              * std::min(
                static_cast<long>(float(len) / denom), static_cast<long>(std::round(duration * float(framerate))))) {
    throw std::invalid_argument("Signal is too short to be apodized with the given parameters.");
  }

  const long frac_seg = len / static_cast<long>(denom);
  const long time_seg = static_cast<long>(std::round(duration * float(framerate)));
  const long min_seg = std::min(frac_seg, time_seg);

  if (min_seg == 0) { return ys; }

  const nc::NdArray<float> arr1 = nc::linspace<float>(0.0, 1.0, uint32_t(min_seg));
  const nc::NdArray<float> arr2 = nc::ones<float>(uint32_t(float(len) - 2.0F * float(min_seg)));// NOLINT
  const nc::NdArray<float> tmp_arr3 = nc::linspace<float>(0.0, 1.0, uint32_t(min_seg));
  const nc::NdArray<float> arr3 = nc::flip(tmp_arr3, nc::Axis::ROW);

  const std::vector<nc::NdArray<float>> win_components = { arr1, arr2, arr3 };
  const nc::NdArray<float> window = nc::concatenate(win_components);

  return ys * window;
}

}// namespace asf
