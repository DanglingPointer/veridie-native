#ifndef ALWAYS_COPYABLE_HPP
#define ALWAYS_COPYABLE_HPP

#include <utility>

template <typename F>
struct AlwaysCopyable : F
{
private:
   AlwaysCopyable(F & f)
      : F(std::move(f))
   {}
   AlwaysCopyable(AlwaysCopyable & c)
      : AlwaysCopyable(static_cast<F &>(c))
   {}

public:
   AlwaysCopyable(F && f)
      : F(std::move(f))
   {}
   AlwaysCopyable(AlwaysCopyable && cc)
      : AlwaysCopyable(static_cast<F &&>(cc))
   {}
   AlwaysCopyable(const AlwaysCopyable & c)
      : AlwaysCopyable(const_cast<AlwaysCopyable &>(c))
   {}
};

#endif // ALWAYS_COPYABLE_HPP
