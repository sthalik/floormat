#pragma once
#include "compat/defs.hpp"
#include "rotation.hpp"
#include "anim.hpp"
#include "src/quads.hpp"
#include "compat/borrowed-ptr.hpp"
#include <array>
#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

class anim_atlas final : public bptr_base
{
    using texcoords = Quads::texcoords;
    using quad = Quads::quad;

    String _name;
    BitArray _bitmask;
    anim_def _info;
    std::array<uint8_t, (size_t)rotation_COUNT> _group_indices = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    GL::Texture2D _tex;

    static decltype(_group_indices) make_group_indices(const anim_def& anim) noexcept;
    static uint8_t rotation_to_index(StringView name);

public:
    anim_atlas() noexcept;
    anim_atlas(String name, const ImageView2D& tex, anim_def info);
    ~anim_atlas() noexcept override;

    anim_atlas(anim_atlas&&) noexcept;
    anim_atlas& operator=(anim_atlas&&) noexcept;

    StringView name() const noexcept;
    GL::Texture2D& texture() noexcept;
    const anim_def& info() const noexcept;

    const anim_group& group(rotation r) const;
    const anim_frame& frame(rotation r, size_t frame) const;
    texcoords texcoords_for_frame(rotation r, size_t frame, bool mirror) const noexcept;
    quad frame_quad(const Vector3& center, rotation r, size_t frame) const noexcept;

    BitArrayView bitmask() const;

    [[nodiscard]] rotation next_rotation_from(rotation r) const noexcept;
    [[nodiscard]] rotation prev_rotation_from(rotation r) const noexcept;
    [[nodiscard]] bool check_rotation(rotation r) const noexcept;
    rotation first_rotation() const noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(anim_atlas);

    static void make_bitmask_(const ImageView2D& tex, BitArray& array);
    static BitArray make_bitmask(const ImageView2D& tex);
};

} // namespace floormat
