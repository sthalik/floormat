#pragma once
#include "compat/borrowed-ptr.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include "wall-defs.hpp"
#include "compat/defs.hpp"
#include "compat/array-size.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Texture.h>

namespace floormat { class wall_atlas; }

namespace floormat::Wall {

uint8_t direction_index_from_name(StringView s) noexcept(false);
StringView direction_index_to_name(size_t i) noexcept(false);

struct Frame
{
    Vector2ui offset = { (unsigned)-1, (unsigned)-1 }, size;

    bool operator==(const Frame&) const noexcept;
};

struct Group
{
    uint32_t index = (uint32_t)-1, count = 0;
    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    uint8_t from_rotation = (uint8_t)-1;
    bool mirrored     : 1 = false,
         default_tint : 1 = true,
         is_defined   : 1 = false;

    bool operator==(const Group&) const noexcept;
};

struct Direction
{
    using memfn_ptr = Group Direction::*;
    struct member_tuple { StringView name; memfn_ptr member; Group_ tag; };

    Group wall{}, side{}, top{}, corner{};

    const Group& group(Group_ i) const;
    const Group& group(size_t i) const;
    Group& group(Group_ i);
    Group& group(size_t i);

    bool operator==(const Direction&) const noexcept;

    static constexpr inline member_tuple groups[] = {
        { "wall"_s,     &Direction::wall,     Group_::wall      },
        { "side"_s,     &Direction::side,     Group_::side      },
        { "top"_s,      &Direction::top,      Group_::top       },
        { "corner"_s,   &Direction::corner,   Group_::corner    },
    };
    static_assert(array_size(groups) == (size_t)Group_::COUNT);

    static constexpr inline member_tuple groups_for_draw[] = {
        { "wall"_s,     &Direction::wall,     Group_::wall      },
        { "side"_s,     &Direction::side,     Group_::side      },
        { "top"_s,      &Direction::top,      Group_::top       },
    };
};

struct Info
{
    String name;
    unsigned depth = 0;
    pass_mode passability = pass_mode::blocked;

    bool operator==(const Info&) const noexcept;
};

struct DirArrayIndex {
    uint8_t val = (uint8_t)-1;
    explicit operator bool() const { return val != (uint8_t)-1; }

    bool operator==(const DirArrayIndex&) const noexcept;
};

void resolve_wall_rotations(Array<Wall::Direction>& dirs, const std::array<DirArrayIndex, Direction_COUNT>& map) noexcept(false);

} // namespace floormat::Wall

namespace floormat {

struct wall_atlas_def final
{
    bool operator==(const wall_atlas_def&) const noexcept;

    Wall::Info header;
    Array<Wall::Frame> frames;
    Array<Wall::Direction> direction_array;
    std::array<Wall::DirArrayIndex, Wall::Direction_COUNT> direction_map;
    std::array<bool, Wall::Direction_COUNT> direction_mask{};

    static wall_atlas_def deserialize(StringView filename);
    void serialize(StringView filename) const;
    static void serialize(StringView filename, const Wall::Info& header, ArrayView<const Wall::Frame> frames,
                          ArrayView<const Wall::Direction> dir_array,
                          std::array<Wall::DirArrayIndex, Wall::Direction_COUNT> dir_map);
};

class wall_atlas final : public bptr_base
{
    using Frame = Wall::Frame;
    using Group = Wall::Group;
    using Direction_ = Wall::Direction_;
    using Direction = Wall::Direction;
    using Info = Wall::Info;
    using Group_ = Wall::Group_;
    using DirArrayIndex = Wall::DirArrayIndex;

    Array<Direction> _dir_array;
    Array<Frame> _frame_array;
    Info _info;
    String _path;
    Vector2ui _image_size;
    GL::Texture2D _texture;
    std::array<DirArrayIndex, Wall::Direction_COUNT> _direction_map;

    Direction* get_Direction(Direction_ num) const;

public:
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(wall_atlas);
    wall_atlas() noexcept;
    ~wall_atlas() noexcept override;
    wall_atlas(wall_atlas_def def, String path, const ImageView2D& img);
    void serialize(StringView filename) const;

    const Group* group(Direction_ dir, Group_ group) const;
    const Group* group(size_t dir, size_t group) const;
    const Group* group(size_t dir, Group_ tag) const;
    static const Group* group(const Direction& dir, Group_ group);
    const Direction* direction(size_t dir) const;
    const Direction* direction(Direction_ dir) const;
    const Direction& calc_direction(Direction_ dir) const;
    uint8_t direction_count() const;
    ArrayView<const Frame> frames(const Group& a) const;
    ArrayView<const Frame> frames(Direction_ dir, Group_ g) const noexcept(false);
    ArrayView<const Frame> raw_frame_array() const;

    unsigned depth() const { return _info.depth; } // todo use it in more places
    const Info& info() const { return _info; }
    StringView name() const { return _info.name; }
    //StringView path() const { return _path; }

    GL::Texture2D& texture();
    Vector2ui image_size() const;

    static size_t enum_to_index(enum rotation x);
    static Vector2ui expected_size(unsigned depth, Group_ group);

    struct dir_tuple
    {
        StringView name;
        Direction_ direction;
    };

    static constexpr dir_tuple directions[] = {
        { "n"_s, Direction_::N },
        { "w"_s, Direction_::W },
    };
};

} // namespace floormat
