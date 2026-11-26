#pragma once

// [TEMP] [CHOSEN, not recovered] Android exit-hang diagnostic. Reports each stage of the
// engine shutdown to stderr, because BF2Log is closed partway through
// BF2EngineSetup::shutdownModules and the Android log captures stderr.
// [RETIRE-WHEN: android-exit-diag] Remove once the Android exit hang is located, fixed, and a
// device log shows the app leaving main after a quit from the menu.
#include <cstdio>

#if defined(__ANDROID__) || defined(BF2_TMPTRACE_SHUTDOWN)
#define BF2_SHUTDOWN_STEP(...) \
    (std::fprintf(stderr, "[TEMP][SHUTDOWN] " __VA_ARGS__), std::fputc('\n', stderr), std::fflush(stderr))
#else
#define BF2_SHUTDOWN_STEP(...) ((void)0)
#endif
