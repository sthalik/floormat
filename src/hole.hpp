#pragma once
#include "object.hpp"
#include <array>
#include <Magnum/DimensionTraits.h>

namespace floormat {

struct hole_proto final : object_proto
{
    ~hole_proto() noexcept override;
    hole_proto();
    hole_proto(const hole_proto&);
    hole_proto& operator=(const hole_proto&);
    hole_proto(hole_proto&&) noexcept;
    hole_proto& operator=(hole_proto&&) noexcept;
    bool operator==(const object_proto& proto) const override;

    uint8_t height = tile_size_z, z_offset = 0;
    struct Flags
    {
        bool enabled : 1 = true;

        bool operator==(const Flags&) const noexcept;
    } flags;
};

struct hole final : object
{
    const uint8_t height = tile_size_z, z_offset = 0;
    const hole_proto::Flags flags;

    hole(object_id id, class chunk& c, const hole_proto& proto);
    ~hole() noexcept override;

    int32_t depth_offset() const override;
    object_type type() const noexcept override;
    void update(const bptr<object>& ptr, size_t& i, const Ns& dt) override;
    bool is_dynamic() const override;
    bool updates_passability() const override;
    bool updates_walls() const override;
    bool is_virtual() const override;

    void set_height(uint8_t height);
    void set_z_offset(uint8_t z);
    void set_enabled(bool value);

    explicit operator hole_proto() const;

    friend class world;

private:
    void mark_neighbor_chunks_modified() override;
};

template<> struct object_type_<struct hole> : std::integral_constant<object_type, object_type::hole> {};
template<> struct object_type_<struct hole_proto> : std::integral_constant<object_type, object_type::hole> {};

} // namespace floormat
