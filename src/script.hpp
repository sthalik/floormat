#pragma once
#include "script-enums.hpp"
#include "compat/defs.hpp"
#include <memory>

namespace floormat
{
struct object;
struct Ns;

struct base_script
{
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(base_script);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(base_script);

    base_script() noexcept;
    virtual ~base_script() noexcept;

    static StringView state_name(script_lifecycle x);
};

template<typename S, typename Obj>
class Script final
{
    S* ptr;
    script_lifecycle _state;
    void _assert_state(script_lifecycle s, const char* file, int line);
    static S* make_empty();

public:
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(Script);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(Script);

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
    // [no-init]        -> ~script() // for tests

    script_lifecycle state() const;
    S* operator->();

    void do_create(S* ptr);
    void do_initialize(const std::shared_ptr<Obj>& obj);
    void do_reassign(S* ptr, const std::shared_ptr<Obj>& obj);
    void do_destroy_pre(const std::shared_ptr<Obj>& obj, script_destroy_reason r);
    void do_finish_destroy();
    void do_ensure_torn_down();
    void do_error_unwind();
};

} // namespace floormat
