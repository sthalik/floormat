#pragma once
#include "compat/defs.hpp"
#include <Corrade/Containers/Array.h>

namespace Magnum::GL { class AbstractTexture; }

namespace floormat {

struct texture_unit_cache final
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(texture_unit_cache);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(texture_unit_cache);

    texture_unit_cache();

    [[nodiscard]] int32_t bind(GL::AbstractTexture* ptr);
    [[nodiscard]] int32_t bind(GL::AbstractTexture& x);
    void invalidate();
    void lock(size_t i, GL::AbstractTexture* = (GL::AbstractTexture*)-1);
    void lock(size_t i, GL::AbstractTexture& tex) { lock(i, &tex); }
    void unlock(size_t i, bool immediately = true);

    size_t reuse_count() const { return cache_hit_count; }
    size_t bind_count() const { return rebind_count; }
    void reset_stats() { rebind_count = cache_hit_count = 0; }

private:
    static size_t get_unit_count();
    struct unit_data;

    size_t unit_count, lru_counter = 0, rebind_count = 0, cache_hit_count = 0;
    Array<unit_data> units;
};

} // namespace floormat