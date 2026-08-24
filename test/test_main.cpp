#include "test_main.h"

int main()
{
    std::printf("running %zu tests\n", tests().size());
    for (const auto& t : tests()) {
        int before = failures();
        t.fn();
        std::printf("  [%s] %s\n", failures() == before ? "ok  " : "FAIL", t.name);
    }
    if (failures()) {
        std::printf("\n%d check(s) failed\n", failures());
        return 1;
    }
    std::printf("\nall tests passed\n");
    return 0;
}
