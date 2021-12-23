#include "utils/format.hpp"

namespace fmt::internal {

std::span<char> WriteAsText(char arg, std::span<char> dest)
{
   assert(!dest.empty());
   dest[0] = arg;
   return dest.subspan(1);
}

std::span<char> WriteAsText(std::string_view arg, std::span<char> dest)
{
   assert(!dest.empty());
   const size_t length = std::min(arg.size(), dest.size());
   std::copy_n(arg.cbegin(), length, dest.begin());
   return dest.subspan(length);
}

std::span<char> WriteAsText(void * arg, std::span<char> dest)
{
   assert(!dest.empty());
   static constexpr std::string_view prefix = "0x";
   dest = WriteAsText(prefix, dest);
   const auto [last, _] =
      std::to_chars(&dest[0], &dest[dest.size()], reinterpret_cast<uintptr_t>(arg), 16);
   return dest.subspan(static_cast<size_t>(std::distance(dest.data(), last)));
}

std::span<char> WriteAsText(bool arg, std::span<char> dest)
{
   assert(!dest.empty());
   static constexpr std::string_view t = "true";
   static constexpr std::string_view f = "false";
   return arg ? WriteAsText(t, dest) : WriteAsText(f, dest);
}

std::span<char> WriteAsText(const char * arg, std::span<char> dest)
{
   assert(!dest.empty());
   return WriteAsText(std::string_view(arg), dest);
}

std::span<char> WriteAsText(const std::string & arg, std::span<char> dest)
{
   assert(!dest.empty());
   return WriteAsText(std::string_view(arg), dest);
}


size_t ParsePlaceholder(std::string_view from)
{
   static constexpr std::string_view placeholder = "{}";
   return from.starts_with(placeholder) ? placeholder.size() : 0;
}

size_t CountPlaceholders(std::string_view fmt)
{
   size_t count = 0;
   for (size_t i = 0; i < fmt.size(); ++i)
      count += ParsePlaceholder(fmt.substr(i)) ? 1u : 0u;
   return count;
}

std::tuple<std::string_view, std::span<char>> CopyUntilPlaceholder(std::string_view src,
                                                                   std::span<char> dest)
{
   size_t srcPos = 0;
   size_t destPos = 0;
   while (srcPos < src.size() && destPos < dest.size()) {
      const size_t placeholderLength = ParsePlaceholder(src.substr(srcPos));
      if (placeholderLength > 0) {
         srcPos += placeholderLength;
         break;
      }
      dest[destPos++] = src[srcPos++];
   }
   return {src.substr(srcPos), dest.subspan(destPos)};
}

} // namespace fmt::internal
