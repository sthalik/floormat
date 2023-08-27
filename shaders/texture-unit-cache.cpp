#include "texture-unit-cache.hpp"

#include "compat/assert.hpp"
#include <Corrade/Containers/String.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

struct texture_unit_cache::unit_data final
{
    GL::AbstractTexture* ptr;
    size_t lru_val;
};

int32_t texture_unit_cache::bind(GL::AbstractTexture* tex)
{
    fm_debug_assert(tex != nullptr && tex != (GL::AbstractTexture*)-1);

    constexpr auto invalid = (size_t)-1;
    auto unbound_id = invalid;

    for (auto i = 0uz; i < unit_count; i++)
    {
        auto& unit = units[i];
        auto* ptr = unit.ptr;
        if (!ptr)
            unbound_id = i;
        else if (ptr == tex)
        {
            unit.lru_val = ++lru_counter;
            ++cache_hit_count;
            return (int32_t)i;
        }
    }

    if (unbound_id != invalid)
    {
        units[unbound_id] = {tex, ++lru_counter};
        tex->bind((Int)unbound_id);
        ++cache_hit_count;
        auto label = tex->label();
        Debug{Debug::Flag::NoSpace} << "binding '" << tex->label() << "' to " << unbound_id;
        return (int32_t)unbound_id;
    }
    else
    {
        auto min_lru = invalid, min_index = invalid;
        for (auto i = 0uz; i < unit_count; i++)
        {
            auto& unit = units[i];
            if (unit.lru_val < min_lru)
            {
                min_lru = unit.lru_val;
                min_index = i;
            }
        }
        fm_assert(min_index != invalid);
        ++rebind_count;
        units[min_index] = {tex, ++lru_counter};
        tex->bind((Int)min_index);
        Debug{Debug::Flag::NoSpace} << "binding '" << tex->label() << "' to " << min_index;
        return (int32_t)min_index;
    }
}

texture_unit_cache::texture_unit_cache() :
    unit_count{get_unit_count()},
    units{ValueInit, unit_count}
{
}

void texture_unit_cache::invalidate()
{
    units = {};
    lru_counter = 0;
    rebind_count = 0;
}

void texture_unit_cache::lock(floormat::size_t i, GL::AbstractTexture* tex)
{
    fm_assert(i < unit_count);
    units[i] = { .ptr = tex, .lru_val = (uint64_t)i, };
}

void texture_unit_cache::unlock(size_t i, bool immediately)
{
    fm_assert(i < unit_count);
    if (units[i].ptr == (GL::AbstractTexture*)-1)
        immediately = true;
    units[i] = { .ptr = units[i].ptr, .lru_val = immediately ? 0 : ++lru_counter };
}

size_t texture_unit_cache::get_unit_count()
{
    static auto ret = [] {
        GLint value = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
        fm_assert(value >= /*GL 3.3*/ 16);
        return value;
    }();
    return (size_t)ret;
}

int32_t texture_unit_cache::bind(GL::AbstractTexture& x) { return bind(&x); }

} // namespace floormat
