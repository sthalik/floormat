#pragma once
#include <cr/Pair.h>
#include <cr/String.h>

namespace floormat {

// The process's current directory as UTF-8 with '/' separators, or an empty String on failure.
// With CORRADE_BUILD_CPU_RUNTIME_DISPATCH on, Path::currentDirectory() on Windows ends in a call
// through a CPU-dispatch function pointer, via String::replaceAll(). In a static build that pointer
// is still null while global constructors run, and loader_impl's constructor calls this from one.
String current_directory();

String executable_location();

// {head, tail} of pathname, both views into it, like Python's os.path.split(). tail follows
// the last separator after the root. head precedes it without trailing separators, except that the
// root stays whole. The root is the run of leading separators. On Windows both '/' and '\\'
// separate, and the root also takes what precedes that run: a drive letter with its colon, a "\\?\"
// or "//?/" prefix with the device after it ("C:"), a "\\?\UNC\" prefix with "server/share", or
// "//server/share". "\\.\" and "//./" work like "\\?\" and "//?/". A "//server" without a share has no
// root of its own. Elsewhere only '/' separates.
Pair<StringView, StringView> split_pathname(StringView pathname);

#ifdef _WIN32

// pathname, an absolute path with '/' separators, with the "//?/" prefix that lets the Windows file
// functions take more than MAX_PATH characters. "//server/share/x" becomes "//?/UNC/server/share/x",
// and a "//./" prefix becomes "//?/". A path that has "//?/" already comes back unchanged.
String extended_length_path(StringView pathname);

#endif

} // namespace floormat
