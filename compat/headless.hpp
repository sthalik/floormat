#pragma once

#ifdef __APPLE__
#include <mg/WindowlessCglApplication.h>
#define FM_APPLICATION Platform::WindowlessCglApplication
#elif defined _WIN32
#include <mg/WindowlessWglApplication.h>
#define FM_APPLICATION Platform::WindowlessWglApplication
#elif defined MAGNUM_TARGET_EGL
#include <mg/WindowlessEglApplication.h>
#define FM_APPLICATION Platform::WindowlessEglApplication
#else
#include <mg/WindowlessGlxApplication.h>
#define FM_APPLICATION Platform::WindowlessGlxApplication
#endif
