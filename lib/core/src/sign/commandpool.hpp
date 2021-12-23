#ifndef SIGN_COMMANDPOOL_HPP
#define SIGN_COMMANDPOOL_HPP

#include "utils/poolbuilder.hpp"
#include "sign/commands.hpp"

#define COMMAND_MEMPOOL_INITIAL_BLOCK_COUNT 1U

namespace cmd {
namespace internal {

template <typename L1, typename L2>
struct CommandPoolFor;

template <typename... Ts1, typename... Ts2>
struct CommandPoolFor<List<Ts1...>, List<Ts2...>>
{
   using Type = mem::PoolSuitableFor<Ts1..., Ts2...>;
};

} // namespace internal

using CommandPool =
   typename internal::CommandPoolFor<internal::UiDictionary, internal::BtDictionary>::Type;

inline CommandPool pool(COMMAND_MEMPOOL_INITIAL_BLOCK_COUNT);

} // namespace cmd

#endif // SIGN_COMMANDPOOL_HPP
