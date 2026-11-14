// The plan generators that AIBehaviours.ai attaches to the bot behaviours.
// Retail keeps one source per class in AIBotBehaviourPattern (attested for BBPFire2d,
// BBPFire2dDriver, BBPFireInfantery and BBPHeliFire3d). [INFERRED] the shared header, the family
// base names and every member name: the image carries no RTTI for these classes. Class names are the script spellings. Offsets in the
// comments are the retail PE (x86, MSVC) layout. Per-bot arrays are indexed by bot id and sized by
// the configured bot count; the battle-strength arrays by the configured battle-strength count.
//
// [FRONTIER] Every generate body, and the family setters that are not written out below, stops by
// name until its retail body is recovered.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORS_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORS_H

#include "PlanGenerator.h"

#include "../../../../Math/Vec3.h"

#include <list>

namespace dice {
namespace hfe {
namespace ai {

class BotBehaviour;

// Avoid-collision family. PE vtable 10282A98, constructor 10198DD0.
class AvoidCollisionPlanGenerator : public PlanGenerator
{
public:
    AvoidCollisionPlanGenerator();
    virtual ~AvoidCollisionPlanGenerator();

    // Stores what the bot should steer around and marks that a plan is offered.
    virtual void setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                             const Vec3& position, float distance) = 0;

protected:
    /* +0x0C */ unsigned int* m_obstacles;    // uninitialised
    /* +0x10 */ Vec3*         m_positions;    // zeroed
    /* +0x14 */ float*        m_distances;    // uninitialised
};

// 2d and infantry variants keep the direction on the ground plane.
class BBPAvoidCollision2d : public AvoidCollisionPlanGenerator
{
public:
    explicit BBPAvoidCollision2d(int behaviourId);                    // PE 100F0900
    virtual ~BBPAvoidCollision2d();                                   // PE 100F0990
    virtual BotScript* generate(Bot* bot);                            // PE 100F0A60
    virtual void setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                             const Vec3& position, float distance);   // PE 100F09D0
private:
    /* +0x1C */ Vec2* m_directions;
};

class BBPAvoidCollision3d : public AvoidCollisionPlanGenerator
{
public:
    explicit BBPAvoidCollision3d(int behaviourId);                    // PE 100EFBC0
    virtual ~BBPAvoidCollision3d();                                   // PE 100EFC40
    virtual BotScript* generate(Bot* bot);                            // PE 100F0240
    virtual void setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                             const Vec3& position, float distance);   // PE 100EFC80
private:
    /* +0x1C */ Vec3* m_directions;
};

class BBPAvoidCollisionInfantery : public AvoidCollisionPlanGenerator
{
public:
    explicit BBPAvoidCollisionInfantery(int behaviourId);             // PE 100EEF60
    virtual ~BBPAvoidCollisionInfantery();                            // PE 100EEFF0
    virtual BotScript* generate(Bot* bot);                            // PE 100EF0C0
    virtual void setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                             const Vec3& position, float distance);   // PE 100EF030
private:
    /* +0x1C */ Vec2* m_directions;
};

// Waypoint family. PE vtable 1027E600; it has no constructor of its own.
class GotoWaypointPlanGenerator : public PlanGenerator
{
public:
    virtual ~GotoWaypointPlanGenerator();                             // PE 100F13F0
    // Marks that the bot has a waypoint to go to.
    virtual void setWaypoint(Bot* bot) = 0;
};

class BBPGotoWaypoint2d : public GotoWaypointPlanGenerator
{
public:
    explicit BBPGotoWaypoint2d(int behaviourId);                      // PE 100F37F0
    virtual BotScript* generate(Bot* bot);                            // PE 100F3860
    virtual void setWaypoint(Bot* bot);                               // PE 100F3840
};

class BBPGotoWaypointBoat : public GotoWaypointPlanGenerator
{
public:
    explicit BBPGotoWaypointBoat(int behaviourId);                    // PE 100F2960
    virtual BotScript* generate(Bot* bot);                            // PE 100F2BE0
    virtual void setWaypoint(Bot* bot);                               // PE 100F2990
};

class BBPGotoWaypoint3d : public GotoWaypointPlanGenerator
{
public:
    explicit BBPGotoWaypoint3d(int behaviourId);                      // PE 100F2100
    virtual BotScript* generate(Bot* bot);                            // PE 100F22F0
    virtual void setWaypoint(Bot* bot);                               // PE 100F2150
};

