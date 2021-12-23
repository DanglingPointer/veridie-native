#include <gtest/gtest.h>
#include "dice/engine.hpp"
#include "dice/serializer.hpp"

namespace {

TEST(DiceTest, generate_result)
{
   dice::Cast sequence = dice::D6(100);
   for (const auto & val : std::get<dice::D6>(sequence)) {
      EXPECT_EQ((uint32_t)val, 0U);
   }
   auto engine = dice::CreateUniformEngine();
   engine->GenerateResult(sequence);
   for (const auto & val : std::get<dice::D6>(sequence)) {
      EXPECT_GE((uint32_t)val, 1U);
      EXPECT_LE((uint32_t)val, 6U);
   }
}

TEST(DiceTest, count_success)
{
   dice::D8 sequence(10);
   size_t i = 0;
   for (auto & val : sequence) {
      val(i++);
   }
   const uint32_t threshold = 6;
   EXPECT_EQ(dice::GetSuccessCount(sequence, threshold), 4U);
}

TEST(DiceTest, deserialize_request_with_success_from)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(<Request type="D4" size="10" successFrom="3" />)";

      auto parsed = slzr->Deserialize(msg);
      dice::Request * r = std::get_if<dice::Request>(&parsed);
      EXPECT_TRUE(r);

      auto * cast = std::get_if<dice::D4>(&(r->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(10U, cast->size());
      for (const auto & val : *cast) {
         EXPECT_EQ((uint32_t)val, 0U);
      }
      EXPECT_TRUE(r->threshold);
      EXPECT_EQ(3U, *(r->threshold));
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_request_with_success_from)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      auto cast = dice::MakeCast("D4", 10);
      dice::Request request{std::move(cast), 3};
      std::string expected = R"(<Request successFrom="3" size="10" type="D4" />)";
      std::string actual = slzr->Serialize(request);
      EXPECT_EQ(expected, actual);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, deserialize_request_without_success_from)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(<Request type="D4" size="10" />)";

      auto parsed = slzr->Deserialize(msg);
      dice::Request * r = std::get_if<dice::Request>(&parsed);
      EXPECT_TRUE(r);

