set -e
cxx=$1; shift
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
n=0
for f in compat/atomic.cpp compat/atomic-wait.cpp compat/atomic-wrapper.cpp compat/atomic-flag-wrapper.cpp compat/valgrind.cpp \
         test/atomic.cpp test/atomic-wait.cpp test/chase-lev-deque.cpp test/event-count.cpp .github/thread-tests.cpp; do
    n=$((n+1))
    "$cxx" -std=c++23 -g -fno-exceptions -fno-rtti -pthread -I. -include compat/integer-types.hpp -include stddef.h \
        -DCORRADE_NEVER_INLINE='__attribute__((noinline))' -Wall -Wextra "$@" -c "$f" -o "$out/$n.o"
done
"$cxx" -pthread "$@" -o "$out/thread-tests" "$out"/*.o
echo "$cxx $*"
"$out/thread-tests"
