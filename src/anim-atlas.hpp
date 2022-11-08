#pragma once
#include "compat/defs.hpp"
#include "scenery.hpp"
#include "serialize/anim.hpp"
#include <array>
#include <string>
#include <Magnum/Math/Vector2.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

struct anim_atlas final
{
    using anim_info = Serialize::anim;
    using anim_group = Serialize::anim_group;
    using anim_frame = Serialize::anim_frame;
    using texcoords = std::array<Vector2, 4>;
    using quad = std::array<Vector3, 4>;

    anim_atlas() noexcept;
    anim_atlas(StringView name, const ImageView2D& tex, anim_info info) noexcept;
    ~anim_atlas() noexcept;

    anim_atlas(anim_atlas&&) noexcept;
    anim_atlas& operator=(anim_atlas&&) noexcept;

    StringView name() const noexcept;
    GL::Texture2D& texture() noexcept;
    const anim_info& info() const noexcept;

    const anim_group& group(rotation r) const noexcept;
    const anim_frame& frame(rotation r, std::size_t frame) const noexcept;
    texcoords texcoords_for_frame(rotation r, std::size_t frame) const noexcept;
    quad frame_quad(const Vector3& center, rotation r, std::size_t frame) const noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(anim_atlas);

private:
    GL::Texture2D _tex;
    std::string _name;
    anim_info _info;
    std::array<std::uint8_t, (std::size_t)rotation::COUNT> _group_indices = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };

    static decltype(_group_indices) make_group_indices(const Serialize::anim& anim) noexcept;
    static std::uint8_t rotation_to_index(const anim_info& a, rotation r) noexcept;
};

} // namespace floormat