      auto * cast = std::get_if<dice::D4>(&(r->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(10U, cast->size());
      for (const auto & val : *cast) {
         EXPECT_EQ((uint32_t)val, 0U);
      }
      EXPECT_FALSE(r->threshold);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, deserialize_response_with_success_count)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(<Response type="D12" size="5" successCount="3">
                           <Val>1</Val>
                           <Val>2</Val>
                           <Val>3</Val>
                           <Val>4</Val>
                           <Val>5</Val>
                        </Response>)";

      auto parsed = slzr->Deserialize(msg);
      auto * r = std::get_if<dice::Response>(&parsed);
      EXPECT_TRUE(r);

      auto * cast = std::get_if<dice::D12>(&(r->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(5U, cast->size());
      for (int i = 0; i < 5; ++i) {
         EXPECT_EQ(i + 1, cast->at(i));
      }
      EXPECT_TRUE(r->successCount);
      EXPECT_EQ(3U, *(r->successCount));
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, deserialize_response_without_success_count)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(<Response type="D12" size="5">
                           <Val>1</Val>
                           <Val>2</Val>
                           <Val>3</Val>
                           <Val>4</Val>
                           <Val>5</Val>
                        </Response>)";

      auto parsed = slzr->Deserialize(msg);
      auto * r = std::get_if<dice::Response>(&parsed);
      EXPECT_TRUE(r);

      auto * cast = std::get_if<dice::D12>(&(r->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(5U, cast->size());
      for (int i = 0; i < 5; ++i) {
         EXPECT_EQ(i + 1, cast->at(i));
      }
      EXPECT_FALSE(r->successCount);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_request_with_threshold)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::D20 d(15);
      uint32_t successFrom = 5U;
      dice::Request r{d, successFrom};

      std::string serialized = slzr->Serialize(r);

      auto parsed = slzr->Deserialize(serialized);
      dice::Request * r1 = std::get_if<dice::Request>(&parsed);
      EXPECT_TRUE(r1);

      auto * cast = std::get_if<dice::D20>(&(r1->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(d, *cast);
      EXPECT_TRUE(r1->threshold);
      EXPECT_EQ(successFrom, *(r1->threshold));
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_request_without_threshold)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::D12 d(42);
      dice::Request r{d, std::nullopt};

      std::string serialized = slzr->Serialize(r);

      auto parsed = slzr->Deserialize(serialized);
      dice::Request * r1 = std::get_if<dice::Request>(&parsed);
      EXPECT_TRUE(r1);

      auto * cast = std::get_if<dice::D12>(&(r1->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(d, *cast);
      EXPECT_FALSE(r1->threshold);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_response_with_success_count)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::D100 d(6);
      for (int i = 0; i < 6; ++i) {
         d[i](7 - i);
      }
      uint32_t successCount = 1U;
      dice::Response r{d, successCount};

      std::string serialized = slzr->Serialize(r);

      auto parsed = slzr->Deserialize(serialized);
      auto * r1 = std::get_if<dice::Response>(&parsed);
      EXPECT_TRUE(r1);

      auto * cast = std::get_if<dice::D100>(&(r1->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(d, *cast);
      EXPECT_TRUE(r1->successCount);
      EXPECT_EQ(successCount, *(r1->successCount));
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_response_without_success_count)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::D10 d(42);
      for (int i = 0; i < 42; ++i) {
         d[i](i + 1);
      }
      dice::Response r{d, std::nullopt};

      std::string serialized = slzr->Serialize(r);

      auto parsed = slzr->Deserialize(serialized);
      auto * r1 = std::get_if<dice::Response>(&parsed);
      EXPECT_TRUE(r1);

      auto * cast = std::get_if<dice::D10>(&(r1->cast));
      EXPECT_TRUE(cast);
      EXPECT_EQ(d, *cast);
      EXPECT_FALSE(r1->successCount);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_hello)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::Hello hello{"5c:b9:01:f8:b6:49"};
      std::string expected = R"(<Hello><Mac>5c:b9:01:f8:b6:49</Mac></Hello>)";
      std::string actual = slzr->Serialize(hello);
      EXPECT_EQ(expected, actual);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, deserialize_hello)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(
<Hello>
   <Mac>5c:b9:01:f8:b6:49</Mac>
</Hello>)";

      auto parsed = slzr->Deserialize(msg);

      auto * hello = std::get_if<dice::Hello>(&parsed);
      EXPECT_TRUE(hello);
      EXPECT_EQ("5c:b9:01:f8:b6:49", hello->mac);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_hello)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::Hello initial{"5c:b9:01:f8:b6:49"};
      std::string serialized = slzr->Serialize(initial);
      auto parsed = slzr->Deserialize(serialized);
      auto * final = std::get_if<dice::Hello>(&parsed);
      EXPECT_TRUE(final);
      EXPECT_EQ(initial.mac, final->mac);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_offer)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::Offer offer{"5c:b9:01:f8:b6:49", 3U};
      std::string expected = R"(<Offer round="3"><Mac>5c:b9:01:f8:b6:49</Mac></Offer>)";
      std::string actual = slzr->Serialize(offer);
      EXPECT_EQ(expected, actual);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, deserialize_offer)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      std::string msg = R"(
<Offer round="1">
   <Mac>5c:b9:01:f8:b6:49</Mac>
</Offer>)";

      auto parsed = slzr->Deserialize(msg);

      auto * offer = std::get_if<dice::Offer>(&parsed);
      EXPECT_TRUE(offer);
      EXPECT_EQ("5c:b9:01:f8:b6:49", offer->mac);
      EXPECT_EQ(1U, offer->round);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

TEST(DiceTest, serialize_and_deserialize_offer)
{
   auto slzr = dice::CreateXmlSerializer();
   try {
      dice::Offer initial{"5c:b9:01:f8:b6:49", 2U};
      std::string serialized = slzr->Serialize(initial);
      auto parsed = slzr->Deserialize(serialized);
      auto * final = std::get_if<dice::Offer>(&parsed);
      EXPECT_TRUE(final);
      EXPECT_EQ(initial.mac, final->mac);
      EXPECT_EQ(initial.round, final->round);
   }
   catch (const std::invalid_argument & e) {
      ADD_FAILURE() << e.what();
   }
}

} // namespace