@echo off
setlocal EnableDelayedExpansion
set out=build\thread-tests
if exist %out% rmdir /s /q %out%
mkdir %out% || exit /b 1
set n=0
for %%f in (compat\atomic.cpp compat\atomic-wait.cpp compat\atomic-wrapper.cpp compat\atomic-flag-wrapper.cpp compat\valgrind.cpp test\atomic.cpp test\atomic-wait.cpp test\chase-lev-deque.cpp test\event-count.cpp .github\thread-tests.cpp) do (
    set /a n+=1
    cl -nologo -std:c++latest -W4 -Z7 -GR- -EHs-c- -D_HAS_EXCEPTIONS=0 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -DWIN32_LEAN_AND_MEAN "-DCORRADE_NEVER_INLINE=__declspec(noinline)" -I. -FIcompat/integer-types.hpp -FIstddef.h %* -c %%f -Fo%out%\!n!.obj || exit /b 1
)
cl -nologo %* %out%\*.obj -Fe%out%\thread-tests.exe -link synchronization.lib || exit /b 1
