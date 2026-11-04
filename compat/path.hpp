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

// {head, tail} of pathname as views into it, like Python's os.path.split(). On Windows '\\' also
// separates, and the root includes a leading drive letter and colon, a "\\?\", "//?/", "\\.\" or
// "//./" prefix with the device after it, a "\\?\UNC\" prefix with "server/share", or
// "//server/share". A "//server" without a share has no root of its own.
Pair<StringView, StringView> split_pathname(StringView pathname);

#ifdef _WIN32

// pathname, an absolute path with '/' separators, with the "//?/" prefix that lets the Windows file
// functions take more than MAX_PATH characters. "//server/share/x" becomes "//?/UNC/server/share/x",
// and a "//./" prefix becomes "//?/". A path that has "//?/" already comes back unchanged.
String extended_length_path(StringView pathname);

#endif

} // namespace floormat
