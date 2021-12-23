#ifndef SIGN_CMD_HPP
#define SIGN_CMD_HPP

#include <cstdint>
#include <string_view>

#define COMMAND_ID(id) \
   (id << 8)

namespace cmd {

class ICommand
{
public:
   // response codes must be in sync with interop/Command.java
   enum ResponseCode : int64_t {
      OK = 0,
      INVALID_STATE = -1,
      INTEROP_FAILURE = -2,
      BLUETOOTH_OFF = 2,
      LISTEN_FAILED = 3,
      CONNECTION_NOT_FOUND = 4,
      NO_BT_ADAPTER = 5,
      USER_DECLINED = 6,
      SOCKET_ERROR = 7,
   };

   virtual ~ICommand() = default;
   virtual int32_t GetId() const = 0;
   virtual std::string_view GetName() const = 0;
   virtual size_t GetArgsCount() const = 0;
   virtual std::string_view GetArgAt(size_t index) const = 0;
};

inline std::string_view ToString(ICommand::ResponseCode code)
{
#define CASE(name) case ICommand::name: return #name
   switch (code) {
   CASE(OK);
   CASE(INVALID_STATE);
   CASE(INTEROP_FAILURE);
   CASE(BLUETOOTH_OFF);
   CASE(LISTEN_FAILED);
   CASE(CONNECTION_NOT_FOUND);
   CASE(NO_BT_ADAPTER);
   CASE(USER_DECLINED);
   CASE(SOCKET_ERROR);
   }
#undef CASE
   return "Unknown";
}

} // namespace cmd

#endif
