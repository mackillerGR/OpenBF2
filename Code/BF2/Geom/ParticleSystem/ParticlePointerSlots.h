// [CHOSEN, not recovered] Pointer storage for the particle system's retail-layout byte blocks on
// 64-bit hosts. The renderers and pooled instances are addressed by their 32-bit offsets, where a
// pointer occupies four bytes; an eight-byte pointer written there would overwrite its neighbour.
// On 64-bit hosts a pointer-typed field therefore lives in a side table keyed by its address, and
// the block keeps its retail shape for every scalar. A 32-bit build compiles to the plain access.
// [RETIRE-WHEN: particles-typed-layout] Remove once the renderers and instances are typed structs.

#ifndef GEOM_PARTICLESYSTEM_PARTICLEPOINTERSLOTS_H
#define GEOM_PARTICLESYSTEM_PARTICLEPOINTERSLOTS_H

#include <cstdint>

namespace dice {
namespace hfe {
namespace geom {

// Returns the stable slot for the pointer field at `address`; it starts out null.
void*& psmPointerSlot(const void* address);

// Scalars stay in the block; pointers go through the side table on 64-bit hosts.
template <typename T>
struct PsmSlot
{
    static T& at(unsigned char* address) { return *reinterpret_cast<T*>(address); }
};

#if UINTPTR_MAX > 0xFFFFFFFFu
template <typename T>
struct PsmSlot<T*>
{
    static T*& at(unsigned char* address)
    {
        return *(T**)(&psmPointerSlot(address));
    }
};
#endif

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // GEOM_PARTICLESYSTEM_PARTICLEPOINTERSLOTS_H
