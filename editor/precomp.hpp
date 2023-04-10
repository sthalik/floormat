#pragma once

#include "src/precomp.hpp"
#include "entity/metadata.hpp"
#include "entity/accessor.hpp"

#include "src/entity.hpp"
#include "src/chunk.hpp"
#include "src/world.hpp"
#include "src/global-coords.hpp"
#include "src/anim-atlas.hpp"

#include "app.hpp"
#include "imgui-raii.hpp"

#include <cstdlib>
#include <map>

#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Utility/Arguments.h>
#include <Magnum/ImGuiIntegration/Context.hpp>

#if __has_include(<SDL.h>)
#include <Magnum/Platform/Sdl2Application.h>
#endif
