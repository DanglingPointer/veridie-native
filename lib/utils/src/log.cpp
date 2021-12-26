#include "utils/log.hpp"

#include <cstdio>
#include <ctime>

namespace {

void StdLog(FILE * file, char what, const char * tag, const char * text)
{
   char timeBuf[64];
   const std::time_t t = std::time(nullptr);
   [[maybe_unused]] const size_t ret1 =
      std::strftime(std::data(timeBuf), std::size(timeBuf), "%F %T", std::gmtime(&t));
   assert(ret1 > 0);

   fprintf(file, "%s %c/%s: %s\n", std::data(timeBuf), what, tag, text);
}

} // namespace


Log::Handler Log::s_debugHandler = nullptr;
Log::Handler Log::s_infoHandler = nullptr;
Log::Handler Log::s_warningHandler = nullptr;
Log::Handler Log::s_errorHandler = nullptr;
Log::Handler Log::s_fatalHandler = nullptr;

void Log::Debug(const char * tag, const char * text)
{
   if (s_debugHandler)
      s_debugHandler(tag, text);
   else
      StdLog(stdout, 'D', tag, text);
}

void Log::Info(const char * tag, const char * text)
{
   if (s_infoHandler)
      s_infoHandler(tag, text);
   else
      StdLog(stdout, 'I', tag, text);
}

void Log::Warning(const char * tag, const char * text)
{
   if (s_warningHandler)
      s_warningHandler(tag, text);
   else
      StdLog(stderr, 'W', tag, text);
}

void Log::Error(const char * tag, const char * text)
{
   if (s_errorHandler)
      s_errorHandler(tag, text);
   else
      StdLog(stderr, 'E', tag, text);
}

void Log::Fatal(const char * tag, const char * text)
{
   if (s_fatalHandler)
      s_fatalHandler(tag, text);
   else
      StdLog(stderr, 'F', tag, text);
   std::abort();
}
