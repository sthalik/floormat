#pragma once
#include "src/precomp.hpp"

#include <Corrade/Utility/DebugStl.h>

#include <Magnum/Timeline.h>
#include <Magnum/GL/DebugOutput.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Renderbuffer.h>
#include <Magnum/GL/RenderbufferFormat.h>

#include <fmt/format.h>
#include <fmt/compile.h>

#if __has_include(<SDL.h>)
#include <Magnum/Platform/Sdl2Application.h>
#include <SDL_keycode.h>
#include <SDL_events.h>
#endif
