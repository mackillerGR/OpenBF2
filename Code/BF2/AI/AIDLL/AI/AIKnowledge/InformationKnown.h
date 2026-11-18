// Information a side holds about an object it does not own. While the side has contact with the
// object, position and transformation read the engine object directly; without contact they answer
// the last values cached when contact ended, and the security decays from the last time stamp.
// [INFERRED] Source placement beside InformationReal.

#ifndef AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H
#define AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H

#include "Information.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

class InformationKnown : public Information
{
public:
    // The base gets the time -1. Caches the object's side, position and transformation; no contact.
    // degeneration scales how fast the security decays; side is the side that holds the information.
    InformationKnown(IAIObject* object, unsigned int types, double time, float degeneration,
                     float basicTemp, float allowedTimeDiff, int side);
    virtual ~InformationKnown();

    virtual int    getInformationSort();                                // +2 - 1
    // Refreshes the plug-ins once the allowed time difference has passed; false before that.
    virtual bool   setTime(double time);                                // +3
    // Clamps security to [0, 1]. Below the current security nothing happens (false); otherwise the
    // information is refreshed and its time stamp moved back so that it decays from that security.
    virtual bool   setTime(double time, float security);                // +4
    virtual double getTimeStamp() const;                                // +5
    virtual void   addContact();                                        // +6
    virtual void   removeContact();                                     // +7 - caches at zero
    virtual void   removeAllContacts();                                 // +8 - caches
    virtual bool   hasContact() const;                                  // +9
    virtual float  getSecurity(double time);                            // +10
    virtual float  getSecurity();                                       // +11 - 1 with contact
    virtual Vec3&  getPosition();                                       // +12
    virtual float  getAltitude();                                       // +13
    virtual float  getAltitude(const Vec3& direction);                  // +14
    virtual Mat4&  getTransformation();                                 // +15
    virtual Vec3   getFaceing();                                        // +16 - always the cache
    virtual int    getSide() const;                                     // +17 - the cached side

private:
    // Inlined at every use in the retail image: cache the live position and transformation and
    // tell the mobile plug-in that contact was lost. [INFERRED] name.
    void cacheLastKnown();

    // Offsets are PE; the base ends at +0x50 there.
    /* +0x50 */ double m_timeStamp;                 // [INFERRED] name
    /* +0x58 */ float  m_degeneration;              // [INFERRED] name
    /* +0x5C */ Vec3   m_lastPosition;              // [INFERRED] name
    /* +0x68 */ Mat4   m_lastTransformation;        // [INFERRED] name
    /* +0xA8 */ int    m_side;                      // [INFERRED] name; the object's side
    /* +0xAC */ int    m_knownBySide;               // [INFERRED] name; the constructor's side
    /* +0xB0 */ int    m_contacts;                  // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIKNOWLEDGE_INFORMATIONKNOWN_H
