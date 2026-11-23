#pragma once
#include "object.hpp"

namespace floormat {

struct hole_proto final : object_proto
{
    ~hole_proto() noexcept override;
    hole_proto();
    explicit hole_proto(object_proto base);
    hole_proto(const hole_proto&);
    hole_proto& operator=(const hole_proto&);
    hole_proto(hole_proto&&) noexcept;
    hole_proto& operator=(hole_proto&&) noexcept;
    bool operator==(const object_proto& proto) const override;

    u8 height = tile_size_z, z_offset = 0;
    struct Flags
    {
        bool enabled : 1 = true;

        bool operator==(const Flags&) const noexcept;
    } flags;
};

struct hole final : object
{
    const u8 height = tile_size_z, z_offset = 0;
    const hole_proto::Flags flags;

    hole(object_id id, class chunk& c, const hole_proto& proto);
    ~hole() noexcept override;

    i32 depth_offset() const;
    void update(const bptr<object>& ptr, usize& i, const Ns& dt);

    void set_height(u8 height);
    void set_z_offset(u8 z);
    void set_enabled(bool value);

    explicit operator hole_proto() const;

    friend class world;
    friend struct object;

private:
    void mark_modified_with_neighbors();
};

template<> struct object_type_<struct hole> : std::integral_constant<object_type, object_type::hole> {};
template<> struct object_type_<struct hole_proto> : std::integral_constant<object_type, object_type::hole> {};

} // namespace floormat
