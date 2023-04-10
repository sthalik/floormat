#pragma once

#include "src/precomp.hpp"
#include "entity/metadata.hpp"
#include "entity/accessor.hpp"

#include <cstdlib>
#include <map>

#include <fmt/format.h>
#include <fmt/compile.h>

#include <Corrade/Utility/Arguments.h>
#include <Magnum/ImGuiIntegration/Context.h>

#if __has_include(<SDL.h>)
#include <Magnum/Platform/Sdl2Application.h>
#endif
