#ifndef fft_h_
#define fft_h_

#include <complex>
#include <vector>

namespace asf {

using ComplexDoub = std::complex<double>;
using VecComplexDoub = std::vector<ComplexDoub>;

class FFT
{
public:
  FFT() = default;

  static VecComplexDoub convertToFrequencyDomain(std::vector<double> &);
  static std::vector<double> convertToTimeDomain(VecComplexDoub &, size_t);
};

}// namespace asf

#endif
