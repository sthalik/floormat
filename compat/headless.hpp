#pragma once

#ifdef __APPLE__
#include <Magnum/Platform/WindowlessCglApplication.h>
#define FM_APPLICATION Platform::WindowlessCglApplication
#elif defined _WIN32
#include <Magnum/Platform/WindowlessWglApplication.h>
#define FM_APPLICATION Platform::WindowlessWglApplication
#else
#include <Magnum/Platform/WindowlessGlxApplication.h>
#define FM_APPLICATION Platform::WindowlessGlxApplication
#endif
