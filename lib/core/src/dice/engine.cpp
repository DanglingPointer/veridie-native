#include "dice/engine.hpp"

#include <algorithm>
#include <random>

using namespace dice;

namespace {

class UniformEngine : public IEngine
{
public:
   UniformEngine()
      : m_rd()
      , m_generator(m_rd())
   {}
   template <typename D>
   void operator()(std::vector<D> & cast)
   {
      std::uniform_int_distribution<uint32_t> dist(D::MIN, D::MAX);
      for (auto & value : cast) {
         value(dist(m_generator));
      }
      std::sort(std::begin(cast), std::end(cast));
   }
   void GenerateResult(Cast & cast) override { std::visit(*this, cast); }

private:
   std::random_device m_rd;
   std::mt19937 m_generator;
};

struct SuccessCounter
{
   uint32_t threshold;

   template <typename D>
   size_t operator()(const std::vector<D> & cast)
   {
      size_t count = 0;
      for (const auto & value : cast) {
         if (value >= threshold)
            ++count;
      }
      return count;
   }
};

} // namespace

namespace dice {

size_t GetSuccessCount(const Cast & cast, uint32_t threshold)
{
   return std::visit(SuccessCounter{threshold}, cast);
}

std::unique_ptr<IEngine> CreateUniformEngine()
{
   return std::make_unique<UniformEngine>();
}

} // namespace dice
