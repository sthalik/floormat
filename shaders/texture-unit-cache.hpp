#pragma once
#include "compat/defs.hpp"
#include <cr/Array.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat {

struct texture_unit_cache final
{
    fm_DISABLE_COPY(texture_unit_cache);
    fm_DEFAULT_MOVE_(texture_unit_cache);

    struct statistics { size_t cache_hit, cache_miss; };

    texture_unit_cache();
    ~texture_unit_cache() noexcept;

    [[nodiscard]] int32_t bind(GL::AbstractTexture* ptr);
    [[nodiscard]] int32_t bind(GL::AbstractTexture& x);
    void invalidate();
    void lock(size_t i, GL::AbstractTexture* = (GL::AbstractTexture*)-1);
    void lock(size_t i, GL::AbstractTexture& tex);
    void unlock(size_t i, bool reuse_immediately = true);

    size_t reuse_count() const;
    size_t bind_count() const;

    void output_stats();
    statistics stats() const;

    void reset_stats();

private:
    static size_t get_unit_count();
    struct unit_data;

    size_t unit_count, lru_counter = 0, cache_miss_count = 0, cache_hit_count = 0;
    Array<unit_data> units;
};

} // namespace floormat
