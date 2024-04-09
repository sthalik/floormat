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

template <typename S, typename Obj>
Script<S, Obj>::~Script() noexcept
{
    fm_assert(state < script_lifecycle::COUNT);
    switch (state)
    {
    case script_lifecycle::no_init:
    case script_lifecycle::torn_down:
        state = (script_lifecycle)(unsigned)-1;
        break;
    case script_lifecycle::COUNT:
        std::unreachable();
    case script_lifecycle::created:
    case script_lifecycle::destroying:
    case script_lifecycle::initializing:
        fm_abort("invalid state '%s' in script destructor",
                 base_script::state_name(state).data());
    }
}

template <typename S, typename Obj>

Script<S, Obj>::Script(): ptr{nullptr}, state{script_lifecycle::no_init}
{
    detail_Script::concept_check<S, Obj>();
}

template <typename S, typename Obj>
void Script<S, Obj>::_assert_state(script_lifecycle s, const char* file, int line)
{
    if (state != s)
    {
        fm_EMIT_DEBUG2("fatal: ",
                       "invalid state transition from '%s' to '%s'",
                       base_script::state_name(state).data(),
                       base_script::state_name(s).data());
        fm_EMIT_DEBUG("", " in %s:%d", file, line);
        fm_EMIT_ABORT();
    }
}

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
    state = script_lifecycle::initializing;
    ptr = p;
}

template <typename S, typename Obj>
void Script<S, Obj>::do_initialize(const std::shared_ptr<Obj>& obj)
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::initializing);
    state = script_lifecycle::created;
    ptr->on_init(obj);
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
void Script<S, Obj>::do_destroy_1(const std::shared_ptr<Obj>& obj)
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::created);
    state = script_lifecycle::torn_down;
    ptr->on_destroy(obj, script_destroy_reason::kill);
    ptr->delete_self();
    ptr = nullptr;
}

template <typename S, typename Obj>
void Script<S, Obj>::do_destroy_pre(const std::shared_ptr<Obj>& obj)
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::created);
    state = script_lifecycle::destroying;
    ptr->on_destroy(obj, script_destroy_reason::quit);
}

template <typename S, typename Obj>
void Script<S, Obj>::do_finish_destroy()
{
    FM_ASSERT_SCRIPT_STATE(script_lifecycle::destroying);
    state = script_lifecycle::torn_down;
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
    fm_assert(state < script_lifecycle::COUNT);
    switch (state)
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
    fm_assert(false);
}

#undef FM_ASSERT_SCRIPT_STATE

} // namespace floormat
