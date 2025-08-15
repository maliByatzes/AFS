#include <afsproject/fft.h>
#include <algorithm>
#include <fftw3.h>
#include <vector>

namespace afs {

VecComplexDoub FFT::convertToFrequencyDomain(const std::vector<double> &samples)
{
  // NOLINTBEGIN
  size_t N = samples.size();
  if (N == 0) { return {}; }

  size_t complex_output_size = N / 2 + 1;
  fftw_complex *output;
  fftw_plan plan;
  VecComplexDoub ans(complex_output_size);

  output = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * complex_output_size));

  std::vector<double> mut_samples = samples;
  plan = fftw_plan_dft_r2c_1d(int(N), mut_samples.data(), output, FFTW_ESTIMATE);

  fftw_execute(plan);

  fftw_destroy_plan(plan);

  for (size_t i = 0; i < complex_output_size; ++i) {
    ComplexDoub value(output[i][0], output[i][1]);
    ans[i] = value;
  }

  fftw_free(output);

  return ans;
  // NOLINTEND
}

std::vector<double> FFT::convertToTimeDomain(VecComplexDoub &real_data, size_t original_N)
{
  // NOLINTBEGIN
  size_t complex_input_size = real_data.size();
  if (complex_input_size == 0) { return {}; }

  fftw_complex *input;
  std::vector<double> output(original_N);
  fftw_plan plan;

  input = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * complex_input_size));

  for (size_t i = 0; i < complex_input_size; ++i) {
    input[i][0] = real_data[i].real();
    input[i][1] = real_data[i].imag();
  }

  plan = fftw_plan_dft_c2r_1d(int(original_N), input, output.data(), FFTW_ESTIMATE);

  fftw_execute(plan);
  fftw_destroy_plan(plan);
  fftw_free(input);

  std::ranges::for_each(output, [&](double &val) {
    val /= static_cast<double>(original_N);
  });

  return output;
  // NOLINTEND
}

}// namespace afs
