#include "impl.hpp"
#include "loader/sprite-atlas-impl.hpp"
#include "src/anim-atlas.hpp"

namespace floormat::loader_detail {

void loader_impl::register_sprite(const class anim_atlas& a, rotation r, uint32_t f,
                                  const SpriteAtlas::Sprite* s) noexcept
{
    _sprite_registry.insert_or_assign(frame_key{ &a, r, f }, s);
}

const SpriteAtlas::Sprite* loader_impl::find_sprite(const class anim_atlas& a, rotation r,
                                                    uint32_t f) noexcept
{
    auto it = _sprite_registry.find(frame_key{ &a, r, f });
    const auto* s = it == _sprite_registry.end() ? nullptr : it->second;
    return s;
}

} // namespace floormat::loader_detail
