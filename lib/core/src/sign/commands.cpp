//#undef NDEBUG
#include "sign/commands.hpp"
#include <cassert>
#include <charconv>
#include <cstring>
#include <climits>

namespace {

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<std::decay_t<T>>>>
void WriteToBuffer(T i, char * from, char * to) noexcept
{
   char * first = from + 1;
   auto [last, _] = std::to_chars(first, to, i);
   assert(last <= to);
   *from = static_cast<unsigned char>(last - first);
}

void WriteToBuffer(std::string_view sv, char * from, char * to) noexcept
{
   assert(sv.size() < to - from);
   *from++ = static_cast<unsigned char>(sv.size());
   std::memcpy(from, sv.data(), sv.size());
   (void)to;
}

void WriteToBuffer(bool b, char * from, char * /*to*/) noexcept
{
   constexpr char t[] = "true";
   constexpr char f[] = "false";
   if (b) {
      *from++ = static_cast<unsigned char>(sizeof(t) - 1);
      std::memcpy(from, t, sizeof(t));
   } else {
      *from++ = static_cast<unsigned char>(sizeof(f) - 1);
      std::memcpy(from, f, sizeof(f));
   }
}

template <typename Rep, typename Period>
void WriteToBuffer(std::chrono::duration<Rep, Period> d, char * from, char * to) noexcept
{
   WriteToBuffer(d.count(), from, to);
}

void WriteToBuffer(const dice::Cast & cast, char * from, char * to)
{
   char * it = from + 1;
   std::visit(
      [&](const auto & vec) {
         for (const auto & e : vec) {
            auto [last, _] = std::to_chars(it, to, static_cast<uint32_t>(e));
            assert(last <= to);
            it = last;
            *it++ = ';';
         }
      },
      cast);
   *from = static_cast<unsigned char>(it - from - 1);
}

template <typename T, size_t Length, size_t Size, size_t... Is>
void FillCharArrays(T tuple,
                    std::array<std::array<char, Length>, Size> & arr,
                    std::index_sequence<Is...>)
{
   (..., WriteToBuffer(std::get<Is + 1>(tuple), std::get<Is>(arr).begin(), std::get<Is>(arr).end()));
}

} // namespace


namespace cmd {

template <typename TTraits>
Base<TTraits>::Base(ParamTuple params)
{
   if constexpr (ARG_SIZE > 0) {
      WriteToBuffer(std::get<0>(params),
                    std::get<0>(m_longArgs).begin(),
                    std::get<0>(m_longArgs).end());
   }
   if constexpr (ARG_SIZE > 1) {
      FillCharArrays(params, m_shortArgs, std::make_index_sequence<ARG_SIZE - 1>{});
   }
}

template <typename TTraits>
int32_t Base<TTraits>::GetId() const noexcept
{
   return ID;
}

template <typename TTraits>
size_t Base<TTraits>::GetArgsCount() const noexcept
{
   return ARG_SIZE;
}

template <typename TTraits>
std::string_view Base<TTraits>::GetArgAt(size_t index) const noexcept
{
   const char * buffer;
   switch (index) {
      case 0:
         assert(index < m_longArgs.size());
         buffer = m_longArgs[index].data();
         if constexpr (MAX_BUFFER_SIZE <= UCHAR_MAX) {
            assert(static_cast<size_t>(*buffer) <= m_longArgs[index].size());
            return std::string_view(buffer + 1, static_cast<size_t>(*buffer));
         } else {
            assert(std::string_view(buffer + 1).size() <= m_longArgs[index].size());
            return std::string_view(buffer + 1);
         }
      default:
         assert(index - 1 < m_shortArgs.size());
         buffer = m_shortArgs[index - 1].data();
         assert(static_cast<size_t>(*buffer) <= m_shortArgs[index - 1].size());
         return std::string_view(buffer + 1, static_cast<size_t>(*buffer));
   }
}

#define INSTANTIATE(name)                                        \
   template <>                                                   \
   std::string_view Base<name##Traits>::GetName() const noexcept \
   {                                                             \
      return #name;                                              \
   }                                                             \
   template class Base<name##Traits>

INSTANTIATE(StartListening);
INSTANTIATE(StartDiscovery);
INSTANTIATE(StopListening);
INSTANTIATE(StopDiscovery);
INSTANTIATE(CloseConnection);
INSTANTIATE(EnableBluetooth);
INSTANTIATE(NegotiationStart);
INSTANTIATE(NegotiationStop);
INSTANTIATE(SendMessage);
INSTANTIATE(SendLongMessage);
INSTANTIATE(ShowAndExit);
INSTANTIATE(ShowToast);
INSTANTIATE(ShowNotification);
INSTANTIATE(ShowRequest);
INSTANTIATE(ShowResponse);
INSTANTIATE(ShowLongResponse);
INSTANTIATE(ResetGame);
INSTANTIATE(ResetConnections);

} // namespace cmd