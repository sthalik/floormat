#pragma once

#if defined _MSC_VER
#define FM_PRETTY_FUNCTION __FUNCSIG__
#else
#define FM_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif
