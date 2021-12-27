#ifndef BT_DEVICE_HPP
#define BT_DEVICE_HPP

#include <compare>
#include <functional>
#include <string>

namespace bt {

struct Device
{
   std::string name;
   std::string mac;
};

inline bool operator==(const Device & lhs, const Device & rhs)
{
   return lhs.mac == rhs.mac;
}

#ifdef _LIBCPP_VERSION
inline std::weak_ordering operator<=>(const Device & lhs, const Device & rhs)
{
   if (lhs.mac == rhs.mac)
      return std::weak_ordering::equivalent;
   return lhs.mac > rhs.mac ? std::weak_ordering::greater : std::weak_ordering::less;
}
#else
inline auto operator<=>(const Device & lhs, const Device & rhs)
{
   return operator<=>(lhs.mac, rhs.mac);
}
#endif
} // namespace bt

namespace std {

template <>
struct hash<bt::Device>
{
   using argument_type = bt::Device;
   using result_type = size_t;
   result_type operator()(const argument_type & device) const noexcept
   {
      return std::hash<std::string>{}(device.mac);
   }
};
} // namespace std

#endif // BT_DEVICE_HPP
