#include "ground-atlas.hpp"
#include "quads.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "tile-image.hpp"
#include "loader/loader.hpp"
#include <cstring>
#include <limits>
#include <cr/GrowableArray.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelStorage.h>

namespace floormat {

template class bptr<ground_atlas>;
template class bptr<const ground_atlas>;

ground_atlas::ground_atlas(ground_def info, const ImageView2D& image) :
    _def{move(info)}, _path{make_path(_def.name)}
{
    //Debug{} << "make ground_atlas" << _def.name;
    constexpr auto variant_max = std::numeric_limits<variant_t>::max();
    fm_soft_assert(num_tiles() <= variant_max);
    fm_soft_assert(_def.size.x() > 0 && _def.size.y() > 0);

    const Vector2ui img_size{(uint32_t)image.size().x(), (uint32_t)image.size().y()};
    fm_soft_assert(img_size.product() > 0);
    fm_soft_assert(img_size % Vector2ui{_def.size} == Vector2ui());
    const Vector2ui tile_px = img_size / Vector2ui{_def.size};

    const auto pixels = image.pixels();
    const auto px_size = (size_t)pixels.size()[2];
    const auto row_stride = (size_t)pixels.stride()[0];
    const auto* src_base = (const char*)pixels.data();
    PixelStorage storage;
    storage.setAlignment(1);

    arrayReserve(_frame_sprites, num_tiles());
    for (size_t i = 0; i < num_tiles(); i++)
    {
        const uint32_t gx = (uint32_t)(i % _def.size.x());
        const uint32_t gy = (uint32_t)(i / _def.size.x());
        const uint32_t td_x = gx * tile_px.x();
        const uint32_t td_y = gy * tile_px.y();
        // Y-flip: Magnum bottom-up, JSON top-down. See loader/anim-traits.cpp.
        const uint32_t mem_y_start = img_size.y() - td_y - tile_px.y();
        Array<char> packed{NoInit, (size_t)tile_px.product() * px_size};
        for (uint32_t yy = 0; yy < tile_px.y(); yy++)
        {
            const auto* src_row = src_base
                                + ((size_t)mem_y_start + yy) * row_stride
                                + (size_t)td_x * px_size;
            auto* dst_row = packed.data() + (size_t)yy * (size_t)tile_px.x() * px_size;
            std::memcpy(dst_row, src_row, (size_t)tile_px.x() * px_size);
        }
        const ImageView2D view{storage, image.format(),
                               {(Int)tile_px.x(), (Int)tile_px.y()},
                               ArrayView<const void>{packed.data(), packed.size()}};
        arrayAppend(_frame_sprites, loader.atlas().add(view, true));
    }
}

Quads::texcoords ground_atlas::texcoords_for_id(size_t i) const
{
    fm_assert(i < num_tiles());
    return loader.atlas().texcoords_for(_frame_sprites[i], false);
}

auto ground_atlas::raw_sprite_array() const -> ArrayView<const sprite> { return _frame_sprites; }

size_t ground_atlas::num_tiles() const { return Vector2ui{_def.size}.product(); }
enum pass_mode ground_atlas::pass_mode() const { return _def.pass; }

String ground_atlas::make_path(StringView name)
{
    char buf[fm_FILENAME_MAX];
    auto sv = loader.make_atlas_path(buf, loader.GROUND_TILESET_PATH, name);
    return String{sv};
}

} // namespace floormat
