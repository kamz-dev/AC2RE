#include <cstring>

#include "check.h"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedCases = 0;
    for (const test::Case& c : test::registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        int before = test::failures();
        c.fn();
        ++run;
        bool ok = test::failures() == before;
        if (!ok) ++failedCases;
        std::printf("%s %s\n", ok ? "ok  " : "FAIL", c.name);
    }
    std::printf("%d tests, %d failed\n", run, failedCases);
    return failedCases == 0 ? 0 : 1;
}
