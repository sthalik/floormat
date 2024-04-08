#pragma once
#include "compat/defs.hpp"
#include <memory>

namespace floormat
{
struct object;
struct Ns;

enum class script_lifecycle : uint8_t
{
    no_init, initializing, created, destroying, torn_down, COUNT,
};

struct base_script
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(base_script);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(base_script);

    base_script() noexcept;
    virtual ~base_script() noexcept;

    static StringView state_name(script_lifecycle x);
};

enum class script_destroy_reason : uint8_t
{
    quit,       // game is being shut down
    kill,       // object is being deleted from the gameworld
    unassign,   // script is unassigned from object
    COUNT,
};

template<typename S, typename Obj>
class Script final
{
    S* ptr;
    script_lifecycle state;
    void _assert_state(script_lifecycle s, const char* file, int line);

public:
    Script();
    ~Script() noexcept;

    // [no-init]        -> do_create()              -> [initializing]
    // [initializing]   -> do_initialize()          -> [created]
    // [created]        -> call()                   -> [created]
    // [created]        -> do_reassign()            -> [no-init]
    // [created]        -> do_destroy_pre()         -> [destroying]
    // [destroying]     -> do_finish_destroy()      -> [torn-down]
    // [torn-down]      -> do_ensure_torn_down()    -> [torn-down]
    // *                -> do_error_unwind()        -> [torn-down]
    // [torn-down]      -> ~script()

    S* operator->();

    void do_create(S* ptr);
    void do_initialize(const std::shared_ptr<Obj>& obj);
    void do_reassign(S* ptr, const std::shared_ptr<Obj>& obj);
    void do_destroy_1(const std::shared_ptr<Obj>& obj);
    void do_destroy_pre(const std::shared_ptr<Obj>& obj);
    void do_finish_destroy();
    void do_ensure_torn_down();
    void do_error_unwind();
};

} // namespace floormat
