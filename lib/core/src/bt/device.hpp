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

inline auto operator<=>(const Device & lhs, const Device & rhs)
{
   return operator<=>(lhs.mac, rhs.mac);
}
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
