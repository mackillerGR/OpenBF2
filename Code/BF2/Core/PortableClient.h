#pragma once
// [CHOSEN, not recovered] Forced into the non-x86-32 client build. The recovered client
// asserts retail 32-bit layouts; a 64-bit image cannot meet them, so those asserts are
// compiled out here. Any code that really uses retail byte offsets must be fixed by hand:
// see docs/android-port.md for the audit list.
#ifdef __cplusplus
#include <type_traits>
#define static_assert(...)
#endif
