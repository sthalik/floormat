#pragma once
#include "script.hpp"
#include "object-id.hpp"
#include <cr/Pointer.h>

namespace floormat {

struct critter;
struct Ns;
class astar;

struct point;
struct path_search_result;

struct critter_script : base_script
{
    constexpr critter_script() noexcept = default;
    ~critter_script() noexcept override;

    virtual void on_init(const bptr<critter>& c) = 0;
    virtual void on_update(const bptr<critter>& c, size_t& i, const Ns& dt) = 0;
    virtual void on_destroy(const bptr<critter>& c, script_destroy_reason reason) = 0;
    virtual void delete_self() = 0;

    object_type type() const override;

    static Pointer<critter_script> make_walk_script(point to);
    static Pointer<critter_script> make_walk_script(path_search_result path);
    // Hides from the threat critter, then steps out and back every 2 s. It looks for cover again
    // after the threat moves. A must outlive the script's updates: the editor's astar is destroyed
    // before the world. It steps out until the threat sees its gun, which is to its right as it faces
    // the threat, by half its width along the cover's face or by gun_offset px. A negative gun_offset
    // puts the gun on the left. It turns to the nearest of the 8 rotations toward the threat when it
    // has stepped out, and again when the threat moves and the spot to step out to stays the same.
    // It keeps its rotation when its box turned that way would overlap a collider.
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, uint32_t div_size);
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, uint32_t div_size, int gun_offset);
};

} // namespace floormat
