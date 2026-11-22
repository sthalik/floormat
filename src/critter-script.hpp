#pragma once
#include "script.hpp"
#include "object-id.hpp"
#include <cr/Pointer.h>

namespace floormat {

struct critter;
struct Ns;
class astar;
class order_buffer;
struct order;

struct point;
struct path_search_result;

struct critter_script : base_script
{
    constexpr critter_script() noexcept = default;
    ~critter_script() noexcept override;

    virtual void on_init(const bptr<critter>& c) = 0;
    // Called once per frame while nothing writes the world. Decides what c does this frame, may change only this
    // script's own state, and gives the orders that act on the decision.
    virtual void on_think(critter& c, const Ns& dt, order_buffer& orders) = 0;
    // Runs one order that on_think() gave in this frame. May change c and the world, and may clear the script.
    virtual void on_order(const bptr<critter>& c, usize& i, const Ns& dt, const order& o) = 0;
    virtual void on_destroy(const bptr<critter>& c, script_destroy_reason reason) = 0;
    virtual void delete_self() = 0;

    object_type type() const override;

    static Pointer<critter_script> make_walk_script(point to);
    static Pointer<critter_script> make_walk_script(path_search_result path);
    // Hides from the threat and peeks out until it sees the critter's gun, gun_offset px to its right
    // (default half its width). A must outlive the script's updates.
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, u32 div_size);
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, u32 div_size, int gun_offset);
};

} // namespace floormat
