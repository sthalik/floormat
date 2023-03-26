#include "app.hpp"
#include "chunk.hpp"
#include "world.hpp"
#include "tile-iterator.hpp"
namespace floormat {

static inline bool always_false()
{
    volatile bool ret = false;
    return ret;
}

void test_app::test_tile_iter() // NOLINT(readability-function-size)
{
    if (always_false())
    {
        world w;
        const chunk c{w};
        for ([[maybe_unused]] const auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_proto>);
        for ([[maybe_unused]] const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_proto>);
        for ([[maybe_unused]] auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile_proto>);
    }
    if (always_false())
    {
        world w;
        chunk c{w};
        for ([[maybe_unused]] auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile_ref>);
        for ([[maybe_unused]] const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_ref>);
    }
}

} // namespace floormat
