#pragma once
// ============================================================================
// Minimal GoogleTest-compatible shim.
// CMake uses the real GoogleTest when it is installed (find_package(GTest))
// and falls back to this header otherwise, so `ctest` works on any machine.
// Supports the subset of macros used by NEXA's tests.
// ============================================================================
#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace mini_gtest {
struct TestCase { const char* suite; const char* name; std::function<void()> fn; };
inline TestCase* registry() { static TestCase tests[512]; return tests; }
inline int& count() { static int c = 0; return c; }
inline bool& currentFailed() { static bool f = false; return f; }
struct Registrar { Registrar(const char* s, const char* n, std::function<void()> f) { registry()[count()++] = TestCase{s, n, f}; } };
struct FatalFailure {};
inline void fail(const char* file, int line, const std::string& msg, bool fatal) {
    std::cerr << "  " << file << ":" << line << ": Failure\n    " << msg << "\n";
    currentFailed() = true;
    if (fatal) throw FatalFailure{};
}
template <typename T, typename = void> struct Streamable : std::false_type {};
template <typename T> struct Streamable<T, decltype(void(std::declval<std::ostream&>() << std::declval<const T&>()))> : std::true_type {};
template <typename T> void put(std::ostream& o, const T& v) { if constexpr (Streamable<T>::value) o << v; else o << "<non-printable>"; }
template <typename A, typename B> std::string show(const A& a, const B& b) { std::ostringstream o; o << "lhs="; put(o, a); o << " rhs="; put(o, b); return o.str(); }
inline int runAll() {
    int failed = 0;
    for (int i = 0; i < count(); ++i) {
        TestCase& t = registry()[i];
        currentFailed() = false;
        std::cout << "[ RUN      ] " << t.suite << "." << t.name << "\n";
        try { t.fn(); } catch (FatalFailure&) {} catch (std::exception& e) { fail(__FILE__, __LINE__, std::string("uncaught exception: ") + e.what(), false); }
        if (currentFailed()) { ++failed; std::cout << "[  FAILED  ] " << t.suite << "." << t.name << "\n"; }
        else std::cout << "[       OK ] " << t.suite << "." << t.name << "\n";
    }
    std::cout << "[==========] " << count() << " tests, " << (count() - failed) << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
} // namespace mini_gtest

#define TEST(S, N) \
    static void S##_##N##_impl(); \
    static mini_gtest::Registrar S##_##N##_reg(#S, #N, S##_##N##_impl); \
    static void S##_##N##_impl()

#define MG_CHECK(cond, msg, fatal) do { if (!(cond)) mini_gtest::fail(__FILE__, __LINE__, msg, fatal); } while (0)
#define EXPECT_TRUE(c)  MG_CHECK((c), "expected true: " #c, false)
#define EXPECT_FALSE(c) MG_CHECK(!(c), "expected false: " #c, false)
#define ASSERT_TRUE(c)  MG_CHECK((c), "expected true: " #c, true)
#define ASSERT_FALSE(c) MG_CHECK(!(c), "expected false: " #c, true)
#define EXPECT_EQ(a, b) MG_CHECK((a) == (b), std::string(#a " == " #b " ; ") + mini_gtest::show((a), (b)), false)
#define ASSERT_EQ(a, b) MG_CHECK((a) == (b), std::string(#a " == " #b " ; ") + mini_gtest::show((a), (b)), true)
#define EXPECT_NE(a, b) MG_CHECK((a) != (b), #a " != " #b, false)
#define EXPECT_LT(a, b) MG_CHECK((a) < (b), std::string(#a " < " #b " ; ") + mini_gtest::show((a), (b)), false)
#define EXPECT_LE(a, b) MG_CHECK((a) <= (b), std::string(#a " <= " #b " ; ") + mini_gtest::show((a), (b)), false)
#define EXPECT_GT(a, b) MG_CHECK((a) > (b), std::string(#a " > " #b " ; ") + mini_gtest::show((a), (b)), false)
#define EXPECT_GE(a, b) MG_CHECK((a) >= (b), std::string(#a " >= " #b " ; ") + mini_gtest::show((a), (b)), false)
#define ASSERT_GE(a, b) MG_CHECK((a) >= (b), std::string(#a " >= " #b " ; ") + mini_gtest::show((a), (b)), true)
#define EXPECT_NEAR(a, b, eps) MG_CHECK(std::fabs((a) - (b)) <= (eps), std::string(#a " ~= " #b " ; ") + mini_gtest::show((a), (b)), false)
#define EXPECT_THROW(stmt, ex) do { bool caught = false; try { stmt; } catch (const ex&) { caught = true; } catch (...) {} MG_CHECK(caught, "expected exception " #ex " from " #stmt, false); } while (0)
#define EXPECT_NO_THROW(stmt) do { bool threw = false; try { stmt; } catch (...) { threw = true; } MG_CHECK(!threw, "unexpected exception from " #stmt, false); } while (0)

#define MINI_GTEST_MAIN int main() { return mini_gtest::runAll(); }
