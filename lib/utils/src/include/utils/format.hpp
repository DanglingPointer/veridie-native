#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <cassert>
#include <charconv>
#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <tuple>

namespace fmt {
namespace internal {

template <typename T>
concept Arithmetic =
   std::is_arithmetic_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;

template <Arithmetic T>
std::span<char> WriteAsText(T arg, std::span<char> dest)
{
   assert(!dest.empty());
   const auto [last, _] = std::to_chars(&dest[0], &dest[dest.size()], arg);
   return dest.subspan(static_cast<size_t>(std::distance(dest.data(), last)));
}
std::span<char> WriteAsText(char arg, std::span<char> dest);
std::span<char> WriteAsText(std::string_view arg, std::span<char> dest);
std::span<char> WriteAsText(void * arg, std::span<char> dest);
std::span<char> WriteAsText(bool arg, std::span<char> dest);
std::span<char> WriteAsText(const char * arg, std::span<char> dest);
std::span<char> WriteAsText(const std::string & arg, std::span<char> dest);

// clang-format off
template <typename T>
concept Writable = requires(T && arg, std::span<char> buf) {
   { WriteAsText(std::forward<T>(arg), buf) } -> std::same_as<std::span<char>>;
};
// clang-format on

size_t ParsePlaceholder(std::string_view from);
size_t CountPlaceholders(std::string_view fmt);
std::tuple<std::string_view, std::span<char>> CopyUntilPlaceholder(std::string_view src,
                                                                   std::span<char> dest);

} // namespace internal

template <typename T>
concept Formattable = internal::Writable<T>;

template <Formattable... Ts>
std::span<char> Format(std::span<char> buffer, std::string_view fmt, Ts &&... args)
{
   using namespace internal;
   assert(CountPlaceholders(fmt) == sizeof...(Ts));
   auto ProcessArg = [&](auto && arg) {
      std::tie(fmt, buffer) = CopyUntilPlaceholder(fmt, buffer);
      if (!buffer.empty())
         buffer = WriteAsText(std::forward<decltype(arg)>(arg), buffer);
      return !buffer.empty();
   };
   (... && ProcessArg(std::forward<Ts>(args)));
   std::tie(fmt, buffer) = CopyUntilPlaceholder(fmt, buffer);
   return buffer;
}

} // namespace fmt

#endif
