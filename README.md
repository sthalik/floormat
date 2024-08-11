[![commit](https://github.com/sthalik/floormat/actions/workflows/cmake.yml/badge.svg)](https://github.com/sthalik/floormat/actions/workflows/cmake.yml)

# floormat

Game project in early development.

## Build

Should build cleanly on Clang 17+, GCC 13.2+ and MSVC 17.7+.

### Debian/Ubuntu dependencies
`xorg-dev` `libsdl2-dev` `build-essential` `ninja-build` `cmake-curses-gui`

### Windows dependencies

Only the compiler (MSVC, GCC, Clang) is needed.

### Building
```console
git clone https://github.com/sthalik/floormat.git
cd floormat
git submodule update --init
mkdir build
cd build
cmake -GNinja ../ 
ninja install
ln -s ../doc/saves ./save
install/bin/floormat-editor
```
