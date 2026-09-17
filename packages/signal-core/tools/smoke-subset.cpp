#include "SignalCore/SmokeSubset.h"
#include <cstdio>

int main() {
    int assertions = 0;
    int failures = 0;
    const auto Check = [&assertions, &failures](bool passed, const char *name) {
        ++assertions;
        if (!passed) {
            ++failures;
            std::printf("SignalCoreSmoke failed: %s\n", name);
        }
    };
    signal_core::RunSmokeSubset(Check);
    std::printf("SignalCoreSmoke assertions=%d failures=%d\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
