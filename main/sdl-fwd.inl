#pragma once
#include "sdl-fwd.hpp"
#include <Magnum/Platform/Sdl2Application.h>

namespace floormat::sdl2 {

using App = Platform::Sdl2Application;

struct EvButtons { App::Pointer&          val; };
struct EvClick   { App::PointerEvent&     val; };
struct EvMove    { App::PointerMoveEvent& val; };
struct EvScroll  { App::ScrollEvent&      val; };
struct EvKey     { App::KeyEvent&         val; };

} // namespace floormat::sdl2
