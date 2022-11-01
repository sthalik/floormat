#include "app.hpp"
#include "chunk.hpp"
namespace floormat {

static inline bool always_false()
{
    volatile bool ret = false;
    return ret;
}

bool floormat::test_tile_iter() // NOLINT(readability-function-size)
{
#if 0
    if (always_false())
    {
        const chunk c;
        for (const auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
        for (auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
        for (auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
        for (auto&& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
    }
#endif
    if (always_false())
    {
        chunk c;
        for (auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile_ref>);
#if 0
        // warns
        for (const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
        for (auto&& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile&>);
        // fails to compile
        for (const auto&& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
#endif
    }
    return true;
}

} // namespace floormat

