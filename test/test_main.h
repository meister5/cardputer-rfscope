// Minimal zero-dependency test harness for host-side unit tests.
#pragma once
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& tests()
{
    static std::vector<TestCase> t;
    return t;
}

inline int& failures()
{
    static int f = 0;
    return f;
}

struct TestReg {
    TestReg(const char* n, std::function<void()> f)
    {
        tests().push_back({n, f});
    }
};

#define TEST(name)                                    \
    static void name();                               \
    static TestReg test_reg_##name(#name, name);      \
    static void name()

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("    FAIL %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            failures()++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        long _a = (long)(a);                                                   \
        long _b = (long)(b);                                                   \
        if (_a != _b) {                                                        \
            std::printf("    FAIL %s:%d  %s == %s  (got %ld, want %ld)\n",     \
                        __FILE__, __LINE__, #a, #b, _a, _b);                   \
            failures()++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        double _a = (double)(a);                                               \
        double _b = (double)(b);                                               \
        if (std::fabs(_a - _b) > (eps)) {                                      \
            std::printf("    FAIL %s:%d  %s ~= %s  (got %f, want %f)\n",       \
                        __FILE__, __LINE__, #a, #b, _a, _b);                   \
            failures()++;                                                      \
        }                                                                      \
    } while (0)
