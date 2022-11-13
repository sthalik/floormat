#include "app.hpp"
#include "chunk.hpp"
namespace floormat {

static inline bool always_false()
{
    volatile bool ret = false;
    return ret;
}

bool test_app::test_tile_iter() // NOLINT(readability-function-size)
{
    if (always_false())
    {
        const chunk c;
        for ([[maybe_unused]] const auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_proto>);
        for ([[maybe_unused]] const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_proto>);
        for ([[maybe_unused]] auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile_proto>);
    }
    if (always_false())
    {
        chunk c;
        for ([[maybe_unused]] auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile_ref>);
        for ([[maybe_unused]] const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile_ref>);
    }
    return true;
}

} // namespace floormat
