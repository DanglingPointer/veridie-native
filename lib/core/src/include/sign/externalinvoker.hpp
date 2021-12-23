#ifndef SIGN_EXTERNALINVOKER_HPP
#define SIGN_EXTERNALINVOKER_HPP

#include <cstdint>
#include <memory>

#include "utils/poolptr.hpp"

namespace cmd {
class ICommand;

class IExternalInvoker
{
public:
   virtual ~IExternalInvoker() = default;
   virtual bool Invoke(mem::pool_ptr<ICommand> && data, int32_t id) = 0;
};

} // namespace cmd

#endif // SIGN_EXTERNALINVOKER_HPP
