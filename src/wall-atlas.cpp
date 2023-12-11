#include "wall-atlas.hpp"
#include "compat/exception.hpp"
#include "src/tile-defs.hpp"
#include <utility>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

using namespace floormat::Wall;

namespace {

Vector2ui get_image_size(const ImageView2D& img)
{
    const Size<3> size = img.pixels().size();
    const auto width = size[1], height = size[0];
    fm_soft_assert(size[2] >= 3 && size[2] <= 4);
    fm_soft_assert(width > 0 && height > 0);
    return { (unsigned)width, (unsigned)height };
}

} // namespace

wall_atlas::wall_atlas() noexcept = default;
wall_atlas::~wall_atlas() noexcept = default;

Vector2ui wall_atlas::expected_size(unsigned depth, Group_ group)
{
    constexpr auto size = Vector3ui{iTILE_SIZE};
    constexpr auto half_tile = size.x()/2u;
    static_assert(size.x() == size.y());

    fm_assert(depth > 0 && depth < 1<<15);
    CORRADE_ASSUME(group < Group_::COUNT);

    switch (group)
    {
    using enum Group_;
    case wall:
        return { size.x(), size.z() };
    case top:
    case side:
        return { depth, size.z() };
    case corner_L:
        return { half_tile, size.z() };
    case corner_R:
        return { size.x() - half_tile, size.z() };
    default:
        fm_assert(false);
    }
}

wall_atlas::wall_atlas(wall_atlas_def def, String path, const ImageView2D& img)
    : _dir_array{std::move(def.direction_array)},
      _frame_array{std::move(def.frames)},
      _info{std::move(def.header)}, _path{std::move(path)},
      _image_size{get_image_size(img)},
      _direction_map{def.direction_map}
{
    fm_soft_assert(!_frame_array.empty());

    {
        const auto frame_count = _frame_array.size();
        bool found = false;
        for (auto [dir_name, dir] : wall_atlas::directions)
        {
            const auto* D = direction((size_t)dir);
            fm_soft_assert(def.direction_mask[(size_t)dir] == !!D);
            if (!D)
                continue;
            for (auto [group_name, gmemb, gr] : Direction::groups)
            {
                const auto& G = D->*gmemb;
                fm_soft_assert(!G.is_defined == !G.count);
                fm_soft_assert(G.is_defined == (G.index != (uint32_t)-1));
                if (!G.is_defined)
                    continue;
                found = true;
                fm_soft_assert(G.index < frame_count && G.index + G.count <= frame_count);
                const auto size = expected_size(_info.depth, gr);
                for (const auto& frame : ArrayView { &_frame_array[G.index], G.count })
                {
                    fm_soft_assert(frame.size == size);
                    fm_soft_assert(frame.offset + frame.size <= _image_size);
                }
            }
        }
        if (!found) [[unlikely]]
            fm_throw("wall_atlas '{}' is empty!"_cf, _path);
    }

    _texture.setLabel(_path)
            .setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Nearest)
            .setMinificationFilter(GL::SamplerFilter::Linear)
            .setMaxAnisotropy(1) // todo?
            .setBorderColor(Color4{1, 0, 0, 1})
            .setStorage(1, GL::textureFormat(img.format()), img.size())
            .setSubImage(0, {}, img);
}

auto wall_atlas::get_Direction(Direction_ num) const -> Direction*
{
    fm_debug_assert(num < Direction_::COUNT);

    if (auto DAI = _direction_map[(uint8_t)num])
        return const_cast<Direction*>(&_dir_array[DAI.val]);
    else [[unlikely]]
        return {};
}

auto wall_atlas::frames(const Group& group) const -> ArrayView<const Frame>
{
    if (_frame_array.empty()) [[unlikely]]
        return {};
    const auto size = _frame_array.size(); (void)size;
    const auto index = group.index, count = group.count;
    fm_assert(index < size && index <= index + count && index + count <= size);
    return { &_frame_array[index], count };
}

auto wall_atlas::frames(Direction_ dir, Group_ gr) const -> ArrayView<const Frame>
{
    const auto* D = get_Direction(dir);
    if (!D) [[unlikely]]
        fm_throw("no such direction: {}"_cf, (int)dir);
    const auto* G = group(*D, gr);
    if (!G) [[unlikely]]
        fm_throw("no such group {} for direction {}"_cf, (int)gr, (int)dir);
    return { _frame_array.data() + G->index, G->count };
}

