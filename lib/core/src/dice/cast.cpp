#include "dice/cast.hpp"

#include "utils/format.hpp"

namespace dice {

std::span<char> WriteAsText(const Cast & cast, std::span<char> dest)
{
   cast.Apply([&](const auto & vec) {
      for (const auto & e : vec)
         dest = fmt::Format(dest, "{};", static_cast<uint32_t>(e));
   });
   return dest;
}

} // namespace dice
