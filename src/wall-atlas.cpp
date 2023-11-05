#include "wall-atlas.hpp"
#include "compat/assert.hpp"
#include "compat/function2.hpp"
#include <utility>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

namespace {

#define FM_FRAMESET_ITER(Name) do { if (fun( #Name ## _s, const_cast<Self>(frameset.Name), wall_frame_set::type::Name)) return; } while(false)
#define FM_FRAMESET_ITER2(Str, Name) do { if (fun( Str, const_cast<Self>(frameset.Name), wall_frame_set::type::Name )) return; } while(false)

template<typename Self>
CORRADE_ALWAYS_INLINE void visit_frameset_impl(const wall_frame_set& frameset, auto&& fun)
{
    FM_FRAMESET_ITER(wall);
    FM_FRAMESET_ITER(overlay);
    FM_FRAMESET_ITER(side);
    FM_FRAMESET_ITER(top);
    FM_FRAMESET_ITER2("corner-L"_s, corner_L);
    FM_FRAMESET_ITER2("corner-R"_s, corner_R);
}

#undef FM_FRAMESET_ITER

} // namespace

size_t wall_atlas::enum_to_index(enum rotation r)
{
    static_assert(rotation_COUNT == rotation{8});
    fm_debug_assert(r < rotation_COUNT);

    auto x = uint8_t(r);
    x >>= 1;
    return x;
}

bool wall_frames::is_empty() const noexcept
{
    return count == 0;
}

bool wall_frame_set::is_empty() const noexcept
{
    return !wall.is_empty() && !overlay.is_empty() && !side.is_empty() && !top.is_empty() &&
           !corner_L.is_empty() && !corner_R.is_empty();
}

wall_atlas::wall_atlas(wall_info info, const ImageView2D& image,
                       Array<wall_frame> frames,
                       std::unique_ptr<wall_frame_set[]> framesets,
                       uint8_t frameset_count,
                       std::array<uint8_t, 4> frameset_indexes) :
    _framesets{std::move(framesets)}, _frame_array{std::move(frames)}, _info(std::move(info)),
    _frameset_indexes{frameset_indexes},
    _frameset_count{(uint8_t)frameset_count}
{
    {   fm_assert(frameset_count <= 4);
        uint8_t counts[4] = {}, total = 0;

        for (auto i = 0uz; i < frameset_count; i++)
            if (frameset_indexes[i] != (uint8_t)-1)
            {
                fm_assert(++counts[i] == 1);
                total++;
            }
        fm_assert(total == frameset_count);
    }

    _texture.setLabel(_info.name)
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
ArrayView<const wall_frame> wall_atlas::frame_array() const { return _frame_array; }
StringView wall_atlas::name() const { return _info.name; }
const wall_frame_set& wall_atlas::frameset(enum rotation r) const { return frameset(enum_to_index(r)); }

void wall_frame_set::visit(const fu2::function_view<bool(StringView name, const wall_frames& frames, type tag) const>& fun) const&
{
    visit_frameset_impl<const wall_frames&>(*this, fun);
}

void wall_frame_set::visit(const fu2::function_view<bool(StringView name, wall_frames& frames, type tag) const>& fun) &
{
    visit_frameset_impl<wall_frames&>(*this, fun);
}

const wall_frame_set& wall_atlas::frameset(size_t i) const
{
    fm_assert(i < 4 && _frameset_indexes[i] != (uint8_t)-1);
    return _framesets[i];
}

ArrayView<const wall_frame> wall_frames::items(const wall_atlas& a) const
{
    auto sz = a.frame_array().size(); (void)sz;
    fm_assert(index < sz && index + count <= sz);
    return { a.frame_array() + index, count };
}

} // namespace floormat
