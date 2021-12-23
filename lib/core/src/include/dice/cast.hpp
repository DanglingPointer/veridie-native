#ifndef DICE_CAST_HPP
#define DICE_CAST_HPP

#include <vector>
#include <variant>
#include <cstdint>

namespace dice {

namespace internal {

template <uint32_t Min, uint32_t Max>
struct SimpleValue
{
   enum : uint32_t
   {
      MIN = Min,
      MAX = Max
   };
   SimpleValue() noexcept
      : m_value(0U)
   {}
   void operator()(uint32_t value) noexcept { m_value = value; }
   operator uint32_t() const noexcept { return m_value; }

private:
   uint32_t m_value;
};

} // namespace internal

using D4 = std::vector<internal::SimpleValue<1U, 4U>>;

using D6 = std::vector<internal::SimpleValue<1U, 6U>>;

using D8 = std::vector<internal::SimpleValue<1U, 8U>>;

using D10 = std::vector<internal::SimpleValue<1U, 10U>>;

using D12 = std::vector<internal::SimpleValue<1U, 12U>>;

using D16 = std::vector<internal::SimpleValue<1U, 16U>>;

using D20 = std::vector<internal::SimpleValue<1U, 20U>>;

using D100 = std::vector<internal::SimpleValue<1U, 100U>>;

using Cast = std::variant<D4, D6, D8, D10, D12, D16, D20, D100>;

} // namespace dice

#endif // DICE_CAST_HPP
