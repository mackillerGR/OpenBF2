// Physical information about an object the side does not own. setTime caches the health, the
// health modifier and the terrain, air, water and man-down states; the geometry, armour and mass
// queries still read the live engine object.
// [INFERRED] Source placement beside IPIPhysicalReal.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALKNOWN_H

#include "IPIPhysical.h"

namespace dice {
namespace hfe {
namespace ai {

class IPIPhysicalKnown : public IPIPhysical
{
public:
    // Clears the cached states; health and modifier stay as allocated until the first setTime.
    explicit IPIPhysicalKnown(Information* information);
    virtual ~IPIPhysicalKnown();

    virtual void   setTime();                                           // +3 - now
    virtual void   setTime(double time);                                // +4 - refreshes the cache
    virtual float  getHealth() const;                                   // +5 - cached, absolute
    virtual void   setHealth(float health);                             // +6 - overwrites the cache
    virtual float  getMaxHealth();                                      // +7
    virtual float  getMod();                                            // +8 - cached, no refresh
    virtual BoundingBox*            getBoundingBox();                   // +9
    virtual const LocalBoundingBox* getLocalBoundingBox();              // +10
    virtual float  getRadius();                                         // +11
    virtual Vec3   getSphereLocalOffset() const;                        // +12
    virtual float  getSmallestRadius() const;                           // +13
    virtual void   getSubSpheres2d(std::vector<Vec2>& out) const;       // +14
    virtual float  getArmour(Vec3& direction);                          // +15
    virtual float  getBestArmour();                                     // +16
    virtual int    getStrengthType();                                   // +17
    virtual float  getVolume();                                         // +18
    virtual float  getMass();                                           // +19
    virtual int    getMaterial() const;                                 // +20
    virtual float  getManDownTimeLeft() const;                          // +21
    virtual float  getMaxManDownTime() const;                           // +22
    virtual bool   getManDown() const;                                  // +23 - cached
    // Writes the cached terrain position and answers the cached state.
    virtual bool   isTouchingTerrain(Vec3& position) const;             // +24
    virtual bool   isInAir() const;                                     // +25 - cached
    virtual bool   isInWater() const;                                   // +26 - cached

private:
    // Offsets are PE.
    /* +0x08 */ float m_mod;                        // not initialised by the constructor
    /* +0x0C */ float m_health;                     // not initialised by the constructor
    /* +0x10 */ bool  m_touchingTerrain;            // [INFERRED] name
    /* +0x14 */ Vec3  m_terrainPosition;            // [INFERRED] name
    /* +0x20 */ bool  m_inAir;                      // [INFERRED] name
    /* +0x21 */ bool  m_inWater;                    // [INFERRED] name
    /* +0x22 */ bool  m_manDown;                    // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_IPIPHYSICALKNOWN_H