class BBPGotoWaypointInfantery : public GotoWaypointPlanGenerator
{
public:
    explicit BBPGotoWaypointInfantery(int behaviourId);               // PE 100F1470
    virtual BotScript* generate(Bot* bot);                            // PE 100F1770
    virtual void setWaypoint(Bot* bot);                               // PE 100F14A0
};

class BBPMedicAssist : public PlanGenerator
{
public:
    explicit BBPMedicAssist(int behaviourId);                         // PE 100FF250
    virtual ~BBPMedicAssist();                                        // PE 100FF320
    virtual BotScript* generate(Bot* bot);                            // PE 100FF770
private:
    /* +0x0C */ int* m_int0C;                 // -1
    /* +0x10 */ int* m_int10;                 // -1
    /* +0x14 */ int* m_int14;                 // -1
};

// Idle family. PE vtable 10283518, constructor 101A8D80.
class IdlePlanGenerator : public PlanGenerator
{
public:
    explicit IdlePlanGenerator(int behaviourId);
    virtual ~IdlePlanGenerator();                                     // PE 101A8DB0
    // 1 for 2d, 2 for 3d and 3 for infantry; the family base reports 0. [INFERRED] name.
    virtual int getIdleType();                                        // PE 101A8DA0
};

class BBPIdle2d : public IdlePlanGenerator
{
public:
    explicit BBPIdle2d(int behaviourId);                              // PE 100F63D0
    virtual ~BBPIdle2d();                                             // PE 100F6450
    virtual BotScript* generate(Bot* bot);                            // PE 100F6490
    virtual int getIdleType();                                        // PE 100F6380
private:
    /* +0x0C */ int* m_int0C;                 // -1
};

class BBPIdle3d : public IdlePlanGenerator
{
public:
    explicit BBPIdle3d(int behaviourId);                              // PE 100F5BC0
    virtual ~BBPIdle3d();                                             // PE 100F5CA0
    virtual BotScript* generate(Bot* bot);                            // PE 100F5CE0
    virtual int getIdleType();                                        // PE 100F5A60
private:
    /* +0x0C */ float* m_float0C;             // 100; never released, as shipped
    /* +0x10 */ int*   m_int10;               // -1
};

class BBPIdleInfantery : public IdlePlanGenerator
{
public:
    explicit BBPIdleInfantery(int behaviourId);                       // PE 100F5180
    virtual ~BBPIdleInfantery();                                      // PE 100F54E0
    virtual BotScript* generate(Bot* bot);                            // PE 100F5510
    virtual int getIdleType();                                        // PE 100F51A0
};

// Fire family. PE vtable 10283DB0, constructor 101B1F10. The owning behaviour registers itself
// after construction.
class FirePlanGenerator : public PlanGenerator
{
public:
    FirePlanGenerator();
    virtual ~FirePlanGenerator();                                     // PE 101B1E40

    // Aims at a laser-designated position; the 2d, driver and helicopter variants report that they
    // cannot. [INFERRED] name; [UNRESOLVED] the designator's meaning.
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon) = 0;
    // Aims at a known object; clears the designator. [INFERRED] name.
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB) = 0;
    // 2 for 2d, 3 for the 2d driver, 4 for 3d and helicopters, 5 for infantry; 0 for the base.
    // [INFERRED] name.
    virtual int getFireType();                                        // PE 101B1E90

    void setOwner(BotBehaviour* owner) { m_owner = owner; }           // PE 101B1F00

protected:
    /* +0x0C */ unsigned int* m_targets;
    /* +0x10 */ bool*         m_targetChanged;
    /* +0x14 */ int*          m_weapons;
    /* +0x18 */ int*          m_designators;
    /* +0x1C */ Vec3*         m_positions;    // zeroed
    /* +0x20 */ bool          m_flagA;        // uninitialised
    /* +0x21 */ bool          m_flagB;        // uninitialised
    /* +0x24 */ BotBehaviour* m_owner;        // uninitialised until setOwner
};

class BBPFire2d : public FirePlanGenerator
{
public:
    explicit BBPFire2d(int behaviourId);                              // PE 1010DAD0
    virtual ~BBPFire2d();                                             // PE 1010DB10
    virtual BotScript* generate(Bot* bot);                            // PE 1010DD60
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon);
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB);
    virtual int getFireType();                                        // PE 1010D730
private:
    void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA, bool flagB,
                         bool flagC);                                 // PE 1010D740
    /* +0x28 */ bool* m_bool28;
};

