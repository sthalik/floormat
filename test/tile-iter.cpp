#include "app.hpp"
#include "chunk.hpp"
namespace floormat {

static inline bool always_false()
{
    volatile bool ret = false;
    return ret;
}

bool app::test_tile_iter() // NOLINT(readability-function-size)
{
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
    if (always_false())
    {
        chunk c;
        for (auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile&>);
        for (const auto& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
        for (auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile&>);
#if 0
        // warns
        for (const auto [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
#endif
        for (auto&& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), tile&>);
#if 0
        // fails to compile
        for (const auto&& [x, k, pt] : c)
            static_assert(std::is_same_v<decltype(x), const tile&>);
#endif
    }
    return true;
}

} // namespace floormat

