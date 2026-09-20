// Defining this keeps libc++abi's demangler (117 KB)
// out of the output executable. The only caller
// is the unhandled exception handler. It's only used
// for the exception type name (not backtrace), and it falls back
// to printing the mangled exception type.
#ifdef _LIBCPP_VERSION
#include <cxxabi.h>
extern "C" char* __cxa_demangle(const char*, char*, size_t*, int* status)
{
    if (status)
        *status = -2; // invalid mangled name
    return nullptr;
}
#endif
