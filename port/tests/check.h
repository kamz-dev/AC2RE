#pragma once
// A minimal test framework: TEST(name) { CHECK(cond); CHECK_NEAR(a, b, eps); }. Run `ac2_tests [filter]`.

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace test {

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char* file, int line, const char* what) {
    std::printf("  FAIL %s:%d: %s\n", file, line, what);
    ++failures();
}

}  // namespace test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                                     \
    static void TEST_CONCAT(test_fn_, name)();                                         \
    static test::Registrar TEST_CONCAT(test_reg_, name)(#name, TEST_CONCAT(test_fn_, name)); \
    static void TEST_CONCAT(test_fn_, name)()

#define CHECK(cond) \
    do { if (!(cond)) test::fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_NEAR(a, b, eps) \
    do { if (!(std::fabs((a) - (b)) <= (eps))) test::fail(__FILE__, __LINE__, #a " ~= " #b); } while (0)
