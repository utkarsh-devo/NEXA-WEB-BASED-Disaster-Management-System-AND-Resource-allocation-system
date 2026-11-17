#pragma once
// Selects real GoogleTest or the bundled shim (see CMakeLists.txt).
#ifdef NEXA_USE_REAL_GTEST
#include <gtest/gtest.h>
#else
#include "mini_gtest.h"
#endif