auto wall_atlas::group(Direction_ dir, Group_ gr) const -> const Group* { return group((size_t)dir, (size_t)gr); }
auto wall_atlas::group(size_t dir, Group_ gr) const -> const Group* { return group(dir, (size_t)gr); }

auto wall_atlas::group(size_t dir, size_t group) const -> const Group*
{
    fm_assert(group < Group_COUNT);
    const auto* const set_ = direction(dir);
    if (!set_)
        return {};
    const auto& set = *set_;

    const auto memfn = set.groups[group].member;
    const Group& ret = set.*memfn;
#if 0
    if (ret.is_empty())
        return {};
#endif
    return &ret;
}

auto wall_atlas::group(const Direction& dir, Group_ tag) const -> const Group*
{
    fm_assert(tag < Group_::COUNT);
    const auto memfn = dir.groups[(size_t)tag].member;
    const Group& ret = dir.*memfn;
    if (!ret.is_defined)
        return {};
#if 0
    if (ret.is_empty())
        return {};
#endif
    return &ret;
}

auto wall_atlas::direction(size_t dir) const -> const Direction* { return get_Direction(Direction_(dir)); }
auto wall_atlas::direction(Direction_ dir) const -> const Direction* { return get_Direction(dir); }

auto wall_atlas::calc_direction(Direction_ dir) const -> const Direction&
{
    if (auto dai = _direction_map[(size_t)dir]) [[likely]]
        return _dir_array[dai.val];
    CORRADE_ASSUME(dir < Direction_::COUNT);
    switch (dir)
    {
    case Direction_::N:
        if (auto dai = _direction_map[(size_t)Direction_::W])
            return _dir_array[dai.val];
        break;
    case Direction_::W:
        if (auto dai = _direction_map[(size_t)Direction_::N])
            return _dir_array[dai.val];
        break;
    case Direction_::COUNT:
        std::unreachable();
        break;
    }
    fm_abort("wall_atlas: can't find direction '%d'", (int)dir);
}

uint8_t wall_atlas::direction_count() const { return (uint8_t)_dir_array.size(); }
auto wall_atlas::raw_frame_array() const -> ArrayView<const Frame> { return _frame_array; }
GL::Texture2D& wall_atlas::texture() { fm_debug_assert(_texture.id()); return _texture; }
Vector2ui wall_atlas::image_size() const { return _image_size; }

size_t wall_atlas::enum_to_index(enum rotation r)
{
    static_assert(rotation_COUNT == rotation{8});
    fm_debug_assert(r < rotation_COUNT);

    auto x = uint8_t(r);
    x >>= 1;
    return x;
}

} // namespace floormat

namespace floormat::Wall {

const Group& Direction::group(Group_ i) const { return const_cast<Direction&>(*this).group((size_t)i); }
const Group& Direction::group(size_t i) const { return const_cast<Direction&>(*this).group(i); }
Group& Direction::group(Group_ i) { return group((size_t)i); }

Group& Direction::group(size_t i)
{
    fm_assert(i < Group_COUNT);
    auto ptr = groups[i].member;
    return this->*ptr;
}

bool Frame::operator==(const Frame&) const noexcept = default;
bool Direction::operator==(const Direction&) const noexcept = default;
bool Info::operator==(const Info&) const noexcept = default;
bool DirArrayIndex::operator==(const DirArrayIndex&) const noexcept = default;

#if 1
bool Group::operator==(const Group&) const noexcept = default;
#else
bool Group::operator==(const Group& other) const noexcept
{
    bool ret = index == other.index && count == other.count &&
               pixel_size == other.pixel_size &&
               mirrored == other.mirrored && default_tint == other.default_tint;

    if (!ret)
        return ret;

    constexpr auto eps = 1e-5f;
    if (auto diff = Math::abs(Vector4(tint_mult) - Vector4(other.tint_mult)).sum(); diff > eps)
        return false;
    if (auto diff = Math::abs(Vector3(tint_add) - Vector3(other.tint_add)).sum(); diff > eps)
        return false;

    return true;
}
#endif

} // namespace floormat::Wall
