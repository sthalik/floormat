#pragma once
#include "script.hpp"
#include "compat/assert.hpp"
#include <utility>
#include <Corrade/Containers/StringView.h>

// ReSharper disable CppDFAUnreachableCode

namespace floormat::detail_Script {

template<typename S, typename Obj>
concept BaseScript =
requires (S& script, const std::shared_ptr<Obj>& obj, size_t& i, const Ns& ns)
{
    requires std::is_base_of_v<base_script, S>;
    script.on_init(obj);
    script.on_update(obj, i, ns);
    script.on_destroy(obj, script_destroy_reason::COUNT);
    script.delete_self();
    script.~S();
};

template<typename S, typename Obj>
requires requires ()
{
    requires BaseScript<S, Obj>;
    requires std::is_base_of_v<object, Obj>;
}
CORRADE_ALWAYS_INLINE
void concept_check() {}

} // namespace floormat::detail_Script

namespace floormat {

#define FM_ASSERT_SCRIPT_STATE(new_state) (Script<S, Obj>::_assert_state((new_state), __FILE__, __LINE__))

template <typename S, typename Obj> Script<S, Obj>::~Script() noexcept
{
    fm_assert(_state < script_lifecycle::COUNT);
    switch (_state)
    {
    case script_lifecycle::no_init:
    case script_lifecycle::torn_down:
        _state = (script_lifecycle)-1;
        break;
    case script_lifecycle::COUNT:
        std::unreachable();
    case script_lifecycle::created:
    case script_lifecycle::destroying:
    case script_lifecycle::initializing:
        fm_abort("invalid state '%s' in script destructor",
                 base_script::state_name(_state).data());
    }
}

template <typename S, typename Obj>
Script<S, Obj>::Script(): ptr{nullptr}, _state{script_lifecycle::no_init}
{
    detail_Script::concept_check<S, Obj>();
}

template <typename S, typename Obj>
void Script<S, Obj>::_assert_state(script_lifecycle s, const char* file, int line)
{
    if (_state != s)
    {
        fm_EMIT_DEBUG2("fatal: ",
                       "invalid state transition from '%s' to '%s'",
                       base_script::state_name(_state).data(),
                       base_script::state_name(s).data());
        fm_EMIT_DEBUG("", " in %s:%d", file, line);
        fm_EMIT_ABORT();
    }
}

template <typename S, typename Obj> script_lifecycle Script<S, Obj>::state() const { return _state; }

template <typename S, typename Obj>
S* Script<S, Obj>::operator->()
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::created);
    fm_debug_assert(ptr);
    return ptr;
}

template<typename S, typename Obj>
void Script<S, Obj>::do_create(S* p)
{
    fm_assert(p);
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::no_init);
    _state = script_lifecycle::initializing;
    ptr = p;
}

template <typename S, typename Obj>
void Script<S, Obj>::do_initialize(const std::shared_ptr<Obj>& obj)
{
    switch (_state)
    {
    default:
        FM_ASSERT_SCRIPT_STATE(script_lifecycle::initializing);
        std::unreachable();
    case script_lifecycle::no_init:
        ptr = make_empty();
        [[fallthrough]];
    case script_lifecycle::initializing:
        _state = script_lifecycle::created;
        ptr->on_init(obj);
        return;
    }
    std::unreachable();
}

template <typename S, typename Obj>
void Script<S, Obj>::do_reassign(S* p, const std::shared_ptr<Obj>& obj)
{
    fm_assert(p);
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::created);
    fm_debug_assert(ptr);
    ptr->on_destroy(obj, script_destroy_reason::unassign);
    ptr->delete_self();
    ptr = p;
    p->on_init(obj);
}

template <typename S, typename Obj>
void Script<S, Obj>::do_destroy_pre(const std::shared_ptr<Obj>& obj, script_destroy_reason r)
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::created);
    _state = script_lifecycle::destroying;
    ptr->on_destroy(obj, r);
}

template <typename S, typename Obj>
void Script<S, Obj>::do_finish_destroy()
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::destroying);
    _state = script_lifecycle::torn_down;
    ptr->delete_self();
    ptr = nullptr;
}

template <typename S, typename Obj>
void Script<S, Obj>::do_ensure_torn_down()
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::torn_down);
}

template <typename S, typename Obj>
void Script<S, Obj>::do_error_unwind()
{
    fm_assert(_state < script_lifecycle::COUNT);
    switch (_state)
    {
    using enum script_lifecycle;
    case COUNT: std::unreachable();
    case created:
    case initializing:
    case destroying:
        ptr->delete_self();
        ptr = nullptr;
        break;
    case no_init:
    case torn_down:
        break;
    }
}

#undef FM_ASSERT_SCRIPT_STATE

} // namespace floormat
