#ifndef either_h_
#define either_h_

#include <type_traits>

template<class L>
struct Left
{
  L const value;  
};

template<class R>
struct Right
{
  R const value;  
};

template<class T>
constexpr Left<T> left(T const& value) {
  return {value};
}

template<class T>
constexpr Right<T> right(T const& value) {
  return {value};
}


template<class L, class R>
struct Either
{
  union {
    L mutable leftValue;
    R mutable rightValue;
  };
  bool const isLeft;

  // Constructors
  
  constexpr Either(Either<L, R> const& e)
    : isLeft{e.isLeft} {
      if (isLeft) {
        leftValue = e.leftValue;
      } else {
        rightValue = e.rightValue;
      }
    }

  constexpr Either(Left<L> const& left)
    : leftValue{left.value}
    , isLeft{true}
  {}

  constexpr Either(Right<R> const& right)
    : rightValue{right.value}
    , isLeft{false}
  {}

  // Destructor

  ~Either() {
    if (isLeft) {
      leftValue.~L();
    } else {
      rightValue.~R();
    }
  }

  template<class F>
  constexpr auto leftMap(F const& f) const
    -> Either<decltype(f(leftValue)), R> {
      if (isLeft) return { left(f(leftValue)) };
      return { right(rightValue) };
  }

  template<class F>
  constexpr auto rightMap(F const& f) const
    -> Either<L, decltype(f(rightValue))> {
      if (isLeft) return { left(leftValue) };
      return { right(f(rightValue)) };    
  }

  template<class LL=L, class RR=R>
  constexpr auto join() const
    -> typename std::common_type<LL, RR>
    ::type {
      if (isLeft) return leftValue;
      return rightValue;
    }
};

#endif