// One target's state for the infantry fire generator; 0x38 bytes.
struct FireInfanteryTarget
{
    int  m_int00;                             // -1
    int  m_int04;                             // -1
    int  m_int08;                             // -1
    bool m_bool0C;
    bool m_bool14[4];                         // at +0x14
    int  m_int18[3];
    bool m_bool24[3];
    int  m_int28[4];
};

// One bot's aiming state for the infantry fire generator; 0x68 bytes.
struct FireInfanteryAim
{
    FireInfanteryAim();
    int            m_int08;                   // 0
    std::list<int> m_list0C;                  // [UNRESOLVED] element type
    int            m_int18;                   // 0
    double         m_double20;                // 0
    double         m_double28;                // 10
    int            m_int30[9];                // 0
    int            m_int58;                   // -1
    int            m_int5C;                   // -1
};

class BBPFireInfantery : public FirePlanGenerator
{
public:
    explicit BBPFireInfantery(int behaviourId);                       // PE 10105E20
    virtual ~BBPFireInfantery();                                      // PE 101061E0
    virtual BotScript* generate(Bot* bot);                            // PE 1010C110
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon);
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB);
    virtual int getFireType();                                        // PE 10105F70
private:
    /* +0x28 */ FireInfanteryTarget* m_targetState;
    /* +0x2C */ FireInfanteryAim*    m_aim;
};

class BBPFire3d : public FirePlanGenerator
{
public:
    explicit BBPFire3d(int behaviourId);                              // PE 101176B0
    virtual ~BBPFire3d();                                             // PE 10117860
    virtual BotScript* generate(Bot* bot);                            // PE 10119620
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon);
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB);
    virtual int getFireType();                                        // PE 101176D0
};

class BBPHeliFire3d : public FirePlanGenerator
{
public:
    explicit BBPHeliFire3d(int behaviourId);                          // PE 101160E0
    virtual ~BBPHeliFire3d();                                         // PE 10116410
    virtual BotScript* generate(Bot* bot);                            // PE 10117280
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon);
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB);
    virtual int getFireType();                                        // PE 10116100
};

class BBPFire2dDriver : public FirePlanGenerator
{
public:
    explicit BBPFire2dDriver(int behaviourId);                        // PE 10112630
    virtual ~BBPFire2dDriver();                                       // PE 10112670
    virtual BotScript* generate(Bot* bot);                            // PE 101127D0
    virtual void setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon);
    virtual void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                 bool flagB);
    virtual int getFireType();                                        // PE 10112420
private:
    void setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA, bool flagB,
                         bool flagC);                                 // PE 10112430
    /* +0x28 */ bool* m_bool28;
};

// Cover family layout, shared by the vehicle and infantry variants. [UNRESOLVED] The vehicle
// variant's vtable may hold three more slots (PE 101C1620, 1011B610, 101A7C60); the image has no
// RTTI to bound it and no caller of them has been read.
class BBPTakeCover : public PlanGenerator
{
public:
    explicit BBPTakeCover(int behaviourId);                           // PE 1011B7A0
    virtual ~BBPTakeCover();                                          // PE 1011B8A0
    virtual BotScript* generate(Bot* bot);                            // PE 1011BAD0
protected:
    /* +0x0C */ int*  m_int0C;
    /* +0x10 */ Vec3* m_vec10;                // zeroed
    /* +0x14 */ Vec3* m_vec14;                // zeroed
    /* +0x18 */ int*  m_int18;
    /* +0x1C */ int*  m_int1C;
};

class BBPTakeCoverInfantry : public PlanGenerator
{
public:
    explicit BBPTakeCoverInfantry(int behaviourId);                   // PE 1011E5B0
    virtual ~BBPTakeCoverInfantry();                                  // PE 1011E6B0
    virtual BotScript* generate(Bot* bot);                            // PE 1011E8B0
private:
    /* +0x0C */ int*  m_int0C;
    /* +0x10 */ Vec3* m_vec10;                // zeroed
    /* +0x14 */ Vec3* m_vec14;                // zeroed
    /* +0x18 */ int*  m_int18;
    /* +0x1C */ int*  m_int1C;
};

class BBPChange : public PlanGenerator
{
public:
    explicit BBPChange(int behaviourId);                              // PE 100F9160
    virtual ~BBPChange();                                             // PE 100F9230
    virtual BotScript* generate(Bot* bot);                            // PE 100F95E0
private:
    /* +0x0C */ int*  m_int0C;
    /* +0x10 */ Vec3* m_vec10;                // zeroed
    /* +0x14 */ bool* m_bool14;
    /* +0x18 */ bool* m_bool18;
};

