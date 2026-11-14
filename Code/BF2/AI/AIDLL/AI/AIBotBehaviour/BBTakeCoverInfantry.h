// Infantry cover-seeking behaviour. The source path is attested by the image's __FILE__ string.
// [INFERRED] member and helper class names.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H

#include "BotBehaviour.h"

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class PlanGenerator;

// What a bot takes cover from: an object, or a known information handle. Both are eight bytes,
// a vptr and one word. [INFERRED] names; the five slots follow the PE vtables 1027F108 and 1027F0C4.
class CoverThreat
{
public:
    virtual bool isObject() const = 0;
    virtual bool isHandle() const = 0;
    // The threat's position, false when unknown.
    virtual bool getPosition(Vec3& position) const = 0;
    virtual int  getWord() const = 0;
    // [UNRESOLVED] Compares two threats through the first two slots. PE 10119CE0.
    virtual bool matches(const CoverThreat& other) const = 0;
};

class CoverThreatObject : public CoverThreat
{
public:
    CoverThreatObject() : m_object(0) {}
    virtual bool isObject() const { return true; }       // PE 10119FE0
    virtual bool isHandle() const { return false; }      // PE 10119FF0
    virtual bool getPosition(Vec3& position) const;      // PE 1011A040
    virtual int  getWord() const;                        // PE 1011A000
    virtual bool matches(const CoverThreat& other) const;

private:
    /* +4 */ void* m_object;   // [UNRESOLVED] type; its position is at +8
};

class CoverThreatHandle : public CoverThreat
{
public:
    CoverThreatHandle() : m_handle(static_cast<unsigned int>(-1)) {}
    virtual bool isObject() const { return false; }      // PE 10119C80
    virtual bool isHandle() const { return true; }       // PE 10119C90
    virtual bool getPosition(Vec3& position) const;      // PE 1011A070
    virtual int  getWord() const;                        // PE 1011A010
    virtual bool matches(const CoverThreat& other) const;

private:
    /* +4 */ unsigned int m_handle;
};

class BBTakeCoverInfantry : public BotBehaviour
{
public:
    BBTakeCoverInfantry(int behaviourId, int priority, PlanGenerator* generator,
                        UrgeCurve* urgeCurve, float* weights);   // PE 1011C730
    virtual ~BBTakeCoverInfantry();                              // PE 1011C960
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 1011D330

private:
    /* +0x24 */ int                m_botCount;
    /* +0x28 */ void**             m_cover;      // [UNRESOLVED] element type; per-bot, owned
    /* +0x2C */ CoverThreatObject* m_objectThreats;
    /* +0x30 */ CoverThreatHandle* m_handleThreats;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H
