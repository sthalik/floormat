#pragma once
#include "compat/defs.hpp"
#include "src/rotation.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Texture.h>

namespace floormat { class wall_atlas; }

namespace floormat::Wall {

struct Frame
{
    Vector2ui offset = { (unsigned)-1, (unsigned)-1 };
};

struct Group
{
    uint32_t index = (uint32_t)-1, count = 0;
    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    Color3 tint_add;
    uint8_t from_rotation = (uint8_t)-1;
    bool mirrored                : 1 = false,
         default_tint            : 1 = true,
         _default_tint_specified : 1 = false;

    explicit operator bool() const noexcept { return !is_empty(); }
    bool is_empty() const noexcept { return count == 0; }
};

enum class Tag : uint8_t { wall, overlay, side, top, corner_L, corner_R, COUNT };

enum class Direction_ : uint8_t { N, E, S, W, COUNT };

struct Direction
{
    using memfn_ptr = Group Direction::*;
    struct member_tuple { StringView str; memfn_ptr member; Tag tag; };

    explicit operator bool() const noexcept { return !is_empty(); }
    bool is_empty() const noexcept;

    Group wall, overlay, side, top;
    Group corner_L, corner_R;

    static constexpr inline member_tuple members[] = {
        { "wall"_s,     &Direction::wall,     Tag::wall      },
        { "overlay"_s,  &Direction::overlay,  Tag::overlay   },
        { "side"_s,     &Direction::side,     Tag::side      },
        { "top"_s,      &Direction::top,      Tag::top       },
        { "corner-L"_s, &Direction::corner_L, Tag::corner_L, },
        { "corner-R"_s, &Direction::corner_R, Tag::corner_R, },
    };
    static_assert(arraySize(members) == (size_t)Tag::COUNT);
};

struct Info
{
    String name = "(unnamed)"_s;
    unsigned depth = 0;
};

struct DirArrayIndex {
    std::uint8_t val = (uint8_t)-1;
    operator bool() const { return val == (uint8_t)-1; }
};

} // namespace floormat::Wall

namespace floormat {

class wall_atlas final
{
    using Frame = Wall::Frame;
    using Group = Wall::Group;
    using Direction_ = Wall::Direction_;
    using Direction = Wall::Direction;
    using Info = Wall::Info;
    using Tag = Wall::Tag;
    using DirArrayIndex = Wall::DirArrayIndex;

    Array<Direction> _dir_array;
    Array<Frame> _frame_array;
    Info _info;
    GL::Texture2D _texture;
    std::array<DirArrayIndex, 4> _direction_to_Direction_array_index;

    Direction* get_Direction(Direction_ num) const;

public:
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(wall_atlas);
    wall_atlas() noexcept;
    ~wall_atlas() noexcept;

    wall_atlas(Info info, const ImageView2D& image,
               Array<Frame> frames, Array<Direction> directions,
               std::array<DirArrayIndex, 4> direction_to_DirArrayIndex);
    StringView name() const;
    uint8_t direction_count() const;

    const Group* group(size_t dir, Tag tag) const;
    const Group* group(const Direction& dir, Tag tag) const;
    const Group* group(const Direction* dir, Tag tag) const;
    const Direction* direction(size_t dir) const;
    ArrayView<const Frame> frames(const Group& a) const;
    ArrayView<const Frame> raw_frame_array() const;

    static size_t enum_to_index(enum rotation x);
};


} // namespace floormat
