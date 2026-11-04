#pragma once
#include "compat/defs.hpp"
#include <cr/Array.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat {

struct texture_unit_cache final
{
    fm_DISABLE_COPY(texture_unit_cache);
    fm_DEFAULT_MOVE_(texture_unit_cache);

    struct statistics { usize cache_hit, cache_miss; };

    texture_unit_cache();
    ~texture_unit_cache() noexcept;

    [[nodiscard]] i32 bind(GL::AbstractTexture* ptr);
    [[nodiscard]] i32 bind(GL::AbstractTexture& x);
    void invalidate();
    void lock(usize i, GL::AbstractTexture* = (GL::AbstractTexture*)-1);
    void lock(usize i, GL::AbstractTexture& tex);
    void unlock(usize i, bool reuse_immediately = true);

    usize reuse_count() const;
    usize bind_count() const;

    void output_stats();
    statistics stats() const;

    void reset_stats();

private:
    static usize get_unit_count();
    struct unit_data;

    usize unit_count, lru_counter = 0, cache_miss_count = 0, cache_hit_count = 0;
    Array<unit_data> units;
};

} // namespace floormat
