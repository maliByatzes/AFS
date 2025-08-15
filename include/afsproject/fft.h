#ifndef fft_h_
#define fft_h_

#include <complex>
#include <vector>

namespace afs {

using ComplexDoub = std::complex<double>;
using VecComplexDoub = std::vector<ComplexDoub>;

class FFT
{
public:
  FFT() = default;

  static VecComplexDoub convertToFrequencyDomain(const std::vector<double> &);
  static std::vector<double> convertToTimeDomain(VecComplexDoub &, size_t);
};

}// namespace afs

#endif
