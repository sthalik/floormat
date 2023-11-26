#pragma once
#include "compat/defs.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
#include <array>
#include <bitset>
#include <vector>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/Texture.h>

namespace floormat { class wall_atlas; }

namespace floormat::Wall {

struct Frame
{
    Vector2ui offset = { (unsigned)-1, (unsigned)-1 };

    bool operator==(const Frame&) const noexcept;
};

struct Group
{
    uint32_t index = (uint32_t)-1, count = 0;
    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    Color3 tint_add;
    uint8_t from_rotation = (uint8_t)-1; // applies only to images
    bool mirrored     : 1 = false,
         default_tint : 1 = true,
         is_defined   : 1 = false;

    //bool is_empty() const noexcept { return count == 0; }

    bool operator==(const Group&) const noexcept;
};

enum class Group_ : uint8_t { wall, overlay, side, top, corner_L, corner_R, COUNT };

enum class Direction_ : uint8_t { N, E, S, W, COUNT };

struct Direction
{
    using memfn_ptr = Group Direction::*;
    struct member_tuple { StringView str; memfn_ptr member; Group_ tag; };

    Group wall{}, overlay{}, side{}, top{};
    Group corner_L{}, corner_R{};
    pass_mode passability = pass_mode::blocked;

    const Group& group(Group_ i) const;
    const Group& group(size_t i) const;
    Group& group(Group_ i);
    Group& group(size_t i);

    static constexpr inline member_tuple groups[] = {
        { "wall"_s,     &Direction::wall,     Group_::wall      },
        { "overlay"_s,  &Direction::overlay,  Group_::overlay   },
        { "side"_s,     &Direction::side,     Group_::side      },
        { "top"_s,      &Direction::top,      Group_::top       },
        { "corner-L"_s, &Direction::corner_L, Group_::corner_L, },
        { "corner-R"_s, &Direction::corner_R, Group_::corner_R, },
    };
    static_assert(std::size(groups) == (size_t)Group_::COUNT);

    bool operator==(const Direction&) const noexcept;
};

struct Info
{
    String name;
    unsigned depth = 0;

    bool operator==(const Info&) const noexcept;
};

struct DirArrayIndex {
    uint8_t val = (uint8_t)-1;
    operator bool() const { return val != (uint8_t)-1; }

    bool operator==(const DirArrayIndex&) const noexcept;
};

} // namespace floormat::Wall

namespace floormat {

constexpr inline auto Direction_COUNT = (size_t)Wall::Direction_::COUNT;

struct wall_atlas_def final
{
    bool operator==(const wall_atlas_def&) const noexcept;

    Wall::Info header;
    std::vector<Wall::Frame> frames;
    std::vector<Wall::Direction> direction_array;
    std::array<Wall::DirArrayIndex, Direction_COUNT> direction_map;
    std::bitset<Direction_COUNT> direction_mask{0};

    static wall_atlas_def deserialize(StringView filename);
    void serialize(StringView filename) const;
    static void serialize(StringView filename, const Wall::Info& header, ArrayView<const Wall::Frame> frames,
                          ArrayView<const Wall::Direction> dir_array,
                          std::array<Wall::DirArrayIndex, Direction_COUNT> dir_map);
};

class wall_atlas final
{
    using Frame = Wall::Frame;
    using Group = Wall::Group;
    using Direction_ = Wall::Direction_;
    using Direction = Wall::Direction;
    using Info = Wall::Info;
    using Group_ = Wall::Group_;
    using DirArrayIndex = Wall::DirArrayIndex;

    std::vector<Direction> _dir_array;
    std::vector<Frame> _frame_array;
    Info _info;
    String _path;
    GL::Texture2D _texture{NoCreate};
    std::array<DirArrayIndex, 4> _direction_map;

    Direction* get_Direction(Direction_ num) const;

public:
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(wall_atlas);
    wall_atlas() noexcept;
    ~wall_atlas() noexcept;
    wall_atlas(wall_atlas_def def, String path, const ImageView2D& img);
    void serialize(StringView filename) const;

    const Group* group(Direction_ dir, Group_ group) const;
    const Group* group(size_t dir, size_t group) const;
    const Group* group(size_t dir, Group_ tag) const;
    const Group* group(const Direction& dir, Group_ group) const;
    const Direction* direction(size_t dir) const;
    uint8_t direction_count() const;
    ArrayView<const Frame> frames(const Group& a) const;
    ArrayView<const Frame> raw_frame_array() const;

    const Info& info() const { return _info; }
    StringView name() const { return _info.name; }
    //StringView path() const { return _path; }

    GL::Texture2D& texture();

    static size_t enum_to_index(enum rotation x);
    //static void validate(const wall_atlas& a, const ImageView2D& img) noexcept(false);
    static Vector2i expected_size(unsigned depth, Group_ group);

    struct dir_tuple
    {
        StringView name;
        Direction_ direction;
    };

    static constexpr dir_tuple directions[] = {
        { "n"_s, Direction_::N },
        { "e"_s, Direction_::E },
        { "s"_s, Direction_::S },
        { "w"_s, Direction_::W },
    };
};

} // namespace floormat
