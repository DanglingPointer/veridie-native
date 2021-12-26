#include "sign/commands.hpp"

#include "utils/format.hpp"

#include <cassert>
#include <cstring>
#include <climits>

namespace {

template <typename T>
void WriteToBuffer(T && val, std::span<char> buffer)
{
   const auto rest = fmt::Format(buffer.subspan(1), "{}", std::forward<T>(val));
   const size_t length = buffer.size() - rest.size() - 1;
   buffer[0] = static_cast<unsigned char>(length);
}

void WriteToBuffer(std::chrono::seconds dur, std::span<char> buffer) noexcept
{
   WriteToBuffer(dur.count(), buffer);
}

template <typename T, size_t Length, size_t Size, size_t... Is>
void FillCharArrays(T tuple,
                    std::array<std::array<char, Length>, Size> & arr,
                    std::index_sequence<Is...>)
{
   (..., WriteToBuffer(std::get<Is + 1>(tuple), {std::get<Is>(arr)}));
}

} // namespace

namespace cmd {

template <typename TTraits>
Base<TTraits>::Base(ParamTuple params)
{
   if constexpr (ARG_SIZE > 0) {
      WriteToBuffer(std::get<0>(params), {std::get<0>(m_longArgs)});
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