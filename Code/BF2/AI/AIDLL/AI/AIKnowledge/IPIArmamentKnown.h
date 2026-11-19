// Armament information about an object the side does not own. Weapon refreshes are deferred:
// updateWeapons only marks the list stale, and the next read rebuilds it from known weapon info.
// [INFERRED] Source placement beside IPIArmamentReal.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H

#include "IPIArmament.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIArmamentKnown : public IPIArmament
{
public:
    // Builds the weapon list from known weapon info.
    explicit IPIArmamentKnown(Information* information);
    // Frees every weapon info this plug-in registered.
    virtual ~IPIArmamentKnown();

    // Rebuilds a stale list, then updates every registered weapon info.
    virtual void setTime(double time);                                  // +4
    virtual unsigned int getWeapon(int index);                          // +7 - rebuilds when stale
    virtual std::vector<unsigned int>& getWeapons();                    // +8 - rebuilds when stale
    virtual void updateWeapons();                                       // +12 - marks stale
    // Rebuilds the list from known weapon info and clears the stale mark. A virtual of its own,
    // after the base's slots. [INFERRED] name.
    virtual void rebuildWeapons();                                      // +16

private:
    /* +0x1C PE */ bool m_weaponsStale;                // [INFERRED] name
    // The PE allocation is 0x24 bytes; no audited body touches its final dword.
    unsigned char m_unresolvedKnownDword[4];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIARMAMENTKNOWN_H
