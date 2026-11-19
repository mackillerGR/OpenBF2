// Weapon information about a weapon the side does not own: a snapshot of the engine weapon,
// refreshed at most once a second, whose security decays with the snapshot's age.
// [INFERRED] Source placement beside WeaponInfoReal.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFOKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_WEAPONINFOKNOWN_H

#include <vector>

#include "WeaponInfo.h"

namespace dice { namespace hfe { namespace ai {

class WeaponInfoKnown : public WeaponInfo
{
public:
    // Takes the first snapshot of the weapon.
    explicit WeaponInfoKnown(IWeapon* weapon);
    virtual ~WeaponInfoKnown();

    // Retakes the snapshot when more than a second has passed. Always true.
    virtual bool   update();
    virtual int    getShotsInMag(int magazine);
    virtual int    getAmmo() const;
    virtual int    getCurrentAmmo() const;
    virtual int    getNumberOfMags() const;
    virtual bool   isReadyToFire() const;
    virtual double getTimeOfFiring() const;
    virtual float  getHeat() const;
    virtual bool   usesHeat() const;                    // the live weapon
    virtual float  getSecurity() const;                 // now
    virtual float  getSecurity(double time) const;

private:
    // Offsets are PE.
    /* +0x0C */ std::vector<int> m_shotsInMags;         // [INFERRED] name
    /* +0x1C */ int    m_ammo;                          // [INFERRED] name
    /* +0x20 */ int    m_currentAmmo;                   // [INFERRED] name
    /* +0x24 */ bool   m_readyToFire;                   // [INFERRED] name
    /* +0x28 */ float  m_heat;                          // [INFERRED] name
    /* +0x30 */ double m_timeOfFiring;                  // [INFERRED] name
    /* +0x38 */ double m_timeStamp;                     // [INFERRED] name
};

} } }
#endif
