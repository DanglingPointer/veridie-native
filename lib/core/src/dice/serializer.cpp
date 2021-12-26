#include "dice/serializer.hpp"
#include "dice/xmlparser.hpp"

#include <stdexcept>

namespace {

struct DiceTypeToString
{
   std::string operator()(const dice::D4 &) { return "D4"; }
   std::string operator()(const dice::D6 &) { return "D6"; }
   std::string operator()(const dice::D8 &) { return "D8"; }
   std::string operator()(const dice::D10 &) { return "D10"; }
   std::string operator()(const dice::D12 &) { return "D12"; }
   std::string operator()(const dice::D16 &) { return "D16"; }
   std::string operator()(const dice::D20 &) { return "D20"; }
   std::string operator()(const dice::D100 &) { return "D100"; }
};

struct RequestToXml
{
   std::string type;
   RequestToXml(std::string type)
      : type(std::move(type))
   {}
   template <typename D>
   std::unique_ptr<xml::Document<char>> operator()(const std::vector<D> & request)
   {
      auto doc = xml::NewDocument("Request");
      doc->GetRoot().AddAttribute("type", std::move(type));
      doc->GetRoot().AddAttribute("size", std::to_string(request.size()));
      return doc;
   }
};

struct ResponseToXml
{
   std::string type;
   ResponseToXml(std::string type)
      : type(std::move(type))
   {}
   template <typename D>
   std::unique_ptr<xml::Document<char>> operator()(const std::vector<D> & response)
   {
      auto doc = xml::NewDocument("Response");
      doc->GetRoot().AddAttribute("type", std::move(type));
      doc->GetRoot().AddAttribute("size", std::to_string(response.size()));
      for (const auto & val : response) {
         doc->GetRoot().AddChild("Val").SetContent(std::to_string(val));
      }
      return doc;
   }
};

struct FillValues
{
   const std::vector<uint32_t> & values;
   FillValues(const std::vector<uint32_t> & values)
      : values(values)
   {}
   template <typename D>
   void operator()(std::vector<D> & cast)
   {
      for (size_t i = 0; i < values.size(); ++i) {
         cast[i](values[i]);
      }
   }
};

class XmlSerializer : public dice::ISerializer
{
public:
   std::string Serialize(const dice::Request & request) override;
   std::string Serialize(const dice::Response & response) override;
   std::string Serialize(const dice::Hello & hello) override;
   std::string Serialize(const dice::Offer & offer) override;

   std::variant<dice::Hello, dice::Offer, dice::Request, dice::Response>
   Deserialize(std::string_view message) override;

private:
   dice::Request ParseRequest(std::unique_ptr<const xml::Document<char>> doc);
   dice::Response ParseResponse(std::unique_ptr<const xml::Document<char>> doc);
   dice::Hello ParseHello(std::unique_ptr<const xml::Document<char>> doc);
   dice::Offer ParseOffer(std::unique_ptr<const xml::Document<char>> doc);
};

std::string XmlSerializer::Serialize(const dice::Request & request)
{
   std::string type = request.cast.Apply(DiceTypeToString{});
   auto doc = request.cast.Apply(RequestToXml{std::move(type)});
   if (request.threshold) {
      doc->GetRoot().AddAttribute("successFrom", std::to_string(*request.threshold));
   }
   return doc->ToString();
}

std::string XmlSerializer::Serialize(const dice::Response & response)
{
   std::string type = response.cast.Apply(DiceTypeToString{});
   auto doc = response.cast.Apply(ResponseToXml{std::move(type)});
   if (response.successCount) {
      doc->GetRoot().AddAttribute("successCount", std::to_string(*response.successCount));
   }
   return doc->ToString();
}

std::string XmlSerializer::Serialize(const dice::Hello & hello)
{
   auto doc = xml::NewDocument("Hello");
   doc->GetRoot().AddChild("Mac").SetContent(hello.mac);
   return doc->ToString();
}

std::string XmlSerializer::Serialize(const dice::Offer & offer)
{
   auto doc = xml::NewDocument("Offer");
   doc->GetRoot().AddAttribute("round", std::to_string(offer.round));
   doc->GetRoot().AddChild("Mac").SetContent(offer.mac);
   return doc->ToString();
}

std::variant<dice::Hello, dice::Offer, dice::Request, dice::Response>
XmlSerializer::Deserialize(std::string_view message)
{
   auto doc = xml::ParseString(message.data(), false);
   const auto & name = doc->GetRoot().GetName();
   if (name == "Request")
      return ParseRequest(std::move(doc));
   if (name == "Response")
      return ParseResponse(std::move(doc));
   if (name == "Hello")
      return ParseHello(std::move(doc));
   if (name == "Offer")
      return ParseOffer(std::move(doc));
   throw std::invalid_argument("Deserialize(): Unknown message type: " + name);
}

dice::Request XmlSerializer::ParseRequest(std::unique_ptr<const xml::Document<char>> doc)
{
   std::string type = doc->GetRoot().GetAttributeValue("type");
   size_t size = std::stoul(doc->GetRoot().GetAttributeValue("size"));
   std::optional<uint32_t> successFrom;
   try {
      successFrom = std::stoul(doc->GetRoot().GetAttributeValue("successFrom"));
   }
   catch (const xml::Exception &) {
   }
   return dice::Request{dice::MakeCast(type, size), successFrom};
}

dice::Response XmlSerializer::ParseResponse(std::unique_ptr<const xml::Document<char>> doc)
{
   std::string type = doc->GetRoot().GetAttributeValue("type");
   size_t size = std::stoul(doc->GetRoot().GetAttributeValue("size"));
   dice::Cast cast = dice::MakeCast(type, size);

   std::vector<uint32_t> values(size);
   for (size_t i = 0; i < size; ++i) {
      values[i] = std::stoul(doc->GetRoot().GetChild(i).GetContent());
   }
   cast.Apply(FillValues(values));

   std::optional<size_t> successCount;
   try {
      successCount = std::stoul(doc->GetRoot().GetAttributeValue("successCount"));
   }
   catch (const xml::Exception &) {
   }
   return dice::Response{std::move(cast), successCount};
}

dice::Hello XmlSerializer::ParseHello(std::unique_ptr<const xml::Document<char>> doc)
{
   return dice::Hello{doc->GetRoot().GetChild("Mac").GetContent()};
}

dice::Offer XmlSerializer::ParseOffer(std::unique_ptr<const xml::Document<char>> doc)
{
   return dice::Offer{doc->GetRoot().GetChild("Mac").GetContent(),
                      static_cast<uint32_t>(std::stoi(doc->GetRoot().GetAttributeValue("round")))};
}

} // namespace

namespace dice {

dice::Cast MakeCast(const std::string & type, size_t size)
{
   if (type == "D4")
      return dice::D4(size);
   if (type == "D6")
      return dice::D6(size);
   if (type == "D8")
      return dice::D8(size);
   if (type == "D10")
      return dice::D10(size);
   if (type == "D12")
      return dice::D12(size);
   if (type == "D16")
      return dice::D16(size);
   if (type == "D20")
      return dice::D20(size);
   if (type == "D100")
      return dice::D100(size);
   throw std::invalid_argument("MakeCast(): Invalid cast type: " + type);
}

std::string TypeToString(const dice::Cast & cast)
{
   return cast.Apply(DiceTypeToString{});
}

std::unique_ptr<dice::ISerializer> CreateXmlSerializer()
{
   return std::make_unique<XmlSerializer>();
}
} // namespace dice