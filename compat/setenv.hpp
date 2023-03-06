#pragma once
#include <cstdlib>

#ifdef _WIN32
namespace floormat {
using std::getenv;
int setenv(const char* name, const char* value, int overwrite);
int unsetenv(const char* name);
} // namespace floormat
#else
#include <cstdlib>
namespace floormat { using std::getenv; using ::setenv; using ::unsetenv; }
#endif
