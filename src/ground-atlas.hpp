#pragma once
#include "src/pass-mode.hpp"
#include "src/quads.hpp"
#include <array>
#include <memory>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

class ground_atlas final
{
    using quad = Quads::quad;
    using texcoords = std::array<Vector2, 4>;

    static std::unique_ptr<const texcoords[]> make_texcoords_array(Vector2ui pixel_size, Vector2ub tile_count);
    static texcoords make_texcoords(Vector2ui pixel_size, Vector2ub tile_count, size_t i);

    std::unique_ptr<const texcoords[]> texcoords_;
    GL::Texture2D tex_;
    String path_, name_;
    Vector2ui size_;
    Vector2ub dims_;
    enum pass_mode passability;

public:
    ground_atlas(StringView path, StringView name, const ImageView2D& img, Vector2ub tile_count, enum pass_mode pass_mode);
    texcoords texcoords_for_id(size_t id) const;
    [[maybe_unused]] Vector2ui pixel_size() const { return size_; }
    size_t num_tiles() const;
    Vector2ub num_tiles2() const { return dims_; }
    GL::Texture2D& texture() { return tex_; }
    StringView name() const { return name_; }
    enum pass_mode pass_mode() const;

    static constexpr enum pass_mode default_pass_mode = pass_mode::pass;
};



} // namespace floormat
