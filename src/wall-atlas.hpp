#pragma once
#include "compat/defs.hpp"
#include "compat/function2.fwd.hpp"
#include "src/rotation.hpp"
#include <array>
#include <memory>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

struct wall_atlas;

struct wall_frame
{
    Vector2ui offset = { (unsigned)-1, (unsigned)-1 };
};

struct wall_frames
{
    bool is_empty() const noexcept;
    ArrayView<const wall_frame> items(const wall_atlas& a) const;

    uint32_t index = (uint32_t)-1, count = 0;

    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    Color3 tint_add;
    uint8_t from_rotation = (uint8_t)-1;
    bool mirrored         : 1 = false,
         use_default_tint : 1 = true;
};

struct wall_frame_set
{
    enum class type : uint8_t { wall = 1, overlay, side, top, corner_L, corner_R, };

    bool is_empty() const noexcept;

    void visit(const fu2::function_view<bool(StringView, const wall_frames&, type) const>& fun) const&;
    void visit(const fu2::function_view<bool(StringView, wall_frames&, type) const>& fun) &;

    wall_frames wall, overlay, side, top;
    wall_frames corner_L, corner_R;
};

struct wall_info
{
    String name = "(unnamed)"_s;
    unsigned depth = 0;
};

struct wall_atlas_def
{
    wall_info info;
    std::unique_ptr<wall_frame_set[]> framesets;
    std::array<uint8_t, 4> frameset_indexes = {255, 255, 255, 255};
    uint8_t frameset_count = 0;

    Array<wall_frame> array;
    uint32_t frame_count = 0;
};

struct wall_atlas final
{
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(wall_atlas);
    wall_atlas();
    ~wall_atlas() noexcept;
    wall_atlas(wall_info info, const ImageView2D& image,
               Array<wall_frame> frames,
               std::unique_ptr<wall_frame_set[]> framesets,
               uint8_t frameset_count,
               std::array<uint8_t, 4> frameset_indexes);

    static size_t enum_to_index(enum rotation x);
    const wall_frame_set& frameset(size_t i) const;
    const wall_frame_set& frameset(enum rotation r) const;
    ArrayView<const wall_frame> frame_array() const;
    StringView name() const;

private:
    std::unique_ptr<wall_frame_set[]> _framesets;
    Array<wall_frame> _frame_array;
    wall_info _info;
    GL::Texture2D _texture;
    std::array<uint8_t, 4> _frameset_indexes;
    uint8_t _frameset_count = 0;
};

} // namespace floormat
