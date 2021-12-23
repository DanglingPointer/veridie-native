#ifndef DICE_SERIALIZER_HPP
#define DICE_SERIALIZER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include "dice/cast.hpp"

namespace dice {

struct Request
{
   dice::Cast cast;
   std::optional<uint32_t> threshold;
};

struct Response
{
   dice::Cast cast;
   std::optional<size_t> successCount;
};

struct Hello
{
   std::string mac;
};

struct Offer
{
   std::string mac;
   uint32_t round;
};

dice::Cast MakeCast(const std::string & type, size_t size);
std::string TypeToString(const dice::Cast & cast);

class ISerializer
{
public:
   virtual ~ISerializer() = default;
   virtual std::string Serialize(const dice::Request & request) = 0;
   virtual std::string Serialize(const dice::Response & response) = 0;
   virtual std::string Serialize(const dice::Hello & hello) = 0;
   virtual std::string Serialize(const dice::Offer & offer) = 0;

   virtual std::variant<dice::Hello, dice::Offer, dice::Request, dice::Response>
   Deserialize(std::string_view message) = 0;
};

std::unique_ptr<dice::ISerializer> CreateXmlSerializer();

} // namespace dice

#endif // DICE_SERIALIZER_HPP
