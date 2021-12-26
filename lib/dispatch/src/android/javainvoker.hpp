#ifndef JNI_JAVAINVOKER_HPP
#define JNI_JAVAINVOKER_HPP

#include "utils/poolptr.hpp"

#include <jni.h>
#include <memory>
#include <string_view>

namespace cmd {
class ICommand;
class IExternalInvoker;
} // namespace cmd

namespace jni {

class JavaInvoker : public std::enable_shared_from_this<JavaInvoker>
{
public:
   JavaInvoker(JNIEnv & env, jclass localRef, std::string_view methodName);
   ~JavaInvoker();

   std::unique_ptr<cmd::IExternalInvoker> GetExternalInvoker();

private:
   void PassCommand(mem::pool_ptr<cmd::ICommand> && cmd, int32_t id);

   JNIEnv & m_env;
   jclass m_class;
   jmethodID m_method;
};

} // namespace jni

#endif
