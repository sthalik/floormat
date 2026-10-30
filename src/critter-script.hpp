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
    // Hides from the threat and peeks out until it sees the critter's gun, gun_offset px to its right
    // (default half its width). A must outlive the script's updates.
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, uint32_t div_size);
    static Pointer<critter_script> make_cover_script(astar& A, object_id threat, uint32_t div_size, int gun_offset);
};

} // namespace floormat
