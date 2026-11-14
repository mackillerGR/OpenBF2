// The bot behaviours that AIBehaviours.ai builds through the behaviour manager's create functions.
// Class names are the script spellings. Retail keeps one source per class in AIBotBehaviour
// (attested for BBTakeCoverInfantry). [INFERRED] the shared header and every member name: the
// image carries no RTTI for these classes. Offsets in the comments are the retail PE (x86, MSVC)
// layout. Per-bot arrays are indexed by bot id and sized by the configured bot count.
//
// [FRONTIER] Every calculateUrgency body, and the extra virtuals that are not written out, stops by
// name until its retail body is recovered.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOURS_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOURS_H

#include "BotBehaviour.h"

#include "../../../../Math/Vec3.h"

#include <map>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class PlanGenerator;

// A function sampled over [low, high] in fixed steps. PE constructor 101B1D50 (the table is
// built by 101B1CB0), destructor 101B17A0. [INFERRED] name.
class SampledFunction
{
public:
    typedef float (*Function)(float);

    SampledFunction(Function function, float low, float high, float step);
    ~SampledFunction();

private:
    void build();

    /* +0x00 */ Function m_function;
    /* +0x04 */ float*   m_table;     // count + 1 entries
    /* +0x08 */ int      m_count;
    /* +0x0C */ float    m_low;
    /* +0x10 */ float    m_high;
    /* +0x14 */ float    m_step;
};

