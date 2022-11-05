#pragma once
#include "compat/defs.hpp"
#include "serialize/anim.hpp"
#include <Corrade/Containers/String.h>
#include <Magnum/GL/Texture.h>


namespace floormat {

struct anim_atlas final
{
    anim_atlas();
    anim_atlas(StringView name, GL::Texture2D&& tex, Serialize::anim metadata) noexcept;
    ~anim_atlas() noexcept;

    anim_atlas(anim_atlas&&) noexcept;
    anim_atlas& operator=(anim_atlas&&) noexcept;

    StringView name() const noexcept;
    GL::Texture2D texture() noexcept;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(anim_atlas);

private:
    GL::Texture2D _tex;
    String _name;
    Serialize::anim _anim;
};

} // namespace floormat
