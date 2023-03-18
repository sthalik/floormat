#pragma once
#include "compat/defs.hpp"
#include "scenery.hpp"
#include "anim.hpp"
#include <array>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Texture.h>

namespace Corrade::Containers {
class BitArray;
template<class> class BasicBitArrayView;
typedef BasicBitArrayView<const char> BitArrayView;
typedef BasicBitArrayView<char> MutableBitArrayView;
} // namespace Corrade::Containers

namespace floormat {

struct anim_atlas final
{
    using texcoords = std::array<Vector2, 4>;
    using quad = std::array<Vector3, 4>;

    anim_atlas() noexcept;
    anim_atlas(String name, const ImageView2D& tex, anim_def info) noexcept;
    ~anim_atlas() noexcept;

    anim_atlas(anim_atlas&&) noexcept;
    anim_atlas& operator=(anim_atlas&&) noexcept;

    StringView name() const noexcept;
    GL::Texture2D& texture() noexcept;
    const anim_def& info() const noexcept;

    const anim_group& group(rotation r) const noexcept;
    const anim_frame& frame(rotation r, std::size_t frame) const noexcept;
    texcoords texcoords_for_frame(rotation r, std::size_t frame, bool mirror) const noexcept;
    quad frame_quad(const Vector3& center, rotation r, std::size_t frame) const noexcept;

    BitArrayView bitmask() const;

    [[nodiscard]] rotation next_rotation_from(rotation r) const noexcept;
    [[nodiscard]] rotation prev_rotation_from(rotation r) const noexcept;
    [[nodiscard]] bool check_rotation(rotation r) const noexcept;
    rotation first_rotation() const noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(anim_atlas);

    static void make_bitmask_(const ImageView2D& tex, BitArray& array);
    static BitArray make_bitmask(const ImageView2D& tex);

private:
    String _name;
    BitArray _bitmask;
    anim_def _info;
    std::array<std::uint8_t, (std::size_t)rotation_COUNT> _group_indices = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    GL::Texture2D _tex;

    static decltype(_group_indices) make_group_indices(const anim_def& anim) noexcept;
    static std::uint8_t rotation_to_index(StringView name) noexcept;
};

} // namespace floormat
