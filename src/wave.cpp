#include <NumCpp/Functions/real.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <asfproject/wave.h>
#include <cmath>
#include <cstddef>
#include <map>
#include <matplot/freestanding/plot.h>
#include <string>
#include <vector>
#include <optional>

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

float Wave::start() const { return m_ts.front(); }

float Wave::end() const { return m_ts.back(); }

nc::NdArray<float> Wave::getYs() const { return m_ys; }

nc::NdArray<float> Wave::getTs() const { return m_ts; }

int Wave::getFramerate() const { return m_framerate; }

}// namespace asf
