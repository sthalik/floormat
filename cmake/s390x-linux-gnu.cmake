set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR s390x)
set(CMAKE_LIBRARY_ARCHITECTURE s390x-linux-gnu)
set(CMAKE_C_FLAGS_INIT "-march=z15 -mzvector")
set(CMAKE_CXX_FLAGS_INIT "-march=z15 -mzvector")
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-s390x)
# Otherwise pkg-config reads the host's .pc files, and SDL2's check for threads fails.
set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/s390x-linux-gnu/pkgconfig:/usr/share/pkgconfig)
set(ENV{PKG_CONFIG_PATH} "")
