// A tiny test harness: no dependencies, so the native tests build with just cl.exe.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& Registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

inline void Fail(const char* file, int line, const std::string& message) {
    std::printf("    FAIL %s:%d  %s\n", file, line, message.c_str());
    ++Failures();
}

inline int RunAll() {
    int failedCases = 0;
    for (auto& c : Registry()) {
        int before = Failures();
        c.body();
        bool ok = Failures() == before;
        failedCases += !ok;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", c.name);
    }
    std::printf("\n%zu tests, %d failed\n", Registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                                        \
    static void name();                                                   \
    static testing::Registrar registrar_##name(#name, name);              \
    static void name()

#define CHECK(condition)                                                                  \
    do {                                                                                  \
        if (!(condition)) testing::Fail(__FILE__, __LINE__, "CHECK(" #condition ")");     \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                         \
    do {                                                                                                   \
        auto&& a_ = (actual);                                                                              \
        auto&& e_ = (expected);                                                                            \
        if (!(a_ == e_)) testing::Fail(__FILE__, __LINE__, "CHECK_EQ(" #actual ", " #expected ")");        \
    } while (0)
