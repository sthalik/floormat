#define FM_NO_CORRADE_DEBUG_EXTERN_TEMPLATE_QUOTED
#include "debug.hpp"

namespace floormat::detail::corrade_debug {

template struct Quoted<StringView>;

Debug::Flags debug1(Debug& dbg)
{
    auto flags = dbg.flags();
    dbg << "";
    dbg.setFlags(flags | Debug::Flag::NoSpace);
    dbg << "'";
    return flags;
}

Debug& debug2(Debug& dbg, Debug::Flags flags)
{
    dbg << "'";
    dbg.setFlags(flags);
    return dbg;
}

} // namespace floormat::detail::corrade_debug
