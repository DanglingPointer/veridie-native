#ifndef CORE_LOG_HPP
#define CORE_LOG_HPP

#include "utils/format.hpp"

#include <array>
#include <concepts>
#include <string_view>

namespace internal {

template <typename... Ts>
concept NonEmpty = sizeof...(Ts) > 0;

} // namespace internal

struct Log final
{
   static void Debug(const char * tag, const char * text);
   static void Info(const char * tag, const char * text);
   static void Warning(const char * tag, const char * text);
   static void Error(const char * tag, const char * text);
   [[noreturn]] static void Fatal(const char * tag, const char * text);


   // clang-format off
   template <typename... Ts> requires internal::NonEmpty<Ts...>
   static void Debug(const char * tag, std::string_view fmt, Ts &&... args)
   {
      auto formatted = FormatArgs(fmt, std::forward<Ts>(args)...);
      Debug(tag, std::data(formatted));
   }
   template <typename... Ts> requires internal::NonEmpty<Ts...>
   static void Info(const char * tag, std::string_view fmt, Ts &&... args)
   {
      auto formatted = FormatArgs(fmt, std::forward<Ts>(args)...);
      Info(tag, std::data(formatted));
   }
   template <typename... Ts> requires internal::NonEmpty<Ts...>
   static void Warning(const char * tag, std::string_view fmt, Ts &&... args)
   {
      auto formatted = FormatArgs(fmt, std::forward<Ts>(args)...);
      Warning(tag, std::data(formatted));
   }
   template <typename... Ts> requires internal::NonEmpty<Ts...>
   static void Error(const char * tag, std::string_view fmt, Ts &&... args)
   {
      auto formatted = FormatArgs(fmt, std::forward<Ts>(args)...);
      Error(tag, std::data(formatted));
   }
   template <typename... Ts> requires internal::NonEmpty<Ts...>
   [[noreturn]] static void Fatal(const char * tag, std::string_view fmt, Ts &&... args)
   {
      auto formatted = FormatArgs(fmt, std::forward<Ts>(args)...);
      Fatal(tag, std::data(formatted));
   }
   // clang-format on

   using Handler = void (*)(const char *, const char *);
   static Handler s_debugHandler;
   static Handler s_infoHandler;
   static Handler s_warningHandler;
   static Handler s_errorHandler;
   static Handler s_fatalHandler;

   static constexpr size_t MAX_LINE_LENGTH = 511;

private:
   static_assert((MAX_LINE_LENGTH + 1) % 64 == 0);

   template <typename... Ts>
   static auto FormatArgs(std::string_view fmt, Ts &&... args)
   {
      std::array<char, MAX_LINE_LENGTH + 1> buffer;
      auto rest = fmt::Format({buffer.data(), MAX_LINE_LENGTH}, fmt, std::forward<Ts>(args)...);
      rest[0] = '\0';
      return buffer;
   }
};

#endif
