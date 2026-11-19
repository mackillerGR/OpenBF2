// Aim-correction state used by BotMain.

#ifndef AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H
#define AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class BotMain;

// Aim-correction state; construction leaves the first four bytes uninitialized.
class FireCorrectionData
{
public:
    FireCorrectionData();                       // for the class

    // Aims at a new firing target: the target time becomes -1, the three flags are raised, the
    // offset at +44 is zeroed and the target's engine object (null for an unknown handle) is
    // cached. PE 101DAB40; [INFERRED] name.
    void setTarget(unsigned int handle);
    // The aim offset at +44; the aim statements read and clear it inline. [INFERRED] accessor.
    Vec3& getTargetOffset() { return m_targetOffset; }
    // Records the last solved aim when recording is on (m_flag6). [INFERRED] name: the turret aim
    // entry writes the fields inline.
    void recordAim(const Vec3& direction, const Vec3& point)
    {
        if (m_flag6) {
            m_aimDirection = direction;
            m_aimPoint = point;
        }
    }

private:
    // [INFERRED] planExecution copies a finished bot projectile's points in inline.
    friend class BotMain;

    /*  0 */ char m_gap0[4];                    // [UNRESOLVED] - not initialised by the ctor
    /*  4 */ bool m_flag4;                      // = true
    /*  5 */ bool m_flag5;                      // = true
    /*  6 */ bool m_flag6;                      // setTarget: true [UNRESOLVED] meaning
    /*  7 */ char m_pad7;
    /*  8 */ int  m_zero8[6];                   // +8..+28, zeroed by the ctor
    // The aim direction and target point of the last solved aim, recorded by the turret aim
    // entry while m_flag6 is set. Zeroed by the ctor. [INFERRED] names.
    /* 32 */ Vec3 m_aimDirection;
    /* 44 */ Vec3 m_targetOffset;               // zeroed by the ctor and setTarget [INFERRED] name
    /* 56 */ Vec3 m_aimPoint;
    /* 68 */ char m_gap68[4];                   // [UNRESOLVED] - the ctor skips +68..+83
    /* 72 */ double m_targetTime;               // setTarget: -1.0 [INFERRED] name
    /* 80 */ void* m_targetObject;              // setTarget: the target's engine object [INFERRED]
    /* 84 */ bool m_flag84;                     // = false
    /* 85 */ bool m_flag85;                     // planExecution: true with m_flag84
    /* 86 */ char m_pad86[2];
    // The last bot projectile that passed its point, from IBFCollisionHandler; zeroed by the
    // constructor except the height.
    /* 88 */ Vec3  m_projectilePrePoint;
    /* 100 */ Vec3 m_projectilePostPoint;
    /* 112 */ Vec3 m_projectileTargetPoint;
    /* 124 */ float m_projectileMaxHeight;      // not initialised by the ctor
};


} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTSUBOBJECTS_H
