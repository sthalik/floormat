#pragma once
#include "object.hpp"
#include <array>

namespace floormat {

struct hole_proto final : object_proto
{
    ~hole_proto() noexcept override;
    hole_proto();
    hole_proto(const hole_proto&);
    hole_proto& operator=(const hole_proto&);
    hole_proto(hole_proto&&) noexcept;
    hole_proto& operator=(hole_proto&&) noexcept;
    bool operator==(const hole_proto&) const;

    uint8_t height = 0;
    bool on_render  : 1 = true;
    bool on_physics : 1 = true;
    bool is_wall    : 1 = false;
};

struct hole final : object
{
    uint8_t _height = 0;
    const bool on_render  : 1 = true;
    const bool on_physics : 1 = true;
    const bool is_wall    : 1 = false;

    hole(object_id id, class chunk& c, const hole_proto& proto);
    ~hole() noexcept override;

    Vector2 ordinal_offset(Vector2b offset) const override;
    float depth_offset() const override;
    object_type type() const noexcept override;
    void update(const std::shared_ptr<object>& ptr, size_t& i, const Ns& dt) override;
    bool is_dynamic() const override;
    bool is_virtual() const override;

    explicit operator hole_proto() const;

    friend class world;
};

struct cut_rectangle_result
{
    struct bbox { Vector2i position; Vector2ub bbox_size; };
    struct rect { Vector2i min, max; };

    uint8_t size = 0;
    std::array<rect, 8> array;

    operator ArrayView<const bbox>() const;
};

cut_rectangle_result cut_rectangle(cut_rectangle_result::bbox input, cut_rectangle_result::bbox hole);

} // namespace floormat
