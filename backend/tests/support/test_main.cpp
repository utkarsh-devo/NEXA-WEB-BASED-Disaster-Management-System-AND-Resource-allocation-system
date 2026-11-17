// Entry point for the bundled mini test runner (unused with real GoogleTest,
// which links GTest::gtest_main instead).
#include "TestFramework.hpp"
#ifndef NEXA_USE_REAL_GTEST
MINI_GTEST_MAIN
#endif
