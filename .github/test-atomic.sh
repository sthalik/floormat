set -e
cc=$1 cxx=$2
shift 2
dir=build/test-atomic
env FLOORMAT_NO_USERCONFIG=1 CC="$cc" CXX="$cxx" cmake --fresh -G Ninja -S . -B "$dir" -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS=$*" >/dev/null
cmake --build "$dir" --target floormat-test-atomic
echo "$cxx $*"
"$dir/Debug/bin/floormat-test-atomic"
