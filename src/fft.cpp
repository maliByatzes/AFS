#include <asfproject/fft.h>
#include <fftw3.h>
#include <vector>

namespace asf {

VecComplexDoub FFT::convertToFrequencyDomain(std::vector<double> &samples)
{
  // NOLINTBEGIN
  size_t N = samples.size();
  fftw_complex *output;
  fftw_plan plan;
  VecComplexDoub ans(N);

  output = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * N));
  plan = fftw_plan_dft_r2c_1d(int(N), samples.data(), output, FFTW_ESTIMATE);

  fftw_execute(plan);

  fftw_destroy_plan(plan);

  for (size_t i = 0; i < N; ++i) {
    ComplexDoub value(output[i][0], output[i][1]);
    ans[i] = value;
  }

  fftw_free(output);

  return ans;
  // NOLINTEND
}

std::vector<double> FFT::convertToTimeDomain(VecComplexDoub &real_data)
{
  // NOLINTBEGIN
  size_t N = real_data.size();
  fftw_complex *input;
  std::vector<double> output(N);
  fftw_plan plan;

  input = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * N));

  for (size_t i = 0; i < N; ++i) {
    input[i][0] = real_data[i].real();
    input[i][1] = real_data[i].imag();
  }
  
  plan = fftw_plan_dft_c2r_1d(int(N), input, output.data(), FFTW_ESTIMATE);

  fftw_execute(plan);
  fftw_destroy_plan(plan);
  fftw_free(input);

  return output;
  // NOLINTEND
}

}// namespace asf
