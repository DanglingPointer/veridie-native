#ifndef BT_DEVICE_HPP
#define BT_DEVICE_HPP

#include <functional>
#include <string>

namespace bt {

struct Device
{
   Device(std::string name, std::string mac)
      : name(std::move(name))
      , mac(std::move(mac))
   {}
   Device(const Device&) = default;
   Device(Device&&) noexcept = default;
   std::string name;
   std::string mac;

   bool operator==(const Device & rhs) const noexcept { return mac == rhs.mac; }
   bool operator!=(const Device & rhs) const noexcept { return !(*this == rhs); }
   bool operator<(const Device & rhs) const noexcept { return mac < rhs.mac; }
   bool operator>(const Device & rhs) const noexcept { return mac > rhs.mac; }
};
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
