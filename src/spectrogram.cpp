#include <NumCpp/NdArray/NdArrayCore.hpp>
#include <afsproject/spectrogram.h>
#include <afsproject/spectrum.h>
#include <cstddef>
#include <map>
#include <matplot/freestanding/axes_functions.h>
#include <matplot/freestanding/plot.h>
#include <optional>
#include <utility>
#include <vector>

namespace afs {

Spectrogram::Spectrogram(std::map<double, Spectrum> spec_map, int seg_length)
  : m_spec_map(std::move(spec_map)), m_seg_length(seg_length)
{}

const Spectrum &Spectrogram::anySpectrum() const { return m_spec_map.begin()->second; }

double Spectrogram::timeRes() const
{
  const Spectrum &spectrum = anySpectrum();
  return static_cast<double>(m_seg_length) / spectrum.getFramerate();
}

double Spectrogram::freqRes() const { return anySpectrum().freqRes(); }

std::vector<double> Spectrogram::times() const
{
  std::vector<double> ts;// NOLINT
  ts.reserve(m_spec_map.size());
  for (const auto &pair : m_spec_map) { ts.push_back(pair.first); }
  return ts;
}

nc::NdArray<double> Spectrogram::frequencies() const { return anySpectrum().getMfs(); }

void Spectrogram::plot(std::optional<double> high) const
{
  const nc::NdArray<double> fs_full = frequencies();// NOLINT
  long i = high.has_value() ? findIndex(high.value(), fs_full) : -1;// NOLINT

  const std::vector<double> fs(fs_full.begin(), fs_full.begin() + i);// NOLINT
  const std::vector<double> ts = times();// NOLINT

  // make the array
  std::vector<std::vector<double>> arr(fs.size(), std::vector<double>(ts.size()));

  size_t j = 0;// NOLINT
  for (const auto &pair : m_spec_map) {
    const Spectrum &spectrum = pair.second;
    const nc::NdArray<double> amps = spectrum.amps();
    const nc::NdArray<double> amps_slice(amps.begin(), amps.begin() + i);

    for (size_t k = 0; k < amps_slice.size(); ++k) { arr[k][j] = amps_slice[int(k)]; }
    j++;
  }

  // matplot::plot(ts_vec, fs_vec, arr);
  matplot::pcolor(arr);
  matplot::xlabel("Time (s)");
  matplot::ylabel("Frequency (Hz)");
  matplot::title("Spectrogram");
  matplot::colorbar();
  matplot::show();
}

}// namespace afs
