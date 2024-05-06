#pragma once
#include "script-enums.hpp"
#include "compat/defs.hpp"
#include "src/object-type.hpp"
#include <memory>

namespace floormat
{
struct object;
struct Ns;

struct base_script
{
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(base_script);

    virtual StringView name() const = 0;
    virtual const void* id() const = 0;
    virtual object_type type() const = 0;

    constexpr base_script() noexcept = default;
    virtual ~base_script() noexcept;

    static StringView state_name(script_lifecycle x);
    static void _assert_state(script_lifecycle old_state, script_lifecycle s, const char* file, int line);
};

template<typename S, typename Obj>
class Script final
{
    S* ptr;
    script_lifecycle _state;
    static S* make_empty();

public:
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Script);

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
    explicit operator bool() const;

    void do_create(S* ptr);
    void do_create(Pointer<S> ptr);
    void do_initialize(const std::shared_ptr<Obj>& obj);
    void do_reassign(S* ptr, const std::shared_ptr<Obj>& obj);
    void do_reassign(Pointer<S> ptr, const std::shared_ptr<Obj>& obj);
    void do_clear(const std::shared_ptr<Obj>& obj);
    void do_destroy_pre(const std::shared_ptr<Obj>& obj, script_destroy_reason r);
    void do_finish_destroy();
    void do_ensure_torn_down();
    void do_error_unwind();
};

} // namespace floormat
