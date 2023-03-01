#pragma once
#include "pass-mode.hpp"
#include "tile-defs.hpp"
#include "rotation.hpp"
#include <cstdint>
#include <memory>
#include <type_traits>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Magnum.h>

namespace floormat {

struct chunk;
struct anim_atlas;

enum class scenery_type : std::uint8_t {
    none, generic, door,
};

struct scenery final
{
    struct none_tag_t final {};
    struct generic_tag_t final {};
    struct door_tag_t final {};

    static constexpr auto none    = none_tag_t{};
    static constexpr auto generic = generic_tag_t{};
    static constexpr auto door    = door_tag_t{};

    using frame_t = std::uint16_t;

    std::uint16_t delta = 0;
    frame_t frame = 0;
    Vector2b offset, bbox_offset;
    Vector2ub bbox_size{usTILE_SIZE2/2};
    rotation     r           : 3 = rotation::N;
    scenery_type type        : 3 = scenery_type::none;
    pass_mode    passability : 2 = pass_mode::shoot_through;
    std::uint8_t active      : 1 = false;
    std::uint8_t closing     : 1 = false;
    std::uint8_t interactive : 1 = false;

    constexpr scenery() noexcept;
    constexpr scenery(none_tag_t) noexcept;
    scenery(generic_tag_t, const anim_atlas& atlas, rotation r, frame_t frame,
            pass_mode passability, bool active, bool interactive,
            Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);
    scenery(door_tag_t, const anim_atlas& atlas, rotation r, bool is_open,
            Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size);

    bool operator==(const scenery&) const noexcept;
};

constexpr scenery::scenery() noexcept : scenery{scenery::none_tag_t{}} {}
constexpr scenery::scenery(none_tag_t) noexcept {}

struct scenery_proto final
{
    std::shared_ptr<anim_atlas> atlas;
    scenery frame;

    scenery_proto() noexcept;
    scenery_proto(const std::shared_ptr<anim_atlas>& atlas, const scenery& frame) noexcept;
    scenery_proto& operator=(const scenery_proto&) noexcept;
    scenery_proto(const scenery_proto&) noexcept;

    template<typename... Ts>
    scenery_proto(scenery::generic_tag_t, const std::shared_ptr<anim_atlas>& atlas, Ts&&... args) :
        atlas{atlas}, frame{scenery::generic, *atlas, std::forward<Ts>(args)...}
    {}

    template<typename... Ts>
    scenery_proto(scenery::door_tag_t, const std::shared_ptr<anim_atlas>& atlas, Ts&&... args) :
        atlas{atlas}, frame{scenery::door, *atlas, std::forward<Ts>(args)...}
    {}

    operator bool() const noexcept;
};

struct scenery_ref final {
    scenery_ref(struct chunk& c, std::size_t i) noexcept;
    scenery_ref(const scenery_ref&) noexcept;
    scenery_ref(scenery_ref&&) noexcept;
    scenery_ref& operator=(const scenery_proto& proto) noexcept;

    operator scenery_proto() const noexcept;
    operator bool() const noexcept;

    struct chunk& chunk() noexcept;
    std::uint8_t index() const noexcept;

    template<std::size_t N> std::tuple_element_t<N, scenery_ref>& get() & { if constexpr(N == 0) return atlas; else return frame; }
    template<std::size_t N> std::tuple_element_t<N, scenery_ref>& get() && { if constexpr(N == 0) return atlas; else return frame; }

    std::shared_ptr<anim_atlas>& atlas;
    scenery& frame;

    bool can_activate() noexcept;
    bool activate();
    void update(float dt);
    void rotate(rotation r);

private:
    struct chunk* c;
    std::uint8_t idx;
};

} // namespace floormat

template<> struct std::tuple_size<floormat::scenery_ref> final : std::integral_constant<std::size_t, 2> {};
template<> struct std::tuple_element<0, floormat::scenery_ref> final { using type = std::shared_ptr<floormat::anim_atlas>; };
template<> struct std::tuple_element<1, floormat::scenery_ref> final { using type = floormat::scenery; };
