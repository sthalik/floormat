#pragma once
#include "compat/LooseQuadtree-impl.h"
#include "src/pass-mode.hpp"
#include <cinttypes>

namespace floormat {

struct collision_bbox final
{
    using BB = loose_quadtree::BoundingBox<std::int16_t>;

    std::int16_t left, top;
    std::uint16_t width, height;
    enum pass_mode pass_mode;

    constexpr collision_bbox();
    constexpr collision_bbox(std::int16_t left, std::int16_t top, std::uint16_t width, std::uint16_t height, enum pass_mode p) noexcept;
    operator BB() const noexcept;
    bool intersects(BB bb) const noexcept;
    bool intersects(collision_bbox bb) const noexcept;
};

constexpr collision_bbox::collision_bbox() :
    left{0}, top{0}, width{0}, height{0}, pass_mode{pass_mode::pass}
{}

constexpr collision_bbox::collision_bbox(std::int16_t left, std::int16_t top, std::uint16_t width, std::uint16_t height, enum pass_mode p) noexcept :
    left{left}, top{top}, width{width}, height{height}, pass_mode{p}
{}

struct collision_bb_extractor final
{
    using BB = loose_quadtree::BoundingBox<std::int16_t>;
    static void ExtractBoundingBox(const collision_bbox* object, BB* bbox);
};

struct collision_iterator final
{
    using Query = typename loose_quadtree::LooseQuadtree<std::int16_t, collision_bbox, collision_bb_extractor>::Query;

    explicit collision_iterator() noexcept;
    explicit collision_iterator(Query* q) noexcept;
    ~collision_iterator() noexcept;
    collision_iterator& operator++() noexcept;
    const collision_bbox* operator++(int) noexcept;
    const collision_bbox& operator*() const noexcept;
    const collision_bbox* operator->() const noexcept;
    bool operator==(const collision_iterator& other) const noexcept;
    operator bool() const noexcept;

private:
    Query* q;
};

struct collision_query
{
    using Query = typename loose_quadtree::LooseQuadtree<std::int16_t, collision_bbox, collision_bb_extractor>::Query;
    collision_query(Query&& q) noexcept;
    collision_iterator begin() noexcept;
    static collision_iterator end() noexcept;

private:
    Query q;
};

} // namespace floormat
