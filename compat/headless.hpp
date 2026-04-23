#pragma once

#ifdef __APPLE__
#include <mg/WindowlessCglApplication.h>
#define FM_APPLICATION Platform::WindowlessCglApplication
#elif defined _WIN32
#include <mg/WindowlessWglApplication.h>
#define FM_APPLICATION Platform::WindowlessWglApplication
#else
#include <mg/WindowlessGlxApplication.h>
#define FM_APPLICATION Platform::WindowlessGlxApplication
#endif
