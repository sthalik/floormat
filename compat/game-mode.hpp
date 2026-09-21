#pragma once
#include <cr/Optional.h>

namespace floormat {

// Ticks Win+G's "Remember this is a game" for the running exe. Win+G's own tick takes effect from
// the next launch, so after adding the entry the constructor runs the exe again and exits with the
// child's status, standing in for execve().
struct with_game_mode final
{
    enum class entry : uint8_t { already_set, added, failed };

    with_game_mode() noexcept;

    static entry designate() noexcept;
    // Nothing when no child was started: under a debugger, or in a child this started, so a
    // repeated `added` can't loop.
    static Optional<int> relaunch() noexcept;
};

} // namespace floormat