class BBPChangeLandingCraft : public PlanGenerator
{
public:
    explicit BBPChangeLandingCraft(int behaviourId);                  // PE 100FBB60
    virtual ~BBPChangeLandingCraft();                                 // PE 100FBBC0
    virtual BotScript* generate(Bot* bot);                            // PE 100FBC00
private:
    /* +0x0C */ int*  m_int0C;
    /* +0x10 */ bool* m_bool10;
};

class BBPChangeTeleport : public PlanGenerator
{
public:
    explicit BBPChangeTeleport(int behaviourId);                      // PE 100FC920
    virtual ~BBPChangeTeleport();                                     // PE 100FC980
    virtual BotScript* generate(Bot* bot);                            // PE 100FC9C0
private:
    /* +0x0C */ int*  m_int0C;
    /* +0x10 */ bool* m_bool10;
};

class BBPSuicide : public PlanGenerator
{
public:
    explicit BBPSuicide(int behaviourId);                             // PE 1011FA90
    virtual BotScript* generate(Bot* bot);                            // PE 1011FB10
};

class BBPCallForMedic : public PlanGenerator
{
public:
    explicit BBPCallForMedic(int behaviourId);                        // PE 10120200
    virtual BotScript* generate(Bot* bot);                            // PE 10120280
};

class BBPRevive : public PlanGenerator
{
public:
    explicit BBPRevive(int behaviourId);                              // PE 10121000
    virtual ~BBPRevive();                                             // PE 10121060
    virtual BotScript* generate(Bot* bot);                            // PE 101210A0
private:
    /* +0x0C */ int* m_int0C;
    /* +0x10 */ int* m_int10;
};

class BBPTakeCover3d : public PlanGenerator
{
public:
    explicit BBPTakeCover3d(int behaviourId);                         // PE 10121B70
    virtual BotScript* generate(Bot* bot);                            // PE 10121BF0
};

class BBPC4 : public PlanGenerator
{
public:
    explicit BBPC4(int behaviourId);                                  // PE 10122460
    virtual ~BBPC4();                                                 // PE 101224A0
    virtual BotScript* generate(Bot* bot);                            // PE 101224E0
private:
    /* +0x0C */ int* m_int0C;
};

class BBPRunOverSoldier : public PlanGenerator
{
public:
    explicit BBPRunOverSoldier(int behaviourId);                      // PE 10125C90
    virtual ~BBPRunOverSoldier();                                     // PE 10125CD0
    virtual BotScript* generate(Bot* bot);                            // PE 10125D10
private:
    /* +0x0C */ int* m_int0C;
};

class BBPBridgeRepair : public PlanGenerator
{
public:
    explicit BBPBridgeRepair(int behaviourId);                        // PE 10123790
    virtual ~BBPBridgeRepair();                                       // PE 10123930
    virtual BotScript* generate(Bot* bot);                            // PE 10123A70
private:
    /* +0x0C */ int*  m_int0C;                // -1
    /* +0x10 */ int*  m_int10;                // -1
    /* +0x14 */ Vec3* m_vec14;                // zeroed
    /* +0x18 */ bool* m_bool18;               // false
    /* +0x1C */ Vec3* m_vec1C;                // zeroed
    /* +0x20 */ Vec3* m_vec20;                // zeroed
};

class BBPDefuseMines : public PlanGenerator
{
public:
    explicit BBPDefuseMines(int behaviourId);                         // PE 10124D10
    virtual ~BBPDefuseMines();                                        // PE 10124E40
    virtual BotScript* generate(Bot* bot);                            // PE 10124F00
private:
    /* +0x0C */ int*  m_int0C;                // -1
    /* +0x10 */ int*  m_int10;                // -1
    /* +0x14 */ Vec3* m_vec14;                // zeroed
    /* +0x18 */ Vec3* m_vec18;                // zeroed
};

class BBPRandom : public PlanGenerator
{
public:
    explicit BBPRandom(int behaviourId);                              // PE 10126370
    virtual ~BBPRandom();                                             // PE 101263B0
    virtual BotScript* generate(Bot* bot);                            // PE 101263F0
private:
    /* +0x0C */ int* m_int0C;
};

class BBPTriggerable : public PlanGenerator
{
public:
    explicit BBPTriggerable(int behaviourId);                         // PE 10127DA0
    virtual ~BBPTriggerable();                                        // PE 10127DF0
    virtual BotScript* generate(Bot* bot);                            // PE 10127E30
private:
    /* +0x0C */ int* m_int0C;
    /* +0x10 */ int  m_int10;                 // uninitialised
    /* +0x14 */ int  m_int14;                 // 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORS_H
