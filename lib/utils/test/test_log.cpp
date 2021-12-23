#include <gtest/gtest.h>

#include "utils/log.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

struct LogFixture : public ::testing::Test
{
   enum class Level
   {
      DEBUG,
      INFO,
      WARNING,
      ERROR,
      FATAL
   };

   struct LogLine
   {
      Level lvl;
      std::string tag;
      std::string text;
   };
   static inline std::vector<LogLine> lines;


   static void LogDebug(const char * tag, const char * text)
   {
      lines.emplace_back(LogLine{Level::DEBUG, tag, text});
   }
   static void LogInfo(const char * tag, const char * text)
   {
      lines.emplace_back(LogLine{Level::INFO, tag, text});
   }
   static void LogWarning(const char * tag, const char * text)
   {
      lines.emplace_back(LogLine{Level::WARNING, tag, text});
   }
   static void LogError(const char * tag, const char * text)
   {
      lines.emplace_back(LogLine{Level::ERROR, tag, text});
   }
   static void LogFatal(const char * tag, const char * text)
   {
      lines.emplace_back(LogLine{Level::FATAL, tag, text});
   }

   LogFixture()
   {
      lines.clear();
      Log::s_debugHandler = LogDebug;
      Log::s_infoHandler = LogInfo;
      Log::s_warningHandler = LogWarning;
      Log::s_errorHandler = LogError;
      Log::s_fatalHandler = LogFatal;
   }
   ~LogFixture()
   {
      lines.clear();
      Log::s_debugHandler = nullptr;
      Log::s_infoHandler = nullptr;
      Log::s_warningHandler = nullptr;
      Log::s_errorHandler = nullptr;
      Log::s_fatalHandler = nullptr;
   }
};

TEST_F(LogFixture, logging_integral_types)
{
   Log::Debug("TAG",
              "max int64_t is {}, min int64_t is {}",
              std::numeric_limits<int64_t>::max(),
              std::numeric_limits<int64_t>::min());
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::DEBUG, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ("max int64_t is 9223372036854775807, min int64_t is -9223372036854775808",
                lines.back().text.c_str());

   lines.clear();
   Log::Info("TAG",
             "max uint64_t is {}, min uint64_t is {}",
             std::numeric_limits<uint64_t>::max(),
             std::numeric_limits<uint64_t>::min());
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ("max uint64_t is 18446744073709551615, min uint64_t is 0",
                lines.back().text.c_str());

   lines.clear();
   Log::Warning("TAG", "true bool is {}, false bool is {}", true, false);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::WARNING, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ("true bool is true, false bool is false", lines.back().text.c_str());

   lines.clear();
   Log::Error("TAG", "space is '{}', tilde is '{}'", ' ', '~');
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::ERROR, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ("space is ' ', tilde is '~'", lines.back().text.c_str());
}

TEST_F(LogFixture, logging_pointer_type)
{
   auto * ptr = reinterpret_cast<int *>(0x123);
   Log::Info("tag", "Pointers like {} are logged in hex", ptr);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("Pointers like 0x123 are logged in hex", lines.back().text.c_str());
}

TEST_F(LogFixture, logging_string_types)
{
   // clang-format off
   const std::string veryLongString =
      "“Two Catholics who have never met can nevertheless go together on crusade or pool funds to\n"
      "build a hospital because they both believe that God was incarnated in human flesh and allowed\n"
      "Himself to be crucified to redeem our sins. States are rooted in common national myths. Two\n"
      "Serbs who have never met might risk their lives to save one another because both believe in\n"
      "the existence of the Serbian nation, the Serbian homeland and the Serbian flag. Judicial\n"
      "systems are rooted in common legal myths. Two lawyers who have never met can nevertheless\n"
      "combine efforts to defend a complete stranger because they both believe in the existence of\n"
      "laws, justice, human rights – and the money paid out in fees. Yet none of these things exists\n"
      "outside the stories that people invent and tell one another. There are no gods in the\n"
      "universe, no nations, no money, no human rights, no laws, and no justice outside the common\n"
      "imagination of human beings.”\n";
   // clang-format on

   Log::Info("TAG", "Here is a very long string: {}", veryLongString);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ(
      ("Here is a very long string: " + veryLongString).substr(0, Log::MAX_LINE_LENGTH).c_str(),
      lines.back().text.c_str());

   const std::string_view veryLongStringView = veryLongString;

   lines.clear();
   Log::Error("ANOTHER_TAG",
              "Here is a very long string view: {} and something else",
              veryLongStringView);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::ERROR, lines.back().lvl);
   EXPECT_STREQ("ANOTHER_TAG", lines.back().tag.c_str());
   EXPECT_STREQ(("Here is a very long string view: " + veryLongString)
                   .substr(0, Log::MAX_LINE_LENGTH)
                   .c_str(),
                lines.back().text.c_str());

   lines.clear();
   Log::Warning("TAG",
                "{ c-style string 1 is {{}}}, c-style string 2 is {} }",
                "string1",
                "string2");
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::WARNING, lines.back().lvl);
   EXPECT_STREQ("TAG", lines.back().tag.c_str());
   EXPECT_STREQ("{ c-style string 1 is {string1}}, c-style string 2 is string2 }",
                lines.back().text.c_str());
}

TEST_F(LogFixture, logging_no_args_texts)
{
   Log::Error("tag", "This  will ignore {} because there are no args");
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::ERROR, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("This  will ignore {} because there are no args", lines.back().text.c_str());

   lines.clear();
   std::array<char, 1024> buffer{};
   fmt::Format(buffer, "This will {} ignore because there are args", "not");
   Log::Warning("tag", buffer.data());
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::WARNING, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("This will not ignore because there are args", lines.back().text.c_str());
}

struct Dimensions
{
   int width = 0;
   int height = 0;
};

std::span<char> WriteAsText(const Dimensions & dims, std::span<char> dest)
{
   return fmt::Format(dest, "{}x{}", dims.width, dims.height);
}

TEST_F(LogFixture, logging_custom_formattable_type)
{
   const Dimensions dims{1920, 1080};
   Log::Info("tag", "The dimensions are {}p", dims);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("The dimensions are 1920x1080p", lines.back().text.c_str());
}

TEST_F(LogFixture, logging_too_few_or_too_many_args)
{
   Log::Info("tag", "The superfluous {} will be at the end", "argument", 42);
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("The superfluous argument will be at the end42", lines.back().text.c_str());

   lines.clear();
   Log::Info("tag", "Too few {} will not {} an exception", "arguments");
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::INFO, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("Too few arguments will not ", lines.back().text.c_str());

   lines.clear();
   Log::Error("tag", "This {} never {} crash", std::string("will"));
   ASSERT_FALSE(lines.empty());
   EXPECT_EQ(Level::ERROR, lines.back().lvl);
   EXPECT_STREQ("tag", lines.back().tag.c_str());
   EXPECT_STREQ("This will never ", lines.back().text.c_str());
}

} // namespace
