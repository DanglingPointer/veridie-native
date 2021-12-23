#ifndef TESTS_FAKELOGGER_HPP
#define TESTS_FAKELOGGER_HPP

#include "utils/log.hpp"

#include <algorithm>
#include <vector>

class FakeLogger
{
public:
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


   FakeLogger()
   {
      s_lines.clear();
      Log::s_debugHandler = LogDebug;
      Log::s_infoHandler = LogInfo;
      Log::s_warningHandler = LogWarning;
      Log::s_errorHandler = LogError;
      Log::s_fatalHandler = LogFatal;
   }
   ~FakeLogger()
   {
      s_lines.clear();
      Log::s_debugHandler = nullptr;
      Log::s_infoHandler = nullptr;
      Log::s_warningHandler = nullptr;
      Log::s_errorHandler = nullptr;
      Log::s_fatalHandler = nullptr;
   }


   std::vector<LogLine> GetEntries() const { return s_lines; }
   std::string GetLastStateLine() const
   {
      auto it = std::find_if(s_lines.crbegin(), s_lines.crend(), [](const LogLine & line) {
         return line.text.starts_with("New state:");
      });
      if (it != s_lines.crend())
         return it->text;
      return "";
   }
   bool Empty() const { return s_lines.empty(); }
   void Clear() { s_lines.clear(); }
   bool NoWarningsOrErrors() const
   {
      for (const auto & line : s_lines) {
         switch (line.lvl) {
         case Level::ERROR:
         case Level::WARNING:
         case Level::FATAL:
            return false;
         default:
            break;
         }
      }
      return true;
   }
   void DumpLines() const
   {
      for (const auto & entry : s_lines) {
         fprintf(stderr,
                 "Tag(%s) Prio(%d): %s\n",
                 entry.tag.c_str(),
                 static_cast<int>(entry.lvl),
                 entry.text.c_str());
      }
   }

private:
   static inline std::vector<LogLine> s_lines;

   static void LogDebug(const char * tag, const char * text)
   {
      s_lines.emplace_back(LogLine{Level::DEBUG, tag, text});
   }
   static void LogInfo(const char * tag, const char * text)
   {
      s_lines.emplace_back(LogLine{Level::INFO, tag, text});
   }
   static void LogWarning(const char * tag, const char * text)
   {
      s_lines.emplace_back(LogLine{Level::WARNING, tag, text});
   }
   static void LogError(const char * tag, const char * text)
   {
      s_lines.emplace_back(LogLine{Level::ERROR, tag, text});
   }
   static void LogFatal(const char * tag, const char * text)
   {
      s_lines.emplace_back(LogLine{Level::FATAL, tag, text});
   }
};

#endif // TESTS_FAKELOGGER_HPP
