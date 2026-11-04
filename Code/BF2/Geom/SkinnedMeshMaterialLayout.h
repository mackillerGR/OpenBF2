// [CHOSEN, not recovered] The skinned material record is addressed by retail byte offsets. On
// 64-bit hosts its vectors, pointers and strings are wider, so this maps each retail offset to the
// host's own. Retail builds use the offsets unchanged.
// [RETIRE-WHEN: skinned-material-typed] Remove once the record is a typed struct.

#ifndef GEOM_SKINNEDMESHMATERIALLAYOUT_H
#define GEOM_SKINNEDMESHMATERIALLAYOUT_H

#include <cstdint>

namespace dice { namespace hfe { namespace dx9rend { namespace Ra {
unsigned char* raMaterialFieldBytes(void* parameters, unsigned int retail);
} } } }

namespace dice { namespace hfe { namespace geom {

// The lod record: 64 bytes of bounds, four part vectors, two flag bytes, padding.
enum
{
    kSkinnedVectorBytes = 4 * sizeof(void*),
    kSkinnedLodFlags    = 64 + 4 * kSkinnedVectorBytes,
    kSkinnedLodBytes    = kSkinnedLodFlags + 2 * sizeof(void*) - (sizeof(void*) == 4 ? 4 : 8),
    kSkinnedGeomBytes   = 2 * kSkinnedVectorBytes
};

#if UINTPTR_MAX > 0xFFFFFFFFu
enum { kSkinnedMaterialRecordBytes = 0x368 };

inline unsigned int sk(unsigned int retail)
{
    if (retail == 0x184) return 0x360;              // the bound shader, past the Ra block
    if (retail >= 0x1BC) return 0x348 + (retail - 0x1BC);
    if (retail >= 0x1A0) return 0x328 + (retail - 0x1A0);
    if (retail >= 0x19C) return 0x320;
    if (retail >= 0x198) return 0x318;
    if (retail >= 0x194) return 0x310;
    if (retail >= 0x190) return 0x308;
    if (retail >= 0x188) return 0x300 + (retail - 0x188);
    if (retail >= 0x38)
        return 0x60 + static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(
            ::dice::hfe::dx9rend::Ra::raMaterialFieldBytes(nullptr, retail - 0x38)));
    if (retail >= 0x28)  return 0x40 + (retail - 0x28) * 2;
    if (retail >= 0x14)  return 0x28 + (retail - 0x14);
    if (retail >= 0x04)  return 0x08 + (retail - 0x04) * 2;
    return retail;
}
#else
enum { kSkinnedMaterialRecordBytes = 456 };
inline unsigned int sk(unsigned int retail) { return retail; }
#endif

} } }

#endif
