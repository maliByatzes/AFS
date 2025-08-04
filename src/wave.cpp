#include <NumCpp/Functions/real.hpp>
#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <asfproject/wave.h>
#include <map>
#include <string>
#include <vector>
//#include <matplot/matplot.h>
#include <matplot/freestanding/plot.h>

namespace asf {

Wave::Wave(const nc::NdArray<float> &ys, const nc::NdArray<float> &ts, int framerate)// NOLINT
  : m_ys(ys), m_ts(ts), m_framerate(framerate)
{}

Wave::Wave(const nc::NdArray<std::complex<float>> &ys, const nc::NdArray<float> &ts, int framerate)//NOLINT
  : m_ys(nc::real(ys)), m_ts(ts), m_framerate(framerate)
{}

float Wave::getXFactor(std::map<std::string, float> &options) {
  float xfactor = 1.0;
  auto it = options.find("xfactor");//NOLINT
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

}// namespace asf
