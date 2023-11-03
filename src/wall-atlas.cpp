#include "wall-atlas.hpp"
#include "compat/assert.hpp"
#include <utility>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

size_t wall_atlas::enum_to_index(enum rotation r)
{
    fm_debug_assert(r < rotation_COUNT);

    auto x = uint8_t(r);
    x >>= 1;
    return x;
}

wall_atlas::wall_atlas(wall_info info, const ImageView2D& image,
                       ArrayView<const wall_frame_set> rotations,
                       ArrayView<const wall_frame> frames) :
    _array{NoInit, frames.size()}, _info(std::move(info))
{
    fm_assert(info.depth > 0);
    fm_assert(rotations.size() <= _rotations.size());
    _rotation_count = (uint8_t)rotations.size();
    for (auto i = 0uz; const auto& fr : frames)
        _array[i++] = fr;
    for (auto i = 0uz; const auto& r : rotations)
        _rotations[i++] = r;

    _texture.setLabel(_name)
            .setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Nearest)
            .setMinificationFilter(GL::SamplerFilter::Linear)
            .setMaxAnisotropy(1)
            .setBorderColor(Color4{1, 0, 0, 1})
            .setStorage(1, GL::textureFormat(image.format()), image.size())
            .setSubImage(0, {}, image);
}

wall_atlas::wall_atlas() = default;
wall_atlas::~wall_atlas() noexcept = default;
const wall_frame_set& wall_atlas::frameset(size_t i) const { return _rotations[i]; }
const wall_frame_set& wall_atlas::frameset(enum rotation r) const { return frameset(enum_to_index(r)); }
const ArrayView<const wall_frame> wall_atlas::array() const { return _array; }
StringView wall_atlas::name() const { return _info.name; }

ArrayView<const wall_frame> wall_frames::items(const wall_atlas& a) const
{
    fm_assert(index < a.array().size());
    fm_debug_assert(count != (uint32_t)-1);
    return { a.array() + index, count };
}

} // namespace floormat
