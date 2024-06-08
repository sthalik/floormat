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

    struct flags
    {
        bool operator==(const flags&) const;

        bool on_render  : 1 = true;
        bool on_physics : 1 = true;
        bool enabled    : 1 = true;
        bool is_wall    : 1 = false;
    };

    uint8_t height = 0, z_offset = tile_size_z/2;
    struct flags flags;
};

struct hole final : object
{
    const uint8_t height = 0, z_offset = tile_size_z/2;
    const struct hole_proto::flags flags;

    hole(object_id id, class chunk& c, const hole_proto& proto);
    ~hole() noexcept override;

    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    object_type type() const noexcept override;
    void update(const std::shared_ptr<object>& ptr, size_t& i, const Ns& dt) override;
    bool is_dynamic() const override;
    bool updates_passability() const override;
    bool is_virtual() const override;

    void set_height(uint8_t height);
    void set_z_offset(uint8_t z);
    void set_enabled(bool on_render, bool on_physics, bool on_both);

    explicit operator hole_proto() const;

    friend class world;

private:
    void maybe_mark_neighbor_chunks_modified() override;
};

template<typename T>
struct CutResult
{
    using Vec2 = VectorTypeFor<2, T>;
    struct bbox { Vec2 position; Vector2ub bbox_size; };
    struct rect { Vec2 min, max; };

    static CutResult cut(bbox input, bbox hole);
    static CutResult cut(Vec2 r0, Vec2 r1, Vec2 h0, Vec2 h1);

    std::array<rect, 8> array;
    uint8_t size = 0;
    bool found = false;
};

} // namespace floormat
