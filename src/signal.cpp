#include <asfproject/signal.h>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace asf {

Wave Signal::makeWave(float duration, float start, int framerate) const
{
  int size = int(std::round(duration * float(framerate)));
}

void Signal::plot(int framerate) {}

float Signal::period() const {}

template<typename T> std::vector<T> arange(T stop)
{
  if (stop <= 0) { throw std::invalid_argument("stop value must be greater than zero."); }

  return arange<T>(0, stop, 1);
}

template<typename T> std::vector<T> arange(T start, T stop, T step)// NOLINT
{
  static_assert(std::is_arithmetic_v<T>, "Can only be used with arithmetic types.");

  if (step > 0 && stop < start) {
    throw std::invalid_argument("stop value must be larger than the start value for a positive step.");
  }

  if (step < 0 && stop > start) {
    throw std::invalid_argument("start value must be larger than the stop value for a negative step.");
  }

  std::vector<T> values;
  T value = start;
  auto counter = T{ 1 };

  if (step > 0) {
    while (value < stop) {
      values.push_back(value);
      value = start + step * counter++;
    }
  } else {
    while (value > stop) {
      values.push_back(value);
      value = start + step * counter++;
    }
  }

  return values;
}

}// namespace asf
