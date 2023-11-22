#pragma once
#include "compat/defs.hpp"
#include "src/rotation.hpp"
#include "src/pass-mode.hpp"
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

    bool operator==(const Frame&) const noexcept;
};

struct Group
{
    uint32_t index = (uint32_t)-1, count = 0;
    Vector2ui pixel_size;
    Color4 tint_mult{1,1,1,1};
    Color3 tint_add;
    uint8_t from_rotation = (uint8_t)-1; // applies only to images
    bool mirrored                : 1 = false,
         default_tint            : 1 = true;

    explicit operator bool() const noexcept { return !is_empty(); }
    bool is_empty() const noexcept { return count == 0; }

    bool operator==(const Group&) const noexcept;
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
    pass_mode passability = pass_mode::blocked;

    static constexpr inline member_tuple groups[] = {
        { "wall"_s,     &Direction::wall,     Tag::wall      },
        { "overlay"_s,  &Direction::overlay,  Tag::overlay   },
        { "side"_s,     &Direction::side,     Tag::side      },
        { "top"_s,      &Direction::top,      Tag::top       },
        { "corner-L"_s, &Direction::corner_L, Tag::corner_L, },
        { "corner-R"_s, &Direction::corner_R, Tag::corner_R, },
    };
    static_assert(arraySize(groups) == (size_t)Tag::COUNT);

    bool operator==(const Direction&) const noexcept;
};

struct Info
{
    String name;
    unsigned depth = 0;

    bool operator==(const Info&) const noexcept;
};

struct DirArrayIndex {
    std::uint8_t val = (uint8_t)-1;
    operator bool() const { return val != (uint8_t)-1; }

    bool operator==(const DirArrayIndex&) const noexcept;
};

} // namespace floormat::Wall

namespace floormat {

struct wall_atlas_def final
{
private:
    using Frame = Wall::Frame;
    using Direction = Wall::Direction;
    using Info = Wall::Info;
    using DirArrayIndex = Wall::DirArrayIndex;

public:
    bool operator==(const wall_atlas_def&) const noexcept;

    Info header;
    Array<Frame> frames;
    Array<Direction> direction_array;
    std::array<DirArrayIndex, 4> direction_map;

    static wall_atlas_def deserialize(StringView filename);
    void serialize(StringView filename) const;
    static void serialize(StringView filename, const Info& header, ArrayView<const Frame> frames,
                          ArrayView<const Direction> dir_array, std::array<DirArrayIndex, 4> dir_map);
};

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

    const Group* group(size_t dir, Tag tag) const;
    const Group* group(const Direction& dir, Tag tag) const;
    const Group* group(const Direction* dir, Tag tag) const;
    const Direction* direction(size_t dir) const;
    uint8_t direction_count() const;
    ArrayView<const Frame> frames(const Group& a) const;
    ArrayView<const Frame> raw_frame_array() const;

    const Info& info() const { return _info; }
    StringView name() const { return _info.name; }
    //StringView path() const { return _path; }

    GL::Texture2D& texture();

    static size_t enum_to_index(enum rotation x);
    static void validate(const wall_atlas& a, const ImageView2D& img) noexcept(false);
    static Vector2i expected_size(int depth, Tag group);

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