class BBAvoid : public BotBehaviour
{
public:
    BBAvoid(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100EDBB0
    virtual ~BBAvoid();                                                   // PE 100EDD30
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100EDF10
};

class BBMoveTo : public BotBehaviour
{
public:
    BBMoveTo(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100F0FF0
    virtual ~BBMoveTo();                                                   // PE 100F10A0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100F10D0
};

class BBMoveToFixed : public BotBehaviour
{
public:
    BBMoveToFixed(int behaviourId, int priority);   // PE 100EBCE0
    virtual ~BBMoveToFixed();                                                   // PE 100EBDB0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100EBDF0

protected:
    /* +0x24 */ float* m_float24;   // zeroed
};

class BBMedicAssist : public BotBehaviour
{
public:
    BBMedicAssist(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100FD200
    virtual ~BBMedicAssist();                                                   // PE 100FD4F0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100FE080
    virtual bool isApplicable(Information* equipment, Bot* bot);          // PE 100FD160

protected:
    /* +0x24 */ std::vector<int> m_vector24;   // [UNRESOLVED] element type
    /* +0x34 */ int* m_battle34;
    /* +0x38 */ int* m_battle38;
    /* +0x3C */ int* m_battle3C;
};

class BBIdle : public BotBehaviour
{
public:
    BBIdle(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100F4500
    virtual ~BBIdle();                                                   // PE 100F4840
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100F4C90
    virtual void reset(Bot* bot);                                       // PE 100F46C0

protected:
    /* +0x24 */ double* m_double24;   // 0
    /* +0x28 */ bool*   m_bool28;     // false
    /* +0x2C */ int*    m_int2C;      // 0
};

class BBFire : public BotBehaviour
{
public:
    BBFire(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10103D50
    virtual ~BBFire();                                                   // PE 10103EE0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10102A60
    // Aims the bot at a laser-designated position. [INFERRED] name.
    virtual void fireAtPosition(int designator, const Vec3& position, Bot* bot, int weapon); // PE 10100C90
    // Aims the bot at its current target. [INFERRED] name.
    virtual void fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB);  // PE 10100C30

protected:
    /* +0x24 */ unsigned int* m_int24;   // -1
    /* +0x28 */ double m_double28;   // 0
    /* +0x30 */ std::map<int, int> m_map30;   // [UNRESOLVED] key and value types
    /* +0x3C */ SampledFunction* m_curve;   // fire curve; BBFire only
};

class BBFireInfantery : public BBFire
{
public:
    BBFireInfantery(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
                    float* weights);                                     // PE 10119B90
    virtual ~BBFireInfantery();                                          // PE 10119BF0
    virtual void fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB);  // PE 10119BE0

private:
    /* +0x40 */ PlanGenerator* m_infanteryGenerator;   // the generator again
};

class BBFire3d : public BotBehaviour
{
public:
    BBFire3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10115640
    virtual ~BBFire3d();                                                   // PE 10115780
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 101157B0

protected:
    /* +0x24 */ unsigned int* m_int24;   // -1
    /* +0x28 */ double m_double28;   // 0
    /* +0x30 */ std::map<int, int> m_map30;   // [UNRESOLVED] key and value types
};

class BBFireUnarmed : public BotBehaviour
{
public:
    BBFireUnarmed(int behaviourId, int priority);   // PE 100EC930
    virtual ~BBFireUnarmed();                                                   // PE 100EC530
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100ED370

protected:
    /* +0x24 */ int* m_int24;   // 0
};

class BBFireLargeBore : public BotBehaviour
{
public:
    BBFireLargeBore(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
                    float* weights, bool flag);   // PE 10111EF0
    virtual ~BBFireLargeBore();                                                   // PE 10112050
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10112080
    // Aims the bot at its current target. [INFERRED] name.
    virtual void fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB, bool flagC); // PE 1010F2D0

protected:
    /* +0x24 */ unsigned int* m_int24;   // -1
    /* +0x28 */ double m_double28;   // 0
    /* +0x30 */ std::map<int, int> m_map30;   // [UNRESOLVED] key and value types
    /* +0x3C */ bool m_bool3C;
};

class BBTakeCover : public BotBehaviour
{
public:
    BBTakeCover(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 1011A0C0
    virtual ~BBTakeCover();                                                   // PE 1011A190
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 1011A1E0
    virtual void reset(Bot* bot);                                       // PE 10119DA0

protected:
    /* +0x24 */ int m_botCount;
    /* +0x28 */ void** m_cover;   // [UNRESOLVED] element type; per-bot, owned
};

class BBFireDriver : public BotBehaviour
{
public:
    BBFireDriver(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10112120
    virtual ~BBFireDriver();                                                   // PE 10112270
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 101122C0

protected:
    /* +0x24 */ BotBehaviour* m_unarmed;      // owned BBFireUnarmed
    /* +0x28 */ BotBehaviour* m_largeBore;    // owned BBFireLargeBore
};

class BBChange : public BotBehaviour
{
public:
    BBChange(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100F7320
    virtual ~BBChange();                                                   // PE 100F7520
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100F76B0

protected:
    /* +0x24 */ std::vector<int> m_vector24;   // [UNRESOLVED] element type
    /* +0x34 */ std::vector<int> m_vector34;   // [UNRESOLVED] element type
    /* +0x44 */ BotBehaviour* m_owned44;   // [UNRESOLVED] type; owned, deleted through its virtual destructor
    /* +0x48 */ BotBehaviour* m_owned48;   // [UNRESOLVED] type; as above
};

class BBChangeLandingCraft : public BotBehaviour
{
public:
    BBChangeLandingCraft(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100FAEF0
    virtual ~BBChangeLandingCraft();                                                   // PE 100FAFF0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100FB180
};

class BBChangeTeleport : public BotBehaviour
{
public:
    BBChangeTeleport(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 100FC040
    virtual ~BBChangeTeleport();                                                   // PE 100FC140
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 100FC180
};

class BBSuicide : public BotBehaviour
{
public:
    BBSuicide(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 1011F910
    virtual ~BBSuicide();                                                   // PE 1011F9D0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 1011FA00
};

class BBCallForMedic : public BotBehaviour
{
public:
    BBCallForMedic(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 1011FD00
    virtual ~BBCallForMedic();                                                   // PE 1011FE40
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 1011FF20
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 1011FE80
};

class BBRevive : public BotBehaviour
{
public:
    BBRevive(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10120480
    virtual ~BBRevive();                                                   // PE 10120620
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10120650
    virtual bool isApplicable(Information* equipment, Bot* bot);          // PE 10120410

protected:
    /* +0x24 */ std::vector<int> m_vector24;   // [UNRESOLVED] element type
    /* +0x34 */ int* m_battle34;
    /* +0x38 */ int* m_battle38;
    /* +0x3C */ int* m_battle3C;
};

class BBTakeCover3d : public BotBehaviour
{
public:
    BBTakeCover3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10121910
    virtual ~BBTakeCover3d();                                                   // PE 101219E0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10121A20
};

class BBC4 : public BotBehaviour
{
public:
    BBC4(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10121F70
    virtual ~BBC4();                                                   // PE 10122020
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10122060
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 101222E0
};

class BBRunOverSoldier : public BotBehaviour
{
public:
    BBRunOverSoldier(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10125780
    virtual ~BBRunOverSoldier();                                                   // PE 10125830
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10125870
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 10125740
};

class BBBridgeRepair : public BotBehaviour
{
public:
    BBBridgeRepair(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10122F90
    virtual ~BBBridgeRepair();                                                   // PE 10123110
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10123140
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 10122CC0

protected:
    /* +0x24 */ std::vector<int> m_vector24;   // [UNRESOLVED] element type
    /* +0x34 */ int* m_battle34;
    /* +0x38 */ int* m_battle38;
    /* +0x3C */ int* m_battle3C;
};

class BBDefuseMines : public BotBehaviour
{
public:
    BBDefuseMines(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 101244B0
    virtual ~BBDefuseMines();                                                   // PE 10124640
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10124670
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 10124460

protected:
    /* +0x24 */ std::vector<int> m_vector24;   // [UNRESOLVED] element type
    /* +0x34 */ int* m_battle34;
    /* +0x38 */ int* m_int38;   // -1
};

class BBRandom : public BotBehaviour
{
public:
    BBRandom(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10125FD0
    virtual ~BBRandom();                                                   // PE 101260A0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 101260E0
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 10125F90

protected:
    /* +0x24 */ bool m_bool24;   // false
    /* +0x28 */ double m_double28;   // 0
    /* +0x30 */ double m_double30;   // 0
    /* +0x34 */ std::vector<int> m_vector34;   // [UNRESOLVED] element type
};

class BBTriggerable : public BotBehaviour
{
public:
    BBTriggerable(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights);   // PE 10126E20
    virtual ~BBTriggerable();                                                   // PE 10126ED0
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed);  // PE 10127BD0
    // Whether the bot can act on the behaviour now. [INFERRED] name.
    virtual bool isUsable(Information* equipment, Bot* bot);            // PE 101268F0

protected:
    /* +0x24 */ int m_int24;   // 0
};

class BBTakeCoverInfantry;   // BBTakeCoverInfantry.h

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOURS_H
