#pragma once
#include "script.hpp"
#include <cr/Pointer.h>

namespace floormat {

struct critter;
struct Ns;

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
};

} // namespace floormat
