#include "demangle.h"
#include <cstdlib>
#include <string>
#include "llvm/Demangle/Demangle.h"

using namespace Corrade::Containers;

String demangle_symbol(const char* symbol)
{
    if (!symbol || !*symbol)
        return {};
    if (const auto str = llvm::demangle(symbol); !str.empty())
        return String{str.data(), str.size()};
    return {};
}
