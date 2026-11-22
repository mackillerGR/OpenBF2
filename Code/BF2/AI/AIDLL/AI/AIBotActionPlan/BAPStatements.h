// Control-flow and primitive statements used by the plan generators: the parallel container,
// if, while, trigger, reset-controls and the wrappers. Primitive statements only carry their
// arguments; what they do is supplied per vehicle by the interpreter's entries.
// [INFERRED] placement and class names: no __FILE__ string names these classes' files.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H

#include <list>

#include "AimSolver.h"
#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"
#include <vector>
#include "BotScript.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace io { class PlayerInput; }
namespace ai {

class BAPCondition;
class Information;
class WeaponInfo;

// A statement with a list of child statements. PE vtable 10282BA4 adds the pure eighth slot.
class BAPStatementList : public BotScript
{
public:
    BAPStatementList();
    // PE 1019A280: frees the list nodes, not the children.
    virtual ~BAPStatementList();
    // Appends a child and claims it.
    virtual void addStatement(BotScript* statement) = 0;

protected:
    /* +0x18 */ std::list<BotScript*> m_children;
    /* +0x24 */ std::list<BotScript*>::iterator m_cursor;
    // Combine child failures with "and" instead of "or". [INFERRED] name.
    /* +0x28 */ bool m_failOnlyIfAllFail;
};

// Runs every child each tick; finished when all are. Type 0. PE vtable 10282BC4.
class BAPParallel : public BAPStatementList
{
public:
    // PE 1019A4B0.
    BAPParallel(int id, bool failOnlyIfAllFail, int limit, bool neverFail);
    // PE 1019A410: fails only when every child fails.
    BAPParallel(int id, int limit, bool neverFail);
    // PE 1019A540: destroys the children.
    virtual ~BAPParallel();

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019A010
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 10199B80
    virtual void addStatement(BotScript* statement);                   // PE 1019A600

private:
    // One flag per child. Execution writes every child's failure into the first node, as shipped.
    /* +0x30 */ std::list<bool> m_childFailed;
    /* +0x3C */ int m_limit;           // [UNRESOLVED] meaning; printed when not -1
    /* +0x40 */ bool m_neverFail;
};

// Runs its children one at a time. Type 1. With the base flag set (always, from this
// constructor) a child that reports failure hands over to the next, and failure is reported only
// after the last child; without it the cursor advances every call. PE vtable 10282C8C.
// [INFERRED] name, from the "Serial combiner" debug title.
class BAPSerial : public BAPStatementList
{
public:
    BAPSerial(int id, int limit);                                      // PE 1019AE30
    virtual ~BAPSerial();                                              // PE 1019AD90: destroys the children

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019ACC0
    virtual unsigned long long getInputMask();                         // PE 1019AD80
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 1019AB90
    virtual void addStatement(BotScript* statement);                   // PE 1019AF40

private:
    /* +0x30 */ int m_limit;           // [UNRESOLVED] meaning
};

// Evaluates a condition once: finished, failed when the condition holds. Type 4. PE vtable
// 102839BC. [INFERRED] name; the type's script spelling is "Wait".
class BAPWait : public BotScript
{
public:
    BAPWait(BAPCondition* condition, int id);                          // PE 101AD200
    virtual ~BAPWait();                                                // PE 101AD250

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AD280
    virtual unsigned long long getInputMask();                         // PE 101AD2B0: none
    virtual BAPStatementType getType();
    virtual bool isComposite();

    /* +0x18 */ BAPCondition* m_condition;
};

// Asks the bot to change into the target vehicle or seat. Type 29. The constructor leaves the id
// unset, as shipped. PE vtable 102839A0. [INFERRED] name.
class BAPICChangeVehicle : public BotScript
{
public:
    explicit BAPICChangeVehicle(unsigned int target);                  // PE 101AD140
    virtual ~BAPICChangeVehicle();                                     // PE 101AD170

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AD1B0
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101AD1F0: true

    /* +0x18 */ unsigned int m_target;
};

// Sets the bot's plan lock. Type 31. The constructor leaves the id unset. PE vtable 10282C24.
// [INFERRED] name.
class BAPICTurnLock : public BotScript
{
public:
    explicit BAPICTurnLock(bool lock);                                 // PE 1019AAA0
    virtual ~BAPICTurnLock();                                          // PE 1019AB40

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019AAF0
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 1019AB30: true

    /* +0x18 */ bool m_lock;
};

// Ends the plan. Mode 0 finishes when the condition is false and fails while it holds (or without
// one); mode 1 finishes with the condition's value as the failure (fails without one); mode 2
// finishes with the negated value (without one it neither finishes nor fails). Other modes write
// nothing. Owns its condition. Type 2. PE vtable 102840B0. [INFERRED] names.
class BAPPlanEnd : public BotScript
{
public:
    BAPPlanEnd(int id, int mode, BAPCondition* condition);            // PE 101B4AC0
    virtual ~BAPPlanEnd();                                             // PE 101B4C30

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B4B40
    virtual unsigned long long getInputMask();                         // PE 101B4C10: none
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101B4C20: true

    /* +0x18 */ BAPCondition* m_condition;
    /* +0x20 */ int m_mode;
};

// Makes the bot's senses forget an object. Type 39. PE vtable 102840F0. [INFERRED] name.
class BAPForgetTarget : public BotScript
{
public:
    explicit BAPForgetTarget(unsigned int target);                     // PE 101B4C80
    virtual ~BAPForgetTarget();                                        // PE 101B4D20

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B4CD0
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101B4D10: true

    /* +0x18 */ unsigned int m_target;
};

// Evaluates its condition once and keeps the value. Owns its condition. Type 43. PE vtable
// 102841A0. The retail debug print (PE 101B53D0) is not reproduced; the base print applies.
// [INFERRED] name.
class BAPEvaluateCondition : public BotScript
{
public:
    BAPEvaluateCondition(BAPCondition* condition, int id);            // PE 101B5300
    virtual ~BAPEvaluateCondition();                                   // PE 101B5450

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B5380
    virtual unsigned long long getInputMask();                         // PE 101B53B0: none
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101B53C0: true

    /* +0x18 */ BAPCondition* m_condition;
    /* +0x1C */ bool m_result;                                         // not initialised
};

// Asks the bot to re-read its equipment. Type 27. PE vtable 10283858. [INFERRED] name.
class BAPUpdateVehicle : public BotScript
{
public:
    explicit BAPUpdateVehicle(int id);                                 // PE 101AC540
    virtual ~BAPUpdateVehicle();                                       // PE 101AC5E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AC5B0
    virtual unsigned long long getInputMask();                         // PE 101AC570: none
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101AC5D0: true
};

// Moves an aircraft toward an object; the per-vehicle MoveToObject entries drive it. Never
// driven directly: unfinished and failed. Type 9. PE vtable 102832F8. [INFERRED] names; the
// fields after the stop condition are [UNRESOLVED] working state.
class BAPAirMoveToObject : public BotScript
{
public:
    // PE 101A7D40. Owns the stop condition and becomes its owner.
    BAPAirMoveToObject(unsigned int target, float range, float speed, float cruiseSpeed,
                       float altitude, BAPCondition* until, unsigned long long inputMask, int id);
    virtual ~BAPAirMoveToObject();                                     // PE 101A7E50

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A7D20
    virtual BAPStatementType getType();                                // PE 101A7CD0: 9
    virtual bool isComposite();                                        // PE 101A7CE0: false
    // Whether the stop condition holds; false without one. PE 101A7CF0.
    virtual bool isFinished(Bot* bot);

    /* +0x18 */ unsigned int m_target;
    /* +0x1C */ float m_range;
    /* +0x20 */ float m_speed;
    /* +0x24 */ float m_cruiseSpeed;
    /* +0x28 */ BAPCondition* m_until;
    /* +0x2C */ float m_state2C[10];         // zeroed
    /* +0x54 */ float m_altitude;
    /* +0x58 */ float m_state58[2];          // left unset, as shipped
    /* +0x60 */ float m_state60[9];          // zero except +0x64, +0x74 and +0x78 = 1
    // Whether the target was a known mobile object at construction.
    /* +0x84 */ bool m_targetMobile;
    /* +0x88 */ int m_int88;                 // zeroed
};

// Fires a flare once the equipment's missile-lock status turns positive, after a second's delay
// for equipment with type flag 4, then clears the status. Type 40. PE vtable 10283338.
// [INFERRED] name.
class BAPUseFlareOnMissileLock : public BotScript
{
public:
    // PE 101A7F20.
    BAPUseFlareOnMissileLock(int arg, unsigned long long inputMask, int id);
    virtual ~BAPUseFlareOnMissileLock();                               // PE 101A7FC0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A8000
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101A7FB0: true

    /* +0x18 */ int m_arg18;          // [UNRESOLVED] stored, not read
    /* +0x20 */ double m_lockTime;    // = -1
    /* +0x28 */ bool m_locked;        // = false
};

// Queues a radio message on the bot's communication agent. Type 37. PE vtable 10283780.
// [INFERRED] name.
class BAPSendRadioMessage : public BotScript
{
public:
    // PE 101AA140.
    BAPSendRadioMessage(int type, int channel, unsigned int priority, float delay, int id, int flag);
    virtual ~BAPSendRadioMessage();                                    // PE 101AA210

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AA1C0
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101AA200: true

    /* +0x18 */ int m_type;
    /* +0x1C */ int m_channel;
    /* +0x20 */ unsigned int m_priority;
    /* +0x24 */ int m_flag;
    /* +0x28 */ float m_delay;
};

// Chooses the soldier pose for a moving bot from two measures the caller supplies, re-deciding
// at most once per interval. Lying is only kept for kit types 1 and 3 (others crouch); water
// deeper than 0.4 forces standing and any water turns lying into crouching. With a companion
// object that is a human-controlled soldier not standing, the bot crouches. The path-following
// moves carry one as a second base. PE vtable 102853C0, fifteen slots. [INFERRED] name and
// member names.
class BAPSoldierPoseSelector
{
public:
    // PE 101DEED0. The threshold is scaled by a random factor in [1, 6); the limit is clamped
    // to [-1, 1].
    BAPSoldierPoseSelector(int defaultPose, int abovePose, float threshold, int belowPose, float limit,
                           unsigned int companion, float interval);
    virtual ~BAPSoldierPoseSelector();                                 // PE 101DEF80
    virtual void setDefaultPose(int pose);                             // PE 101DEE10
    virtual void setAbovePose(int pose);                               // PE 101DEE20
    virtual void setThreshold(float threshold);                        // PE 101DEE30
    virtual void setBelowPose(int pose);                               // PE 101DEE40
    virtual void setLimit(float limit);                                // PE 101DEE50
    virtual void setInterval(float interval);                          // PE 101DEE60
    // The pose to start from: crouching beside a non-standing human companion, else the default.
    virtual int getPose();                                             // PE 101DEFB0
    virtual int getAbovePose();                                        // PE 101DEE70
    virtual float getThreshold();                                      // PE 101DEE80
    virtual int getBelowPose();                                        // PE 101DEE90
    virtual float getLimit();                                          // PE 101DEEA0
    virtual float getInterval();                                       // PE 101DEEB0
    virtual int getCurrentPose();                                      // PE 101DEEC0
    // Re-decides when the interval has passed and returns the current pose: the above pose
    // when `measure` exceeds the threshold, else the below pose when `value` is under the limit,
    // else the default. PE 101DF040.
    virtual int update(float measure, float value, double time, Bot* bot, const Vec3& position);

protected:
    /* +0x08 */ int m_defaultPose;
    /* +0x0C */ int m_abovePose;
    /* +0x10 */ float m_threshold;
    /* +0x14 */ int m_belowPose;
    /* +0x18 */ int m_currentPose;
    /* +0x1C */ float m_limit;
    /* +0x20 */ float m_interval;
    /* +0x28 */ double m_lastTime;     // starts at the lowest float
    /* +0x30 */ unsigned int m_companion;
};

// Asks the soldier for a pose chosen by its pose selector; driven by the interpreter's soldier
// pose entry, which counts it done once the equipment holds the pose. Drives the crouch and lie
// inputs. Type 26. PE vtable 102835AC with the pose selector at +0x18 (PE vtable 10283570).
// [INFERRED] class name, after the statement type's name "SoldierPose".
class BAPSoldierPose : public BotScript, public BAPSoldierPoseSelector
{
public:
    // One pose throughout. PE 101A9130.
    BAPSoldierPose(int pose, BAPCondition* until, int id, bool flag);
    // One pose, crouching beside a non-standing human companion. PE 101A91B0.
    BAPSoldierPose(int pose, BAPCondition* until, int id, unsigned int companion, bool flag);
    // The full selector, an optional stop condition and a second owned condition. PE 101A92D0.
    BAPSoldierPose(int defaultPose, int abovePose, int threshold, int belowPose, float limit,
                   BAPCondition* until, BAPCondition* condition, int id, bool flag);
    virtual ~BAPSoldierPose();                                         // PE 101A9430

    // Never driven directly: unfinished and failed. PE 101A93E0.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);
    virtual BAPStatementType getType();                                // PE 101A93C0: 26
    virtual bool isComposite();                                        // PE 101A93D0: false

    /* +0x50 */ BAPCondition* m_until;      // owned
    /* +0x54 */ BAPCondition* m_condition;  // owned [UNRESOLVED] role
    // Handed to requestSoldierPose with the pose. [UNRESOLVED] meaning.
    /* +0x58 */ bool m_flag58;
};

// Base of the moves that follow a path from the bot's navigation. Type 7. Construction clears
// the bot's move-to status. PE vtable 10282D18 (twenty-one slots) with the pose selector at
// +0x18 (PE vtable 10282CD8). An empty intermediate class (PE vtable 10282AF4) between
// BotScript and this one is folded in. [INFERRED] name, after the attested BAPALook; slot and
// member names are [INFERRED] where given and [UNRESOLVED] where numbered.
class BAPAMove : public BotScript, public BAPSoldierPoseSelector
{
public:
    explicit BAPAMove(Bot* bot);                                       // PE 1019B250
    virtual ~BAPAMove();                                               // PE 1019B360

    virtual unsigned long long getInputMask();                         // PE 1019B1B0
    virtual BAPStatementType getType();
    virtual bool isComposite();
    // Whether the move asked the sensing agent to focus.
    virtual bool isFocusing();                                         // PE 1019B1C0
    virtual bool slot8();                                              // PE 1019B100: false
    virtual const Vec2& getSteerPoint();                               // PE 1019B110
    // Works out the point to steer toward. [UNRESOLVED] the fourth argument.
    virtual bool getSteering(Vec2& point, bool& flag, bool& failed, int arg);   // PE 1019B120: false
    // Whether the stop condition holds; false without one.
    virtual bool isFinished(Bot* bot);                                 // PE 1019B130
    virtual bool getPathPoint(Vec2& point);                            // PE 1019B160: false
    virtual bool getPathTurn(float& turn);                             // PE 1019B170: false
    virtual float slot14();                                            // PE 1019B180: 0
    virtual unsigned int getTargetObject();                            // PE 1019B190: -1
    // The equipment's distance to the target; the equipment is not null-checked.
    virtual float getDistance(Bot* bot);                               // PE 1019B430
    virtual float getDistanceSq(Bot* bot);                             // PE 1019B4A0
    virtual bool slot18() = 0;
    virtual bool isWithinRange() = 0;
    virtual bool slot20();                                             // PE 1019B1A0: false

    // Turns the focus request on or off; when on, points the bot's sensing agent at the object,
    // or at the position when the object is -1. PE 1019B3C0.
    void setFocus(Bot* bot, bool focus, const Vec3& position, unsigned int object);
    // Steers around the nearest obstacle on the way to `steer`: a direction from the equipment
    // in `out`. Recomputed at most once per game time step; true when no avoidance is needed
    // (then `out` is untouched or the plain direction), false with an avoiding unit direction.
    // `ignore` and `allowIgnored` are handed to the obstacle search. PE 1019B510; [INFERRED] name.
    bool getAvoidanceSteering(Bot* bot, Vec2 steer, Vec2& out, Information* ignore, bool allowIgnored);

    /* +0x50 */ bool m_focusing;           // = false
    /* +0x54 */ Vec3 m_target;             // = 0
    /* +0x60 */ float m_float60;           // set by the derived constructors [UNRESOLVED]
    /* +0x64 */ float m_float64;           // likewise [UNRESOLVED]
    /* +0x68 */ BAPCondition* m_until;     // owned; set by the derived constructors
    /* +0x6C */ bool m_flag6C;             // [UNRESOLVED]
    /* +0x70 */ int m_int70;               // = 0 [UNRESOLVED]
    /* +0x74 */ float m_float74;           // = 1 [UNRESOLVED]
    /* +0x78 */ int m_int78;               // = 0 [UNRESOLVED]
    /* +0x7C */ Vec2 m_steerPoint;         // = 0
    /* +0x84 */ int m_int84;               // = 0 [UNRESOLVED]
    /* +0x88 */ int m_int88;               // = 0 [UNRESOLVED]
    /* +0x8C */ int m_int8C;               // left unset [UNRESOLVED]
    // The obstacle being avoided and how ([INFERRED] names, from getAvoidanceSteering).
    /* +0x90 */ unsigned int m_avoidObject;   // = -1; none
    /* +0x94 */ Vec2 m_avoidPoint;            // = 0; the point steered for beside it
    /* +0x9C */ float m_avoidRadius;          // left unset
    /* +0xA0 */ float m_avoidOffset;          // left unset; the sideways distance chosen
    /* +0xA4 */ Vec2 m_avoidCentre;           // = 0; the obstacle's flat position when chosen
    /* +0xAC */ bool m_avoidingTarget;        // left unset; a change/repair target or unhealthy
    /* +0xB0 */ double m_nextAvoidTime;       // = -1000; the game time of the cached answer
    /* +0xB8 */ Vec2 m_cachedSteer;           // = 0
    /* +0xC0 */ bool m_cachedAvoidResult;     // left unset
};

// Free helpers of the infantry move entries, used by BAPAMove too. [INFERRED] names and
// placement; defined in BAPEntries.cpp.
//
// Whether an avoided obstacle (handle) is no longer where it was chosen: false only while its
// flat position lies between `radius` and `radius` + 1.5 from `point`. PE 101C42B0.
bool hasAvoidedObstacleMoved(const Vec2& point, unsigned int object, float radius);
// Picks the nearest sensed obstacle ahead on the line from the equipment to `towards` and the
// side to pass it on, tracing 15 to either side. Writes the avoidance point and the obstacle's
// records; false either way except when the obstacle stands off the mesh (true). PE 101C4AA0.
bool findObstacleToAvoid(Bot* bot, const Vec2& towards, const Vec3& target, unsigned int targetObject,
                         Vec2& avoidPoint, unsigned int& avoidObject, float& avoidOffset, Vec2& avoidCentre,
                         float& avoidRadius, bool& avoidingTarget, Information* ignore, bool allowIgnored);

// Follows a path from the bot's navigation to a position until it is within the range band
// or the stop condition holds. Type 7. PE vtable 10283250 (pose selector at +0x18: 10283210).
// The class name follows the debug output "MoveToFinding(" with the tree's BAP prefix
// [INFERRED].
class BAPMoveToFinding : public BAPAMove
{
public:
    // PE 101A76B0. Reads the bot's current equipment and drops it, as shipped.
    BAPMoveToFinding(Bot* bot, const Vec3& target, float float60, float float64, BAPCondition* until,
                     unsigned long long inputMask, int id, float maxRange, float minRange);
    virtual ~BAPMoveToFinding();                                       // PE 101A77D0

    // Never driven directly: unfinished and failed. PE 101A74F0.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);
    virtual void printDebug(std::ostream& out);                        // PE 101A7620
    // Requests the path when the target or the navigation's goal changed, follows it, and close
    // to the target looks for a free spot around it. PE 101A7800.
    virtual bool getSteering(Vec2& point, bool& arrived, bool& failed, int arg);
    // Finished by the stop condition or by being within range; then the navigation's progress
    // is cleared. PE 101A75D0.
    virtual bool isFinished(Bot* bot);
    virtual bool getPathPoint(Vec2& point);                            // PE 101A7510
    virtual bool getPathTurn(float& turn);                             // PE 101A7530
    // The navigation's distance left along the path. PE 101A7550.
    virtual float slot14();
    virtual bool slot18();                                             // PE 101A74E0: true
    // Whether the squared distance lies between the minimum and maximum squared. PE 101A7590.
    virtual bool isWithinRange();
    // Whether the navigation is waiting for a path search. PE 101A7570.
    virtual bool slot20();

    /* +0xC8 */ int m_zeroC8[8];           // = 0 [UNRESOLVED]
    /* +0xE8 */ bool m_flagE8;             // = false [UNRESOLVED]
    /* +0xE9 */ bool m_flagE9;             // = false [UNRESOLVED]
    /* +0xEC */ int m_intEC;               // = 1 [UNRESOLVED]
    /* +0xF0 */ Bot* m_bot;
    /* +0xF4 */ float m_maxRange;
    /* +0xF8 */ float m_minRange;
    /* +0xFC */ float m_maxRangeSq;
    /* +0x100 */ float m_minRangeSq;
    /* +0x104 */ Vec2 m_freeSpot;          // = 0; a walkable point around the target
    /* +0x10C */ bool m_arrived;           // = false
    /* +0x10D */ bool m_pathRequested;     // = false
};

// Base of the moves toward an object. Type 9. Finished by the stop condition or once the
// equipment is closer to the object than the range; then the navigation's progress is cleared.
// PE vtable 10283068 (pose selector at +0x18: 10283028). The class name is [INFERRED], after
// BAPAMove and the statement type's name "MoveToObject".
class BAPAMoveToObject : public BAPAMove
{
public:
    // PE 101DF400.
    BAPAMoveToObject(Bot* bot, unsigned int target, float range, float float60, float float64,
                     BAPCondition* until, unsigned long long inputMask, int id);
    virtual ~BAPAMoveToObject();                                       // PE 101A5E70

    // Never driven directly: unfinished and failed. PE 101DF4D0.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);
    virtual BAPStatementType getType();                                // PE 101A5E40: 9
    virtual void printDebug(std::ostream& out);                        // PE 101DF4F0
    virtual bool isFinished(Bot* bot);                                 // PE 101DF470
    virtual unsigned int getTargetObject();                            // PE 101A5E60
    // The distance between the equipment and the object; neither lookup is null-checked.
    virtual float getDistance(Bot* bot);                               // PE 101DF560
    virtual float getDistanceSq(Bot* bot);                             // PE 101DF600

    /* +0xC8 */ unsigned int m_targetObject;
    /* +0xCC */ float m_range;
    /* +0xD0 */ float m_rangeSq;
};

// Follows the bot's navigation toward an object until it is within the range band or the stop
// condition holds. Type 9. PE vtable 102830F8 (pose selector at +0x18: 102830BC). The class
// name follows the debug output "MoveToObjectFinding" with the tree's BAP prefix [INFERRED];
// member names are [INFERRED] where given and [UNRESOLVED] where numbered.
class BAPMoveToObjectFinding : public BAPAMoveToObject
{
public:
    // PE 101A6130. A minimum range above the range is clamped to it. Owns both conditions.
    BAPMoveToObjectFinding(Bot* bot, unsigned int target, float range, float float60, float float64,
                           BAPCondition* until, unsigned long long inputMask, int id,
                           BAPCondition* condition, float minRange, int int120);
    virtual ~BAPMoveToObjectFinding();                                 // PE 101A6250

    virtual void printDebug(std::ostream& out);                        // PE 101A6060
    virtual bool slot8();                                              // PE 101A5F50
    // Follows the navigation toward the object's last valid position, choosing a new goal when the
    // object moved, the path's goal is stale or the bot came too close; keeps the sensing agent on
    // the object while a soldier with a usable weapon is near it. PE 101A6A80.
    virtual bool getSteering(Vec2& point, bool& arrived, bool& failed, int arg);
    // The stop condition, or the range band; once m_flag11D is set the band counts only with
    // m_flag11C. Either way the navigation's progress is then cleared. PE 101A5FE0.
    virtual bool isFinished(Bot* bot);
    virtual bool getPathTurn(float& turn);                             // PE 101A5F80
    // The navigation's distance left along the path. PE 101A5F60.
    virtual float slot14();
    virtual float getDistance(Bot* bot);                               // PE 101A68C0
    virtual float getDistanceSq(Bot* bot);                             // PE 101A6960
    virtual bool slot18();                                             // PE 101A5F40: true
    // Whether the squared distance lies between the minimum and the range squared. PE 101A5FA0.
    virtual bool isWithinRange();

    // Picks the goal and requests the path. Too close to the object (closer than the minimum
    // range) and not asked to go straight, it traces up to three points around the object (to
    // the side of its travel, or around the bot's facing) and heads for the first one the trace
    // reaches, else for the farthest blocked one; otherwise it heads for the object's last valid
    // position. `unused` is never read. PE 101A62A0; [INFERRED] name.
    void chooseGoal(float distance, const Vec3* unused, const Vec3* from, unsigned int island,
                    unsigned int polygon, const Vec3* targetPosition, bool direct);

    /* +0xD4 */ bool m_active;              // = true
    /* +0xE0 */ int m_zeroE0[8];            // = 0 [UNRESOLVED]
    /* +0x101 */ bool m_flag101;            // = false [UNRESOLVED]
    /* +0x102 */ bool m_flag102;            // cleared by chooseGoal [UNRESOLVED]
    /* +0x104 */ int m_int104;              // = 1 [UNRESOLVED]
    /* +0x108 */ Bot* m_bot;
    /* +0x10C */ BAPCondition* m_condition; // owned [UNRESOLVED] role
    /* +0x110 */ bool m_flag110;            // = true [UNRESOLVED]
    /* +0x114 */ float m_minRange;
    /* +0x118 */ float m_minRangeSq;
    /* +0x11C */ bool m_flag11C;            // = false [UNRESOLVED]
    /* +0x11D */ bool m_flag11D;            // = false [UNRESOLVED]
    /* +0x11E */ bool m_flag11E;            // = false [UNRESOLVED]
    /* +0x11F */ bool m_flag11F;            // = false [UNRESOLVED]
    /* +0x120 */ int m_int120;              // [UNRESOLVED]
    /* +0x128 */ double m_time128;          // = -1 [UNRESOLVED]
};

// Hands the bot's sensing agent a look target: a direction from the equipment, or an object.
// Type 35. PE vtable 102831D0. The class name follows the statement type's name
// "SetSensingAgentFocus" with the tree's BAP prefix [INFERRED].
class BAPSetSensingAgentFocus : public BotScript
{
public:
    // PE 101A7460: the direction form.
    BAPSetSensingAgentFocus(float weight, const Vec3& direction, int id, bool flag);
    virtual ~BAPSetSensingAgentFocus();                                // PE 101A7420

    // Finishes at once with both flags set. PE 101A7340.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);
    virtual BAPStatementType getType();                                // PE 101A7410
    virtual bool isComposite();                                        // PE 101A73B0: true

    /* +0x18 */ float m_weight;
    /* +0x1C */ bool m_flag;
    /* +0x20 */ unsigned int m_object;     // not set by the direction form
    /* +0x24 */ Vec3 m_direction;          // = 0, then the argument
    /* +0x30 */ bool m_useObject;          // = false
};

// Moves the equipment to a position; the per-vehicle MoveTo entries drive it. Type 7. The
// constructor caches the equipment's altitude along its speed and the axes of its
// transformation. PE vtable 10282EB8. [INFERRED] name and field meanings.
class BAPMoveTo : public BotScript
{
public:
    // PE 1019D160. `speed` and `cruiseSpeed` come from the vehicle's maximum speed in the callers.
    BAPMoveTo(Bot* bot, const Vec3& target, float speed, float cruiseSpeed, BAPCondition* until,
              unsigned long long inputMask, float radius, int id);
    virtual ~BAPMoveTo();                                              // PE 1019D2B0

    // The fallback when no entry is installed: unfinished and failed.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019D0E0
    virtual unsigned long long getInputMask();                         // PE 1019D100
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 1019D110: false
    // Whether the stop condition holds; false without one. PE 1019D0B0; [INFERRED] name.
    virtual bool isStopConditionMet(Bot* bot);

    /* +0x18 */ int m_int18;               // = 0 [UNRESOLVED]
    /* +0x1C */ int m_int1C;               // = 0 [UNRESOLVED]
    /* +0x20 */ int m_int20;               // = 0 [UNRESOLVED]
    /* +0x24 */ float m_speed;
    /* +0x28 */ float m_cruiseSpeed;
    /* +0x2C */ BAPCondition* m_until;
    /* +0x30 */ Vec3 m_target;
    /* +0x3C */ int m_int3C;               // left unset [UNRESOLVED]
    /* +0x40 */ Vec3 m_vec40;              // = 0 [UNRESOLVED]
    /* +0x4C */ float m_radius;
    /* +0x50 */ int m_int50;               // = 0 [UNRESOLVED]
    /* +0x54 */ int m_int54;               // = 0 [UNRESOLVED]
    /* +0x58 */ float m_altitude;
    /* +0x5C */ Vec3 m_forward;            // transformation row 2
    /* +0x68 */ Vec3 m_up;                 // transformation row 1
    /* +0x74 */ Vec3 m_right;              // transformation row 0
};

// Releases the bot's controls in up to three groups. Type 14. PE vtable 10283378.
class BAPResetControls : public BotScript
{
public:
    // PE 101A80F0.
    BAPResetControls(bool movement, bool look, bool actions, int id);
    virtual ~BAPResetControls();

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A8210
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101A8230

    /* +0x18 */ bool m_movement;
    /* +0x19 */ bool m_look;
    /* +0x1A */ bool m_actions;
    // The single input released when the statement has an input mask. PE 101A8200 reads it.
    /* +0x1C */ int m_input;
};

// Runs `then` while the condition holds and `otherwise` while it does not. Type 3.
// PE vtable 102835E8.
class BAPIf : public BotScript
{
public:
    // PE 101A94A0.
    BAPIf(BAPCondition* condition, BotScript* then, BotScript* otherwise, int id,
          bool reevaluate, bool reevaluateOnlyInElse);
    virtual ~BAPIf();                                                  // PE 101A98B0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A9570
    virtual unsigned long long getInputMask();                         // PE 101A9600
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void getDebugString(Bot* bot, std::string& out);           // PE 101A9920
    virtual void printDebug(std::ostream& out);                        // PE 101A96A0

private:
    /* +0x18 */ BAPCondition* m_condition;
    /* +0x20 */ BotScript* m_then;
    /* +0x24 */ BotScript* m_otherwise;
    /* +0x28 */ BotScript* m_current;
    /* +0x2C */ BotScript* m_lastChosen;
    // [INFERRED] names.
    /* +0x30 */ bool m_reevaluate;
    /* +0x31 */ bool m_reevaluateOnlyInElse;
};

// Presses one input until a condition holds. Type 11. PE vtable 10283728.
class BAPTrigger : public BotScript
{
public:
    // PE 101A9E00.
    BAPTrigger(int input, BAPCondition* until, BAPCondition* release,
               unsigned long long inputMask, int id, double delay, bool flag);
    virtual ~BAPTrigger();                                             // PE 101AA0E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A9EF0
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void getDebugString(Bot* bot, std::string& out);           // PE 101A9F10
    virtual void printDebug(std::ostream& out);                        // PE 101A9FA0

    /* +0x18 */ int m_input;
    /* +0x1C */ BAPCondition* m_until;    // [INFERRED] names of both conditions
    /* +0x20 */ BAPCondition* m_release;
    /* +0x24 */ bool m_pressed;           // = false
    /* +0x28 */ double m_startTime;       // game time at construction plus the delay
    /* +0x30 */ bool m_flag30;            // [UNRESOLVED]
};

// Runs its body while the condition holds. Type 5. PE vtable 102833E4.
class BAPWhile : public BotScript
{
public:
    // PE 101A83A0.
    BAPWhile(BAPCondition* condition, BotScript* body, int id);
    // PE 101A8400.
    BAPWhile(BAPCondition* condition, BotScript* body, bool testAfterFailure, int id);
    virtual ~BAPWhile();                                               // PE 101A86C0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A84A0
    virtual unsigned long long getInputMask();                         // PE 101A8570
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101A85D0

private:
    /* +0x18 */ BAPCondition* m_condition;
    /* +0x20 */ BotScript* m_body;
    // [INFERRED] names.
    /* +0x24 */ bool m_abort;             // = false
    /* +0x25 */ bool m_testCondition;     // = true
    /* +0x26 */ bool m_testAfterFailure;  // = true
};

// A statement that runs one child. Type 15. PE vtable 10285420.
class BAPWrapper : public BotScript
{
public:
    // PE 101DF240.
    BAPWrapper(BotScript* child, int id);
    virtual ~BAPWrapper();                                             // PE 101DF2E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101DF2B0
    virtual unsigned long long getInputMask();                         // PE 101A5D50
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101DF320
    // Index into the wrapper name table. [INFERRED] name.
    virtual int getWrapperIndex();

    /* +0x18 */ BotScript* m_child;
};

// The root of a change plan: books the target vehicle for the player while it lives. On
// destruction it cancels the booking and, when the bot sits in the target, thanks the occupant
// that asked for the ride (radio message 8) or reports the vehicle full (11), then deletes the
// child. PE vtable 10283960. [INFERRED] name.
class BAPChangeWrapper : public BAPWrapper
{
public:
    // PE 101ACE20. Books the target, even when it is unknown (the booking then has no object).
    BAPChangeWrapper(unsigned int target, BotScript* child, int id, int playerId);
    virtual ~BAPChangeWrapper();                                       // PE 101ACEB0
    virtual int getWrapperIndex();                                     // PE 101ACEA0: 8

    /* +0x20 */ unsigned int m_target;
    /* +0x24 */ int m_playerId;
};

// The root of a generated plan: remembers where it was made and marks the bot's squad member
// as running a plan while it lives. PE vtable 10283550.
class BAPInfoWrapper : public BAPWrapper
{
public:
    // PE 101A9060.
    BAPInfoWrapper(const Vec3& position, BotScript* child, int id, Bot* bot);
    virtual ~BAPInfoWrapper();                                         // PE 101A90D0
    virtual int getWrapperIndex();

    /* +0x20 */ Vec3 m_position;
    /* +0x2C */ Bot* m_bot;
};

// A wrapper that tags its child as the MoveTo behaviour (wrapper name index 2) and remembers
// the target position. PE vtable 10282FE8. [INFERRED] name.
class BAPMoveToWrapper : public BAPWrapper
{
public:
    // PE 101A5D90.
    BAPMoveToWrapper(const Vec3& position, BotScript* child, int id);
    virtual ~BAPMoveToWrapper();                                       // PE 101A5DD0
    virtual int getWrapperIndex();                                     // PE 101A5D80

    /* +0x20 */ Vec3 m_position;
};

// Steers the soldier's movement relative to where it looks. Type 36. PE vtable 10282B10.
class BAPMergeSoldierInputs : public BotScript
{
public:
    explicit BAPMergeSoldierInputs(int id);                            // PE 10199030
    virtual ~BAPMergeSoldierInputs();                                  // PE 10199140

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101991F0
    virtual BAPStatementType getType();
    virtual bool isComposite();

    // Last tick's look-controller errors, fed back as the derivative's history.
    /* +0x18 */ float m_yawError;
    /* +0x1C */ float m_pitchError;
};

// Turns the bot's look toward a target. Type 23. PE vtable 102854C8; the target comes from the
// eighth slot. The name is attested by SensingAgentSoldierPattern::endPlan's signature.
class BAPALook : public BotScript
{
public:
    // PE 101DF6E0.
    BAPALook(float deadZone, BAPCondition* until, unsigned long long inputMask, int id);
    virtual ~BAPALook();                                               // PE 101DF740

    // The fallback when no entry is installed: unfinished and failed.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101DF6C0
    virtual BAPStatementType getType();
    virtual bool isComposite();
    // Where to look, and whether that is known. [INFERRED] name.
    virtual Vec3 getTarget(bool& valid) = 0;

    /* +0x18 */ BAPCondition* m_until;
    /* +0x1C */ float m_deadZone;     // the look controller's dead zone
    // Controller state the look entry carries between ticks: two values it feeds back to the
    // controller (+0x20 is reset to pi when the look ends) and the last camera forward.
    /* +0x20 */ float m_state20;
    /* +0x24 */ float m_state24;
    /* +0x28 */ Vec3  m_lastForward;
};

// Looks along a fixed direction. PE vtable 10284990. [INFERRED] name.
class BAPALookDirection : public BAPALook
{
public:
    // PE 101C17B0.
    BAPALookDirection(const Vec3& direction, float deadZone, BAPCondition* until,
                      unsigned long long inputMask, int id);
    virtual ~BAPALookDirection();                                      // PE 101C1800
    virtual Vec3 getTarget(bool& valid);                               // PE 101C1780

    /* +0x38 */ Vec3 m_direction;
};

// Looks at an object, or at a plan position when one is given, raised by a height offset.
// PE vtable 10283AA8. [INFERRED] names.
class BAPALookAtObject : public BAPALook
{
public:
    // PE 101AD7D0. `position` may be null.
    BAPALookAtObject(Bot* bot, unsigned int object, float deadZone, BAPCondition* until,
                     unsigned long long inputMask, int id, BotScriptValue<Vec3>* position,
                     float heightOffset);
    virtual ~BAPALookAtObject();                                       // PE 101AD850
    virtual Vec3 getTarget(bool& valid);                               // PE 101AD890

    /* +0x38 */ Bot* m_bot;
    /* +0x3C */ unsigned int m_object;
    /* +0x40 */ BotScriptValue<Vec3>* m_position;
    /* +0x44 */ float m_heightOffset;
};

// Type 6: an aim solution that the fire statements query. Executing it directly fails at once;
// the derived classes solve for an object or a fixed position. The base takes an optional pair
// of plan values; only the object aim reads them. PE vtable 10283EE8, ctor 101C87D0.
// [INFERRED] class and slot names.
class BAPAimAt : public BotScript
{
public:
    BAPAimAt(BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction);
    virtual ~BAPAimAt();                                               // PE 101B3680

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101C8660
    virtual BAPStatementType getType();                                // PE 101B3660: 6
    virtual bool isComposite();                                        // PE 101B3670: false

    // True when the aim follows an object.
    virtual bool tracksObject() = 0;
    // Solves the aim for a muzzle `frame` and the shooter's velocity, writing the direction.
    // `effort` scales the solver's iterations and `lob` asks for indirect fire.
    virtual bool computeAim(const Mat4& frame, const Vec3& ownVelocity, Vec3& direction,
                            float effort, float drag, float speed, float gravityModifier,
                            bool lob, Bot* bot) = 0;
    // The aimed-at point relative to `from`; false without a target.
    virtual bool getRelativePosition(const Vec3& from, Vec3& out) = 0;
    virtual const Mat4& getFrame() = 0;
    virtual bool isSolved() = 0;
    // The last solved direction; false until a solve succeeded.
    virtual bool getAimDirection(Vec3& out) = 0;
    // The predicted target point of the last solve, in world space.
    virtual bool getAimPoint(Vec3& out) = 0;
    virtual bool getImpactTime(double& out) = 0;
    // Where a projectile fired along the frame is after `time` seconds, in world space.
    virtual bool getProjectilePosition(Vec3& out, double time) = 0;

public:
    // Clears the bot's aim-correction offset and returns it. PE 101C88D0.
    Vec3& clearFireOffset(Bot* bot);

    /* +0x18 */ Vec3 m_offset;                    // added to the aimed-at point
    /* +0x24 */ Vec3 m_unresolved24[4];           // zeroed by the ctor [UNRESOLVED]
    /* +0x54 */ Vec3 m_unresolved54[4];           // zeroed by the ctor [UNRESOLVED]
    /* +0x84 */ char m_gap84[0x1C];               // [UNRESOLVED] not initialised
    /* +0xA0 */ double m_double0A0;               // -5e9 [UNRESOLVED]
    /* +0xA8 */ BAPCondition* m_condition;        // owned; stops the turret aim entry when true
    /* +0xAC */ char m_gapAC[4];
    /* +0xB0 */ AimSolver m_solver;
public:
    // Steering state of the turret aim entry: last tick's yaw error (pi after a stop), the last
    // ten pitch errors and the ring position. [INFERRED] names.
    /* +0x120 */ float m_previousYaw;
    /* +0x124 */ float m_pitchHistory[10];
    /* +0x14C */ int m_pitchIndex;
    /* +0x150 */ int m_unresolved150[3];          // +0x150 and +0x158 zeroed [UNRESOLVED]
    /* +0x15C */ BotScriptValue<bool>* m_valid;
    /* +0x160 */ BotScriptValue<Vec3>* m_direction;
};

// Aims at an object, at the bot's sensed point on it or, when allowed, at its position.
// PE vtable 10283F28. [INFERRED] name.
class BAPAimAtObject : public BAPAimAt
{
public:
    // PE 101B37C0. Owns `condition`.
    BAPAimAtObject(Bot* bot, unsigned int target, const Vec3& offset, BAPCondition* condition,
                   unsigned long long inputMask, int id, bool usePosition,
                   BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction);
    virtual ~BAPAimAtObject();                                         // PE 101B3990

    virtual bool tracksObject();                                       // PE 101B3710: true
    virtual bool computeAim(const Mat4& frame, const Vec3& ownVelocity, Vec3& direction,
                            float effort, float drag, float speed, float gravityModifier,
                            bool lob, Bot* bot);                       // PE 101B3AF0
    virtual bool getRelativePosition(const Vec3& from, Vec3& out);     // PE 101B39F0
    virtual const Mat4& getFrame();                                    // PE 101B3760
    virtual bool isSolved();                                           // PE 101B3720
    virtual bool getAimDirection(Vec3& out);                           // PE 101B3A70
    virtual bool getAimPoint(Vec3& out);                               // PE 101B3AB0
    virtual bool getImpactTime(double& out);                           // PE 101B3730
    virtual bool getProjectilePosition(Vec3& out, double time);        // PE 101B3770

private:
    /* +0x164 */ char m_gap164[4];
    /* +0x168 */ Bot* m_bot;
    /* +0x16C */ unsigned int m_target;
    /* +0x170 */ Vec3 m_aimDirection;
    /* +0x17C */ Vec3 m_aimPoint;
    /* +0x188 */ Mat4 m_frame;
    /* +0x1C8 */ double m_impactTime;
    /* +0x1D0 */ bool m_solved;
    /* +0x1D1 */ bool m_usePosition;              // aim at the position when nothing is sensed
};

// Aims at a position held in a plan value. PE vtable 10283F90. [INFERRED] name.
class BAPAimAtPosition : public BAPAimAt
{
public:
    // PE 101B3F30. Owns `condition`.
    BAPAimAtPosition(Bot* bot, BotScriptValue<Vec3>* position, const Vec3& offset,
                     BAPCondition* condition, unsigned long long inputMask, int id,
                     BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction);
    virtual ~BAPAimAtPosition();                                       // PE 101B4050

    virtual bool tracksObject();                                       // PE 101B3E80: false
    virtual bool computeAim(const Mat4& frame, const Vec3& ownVelocity, Vec3& direction,
                            float effort, float drag, float speed, float gravityModifier,
                            bool lob, Bot* bot);                       // PE 101B40B0
    virtual bool getRelativePosition(const Vec3& from, Vec3& out);     // PE 101B42A0
    virtual const Mat4& getFrame();                                    // PE 101B3ED0
    virtual bool isSolved();                                           // PE 101B3E90
    virtual bool getAimDirection(Vec3& out);                           // PE 101B42F0
    virtual bool getAimPoint(Vec3& out);                               // PE 101B4330
    virtual bool getImpactTime(double& out);                           // PE 101B3EA0
    virtual bool getProjectilePosition(Vec3& out, double time);        // PE 101B3EE0

private:
    /* +0x164 */ char m_gap164[4];
    /* +0x168 */ BotScriptValue<Vec3>* m_position;
    /* +0x16C */ Bot* m_bot;
    /* +0x170 */ Vec3 m_aimDirection;
    /* +0x17C */ Vec3 m_aimPoint;
    /* +0x188 */ Mat4 m_frame;
    /* +0x1C8 */ double m_impactTime;
    /* +0x1D0 */ bool m_solved;
};

// Raises the bot's sensing-agent override and finishes as failed. Type 21. PE vtable 10282B50,
// ctor 10199750. [INFERRED] name.
class BAPOverrideSensingAgent : public BotScript
{
public:
    BAPOverrideSensingAgent() {}
    virtual ~BAPOverrideSensingAgent();                                // PE 101997E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101997B0
    virtual BAPStatementType getType();                                // PE 10199780: 21
    virtual bool isComposite();                                        // PE 10199770: false
};

// Crouches the bot and, while its soldier crouches, presses fire on one execution and releases it
// on the next, which fails the statement. Always finished. Type 44. PE vtable 10284AA0.
// [INFERRED] names.
class BAPRevive : public BotScript
{
public:
    explicit BAPRevive(int id);                                        // PE 101C1B60
    virtual ~BAPRevive();                                              // PE 101C1BD0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101C1C10
    virtual BAPStatementType getType();                                // PE 101C1BB0: 44
    virtual bool isComposite();                                        // PE 101C1BC0: true

    /* +0x18 */ bool m_pressed;
};

// Places the bot's C4 at its equipment's position, or removes the placed charge. Finishes as
// failed at once. Type 22. PE vtable 10284AE0. [INFERRED] names.
class BAPPlaceC4 : public BotScript
{
public:
    // PE 101C1CD0. The behaviour id is left unset, as shipped.
    BAPPlaceC4(int placementType, AIStrategicObject* object, bool place);
    virtual ~BAPPlaceC4();                                             // PE 101C1D50

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101C1D90
    virtual BAPStatementType getType();                                // PE 101C1D20: 22
    virtual bool isComposite();                                        // PE 101C1D10: false

    /* +0x18 */ int m_placementType;
    /* +0x1C */ AIStrategicObject* m_object;
    /* +0x20 */ bool m_place;
};

// Switches the bot's soldier to a weapon slot by pressing its activate input. Finished as soon as
// it pressed; failed unless the press changes the weapon. A vehicle presses and fails. The input
// mask is the weapon's activate input. Type 28. PE vtable 102842C0. [INFERRED] name.
class BAPChangeWeapon : public BotScript
{
public:
    // The equipment and its armament are not null-checked, as shipped. PE 101B5E00.
    BAPChangeWeapon(Bot* bot, int weapon, int id);
    virtual ~BAPChangeWeapon();                                        // PE 101B5EB0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B5EF0
    virtual BAPStatementType getType();                                // PE 101B5DE0: 28
    virtual bool isComposite();                                        // PE 101B5DF0: true

private:
    /* +0x18 */ Bot* m_bot;
    /* +0x1C */ int m_weapon;
    /* +0x20 */ WeaponInfo* m_weaponInfo;
};

// Idles until its condition holds: finished every tick, failed while the condition is true.
// Owns the condition. Type 13. PE vtable 102842DC. [INFERRED] name.
class BAPIdle : public BotScript
{
public:
    // PE 101B6070.
    BAPIdle(BAPCondition* condition, unsigned long long inputMask, int id);
    virtual ~BAPIdle();                                                // PE 101B6160

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B6100
    virtual BAPStatementType getType();                                // PE 101B60C0: 13
    virtual bool isComposite();                                        // PE 101B6130: true
    virtual void getDebugString(Bot* bot, std::string& out);           // PE 101B6140

private:
    /* +0x18 */ BAPCondition* m_condition;
};

// Holds an input while its condition holds, once the delay has passed; the plan interpreter's
// entry does the pressing. Executing it directly fails at once. Owns its conditions. Type 12.
// PE vtable 10283420. The retail debug print (PE 101A8840) is not reproduced; the base print
// applies. [INFERRED] names.
class BAPTriggerContinously : public BotScript
{
public:
    // PE 101A8720. `press` is required; `hold` and `stop` may be null.
    BAPTriggerContinously(int input, BAPCondition* press, BAPCondition* hold, BAPCondition* stop,
                          unsigned long long inputMask, int id, double delay);
    virtual ~BAPTriggerContinously();                                  // PE 101A89C0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A8820
    virtual BAPStatementType getType();                                // PE 101A87B0: 12
    virtual bool isComposite();                                        // PE 101A87C0: false

    /* +0x18 */ int m_input;
    /* +0x1C */ BAPCondition* m_press;     // presses while true
    /* +0x20 */ BAPCondition* m_hold;      // keeps a press while false
    /* +0x24 */ BAPCondition* m_stop;      // finishes the statement as failed
    /* +0x28 */ bool m_pressed;            // = false
    /* +0x30 */ double m_startTime;        // game time at construction plus the delay
};

// Points plan values at an object: writes the bot's sensed point on it (or, with `lead`, a point
// ahead of it by the projectile's flight time, measured from the camera) to a position value,
// the direction from the equipment to it, and raises a flag value. With no object (-1) it copies
// its fixed position. Finished, failed unless `succeed`; an unknown object fails unfinished.
// Type 19. PE vtable 10284160. [INFERRED] names.
class BAPLookAtWrapper : public BotScript
{
public:
    // PE 101B4E10.
    BAPLookAtWrapper(unsigned int target, BotScriptValue<Vec3>* position, bool succeed, bool lead,
                     BotScriptValue<Vec3>* direction, BotScriptValue<bool>* found);
    virtual ~BAPLookAtWrapper();                                       // PE 101B4E70

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B4F30
    virtual BAPStatementType getType();                                // PE 101B4DE0: 19
    virtual bool isComposite();                                        // PE 101B4DF0: true

    /* +0x18 */ unsigned int m_target;
    /* +0x1C */ BotScriptValue<Vec3>* m_position;
    /* +0x20 */ BotScriptValue<Vec3>* m_direction;   // may be null
    /* +0x24 */ BotScriptValue<bool>* m_found;       // may be null
    /* +0x28 */ Vec3 m_fixedPosition;                // zero; no audited writer
    /* +0x34 */ bool m_succeed;
    /* +0x35 */ bool m_lead;
};

// Accepts every object not in its list; the line checks of the fire statements and conditions
// use it to skip the target. PE vtable 102855B0, ctor 101E1480, predicate 101E14A0.
// [INFERRED] name.
class IgnoreListPredicate : public AIObjectPredicate
{
public:
    explicit IgnoreListPredicate(const std::vector<IAIObject*>& ignore) : m_ignore(&ignore) {}
    virtual bool predicate(IAIObject* object)
    {
        for (unsigned int i = 0; i < m_ignore->size(); ++i)
            if (object == (*m_ignore)[i])
                return false;
        return true;
    }

private:
    const std::vector<IAIObject*>* m_ignore;
};

// Solves the aim at an object for a plan, writing the direction and whether it is valid to plan
// values, and the aimed-at point to a third. It aims at the bot's sensed point, lowered a little
// for soldiers; splash weapons against reported targets aim at the last known position on the
// ground, and qualifying ones switch to the artillery simulation's high arc. Finished, failed
// unless `succeed`; it fails unfinished without equipment, control, armament, weapon or target,
// and does nothing without a sensed record. Type 16. PE vtable 10284388. [INFERRED] names.
class BAPAimingWrapper : public BotScript
{
public:
    // PE 101B6420.
    BAPAimingWrapper(BotScriptValue<Vec3>* direction, BotScriptValue<bool>* valid,
                     BotScriptValue<Vec3>* position, unsigned int equipment, unsigned int target,
                     bool succeed);
    virtual ~BAPAimingWrapper();                                       // PE 101B64B0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101B6630
    virtual BAPStatementType getType();                                // PE 101B63E0: 16
    virtual bool isComposite();                                        // PE 101B63F0: true

private:
    // The ground height under `point` by a two-metre drop line that skips the target, cached for
    // the last point. PE 101B64F0.
    bool groundHeight(Bot* bot, const Vec3& point, float& height);

    /* +0x18 */ AimSolver m_solver;
    /* +0x88 */ BotScriptValue<Vec3>* m_direction;
    /* +0x8C */ BotScriptValue<Vec3>* m_position;       // may be null
    /* +0x90 */ BotScriptValue<bool>* m_valid;
    /* +0x94 */ unsigned int m_equipment;
    /* +0x98 */ unsigned int m_target;
    /* +0x9C */ bool m_succeed;
    /* +0xA0 */ Vec3 m_groundPoint;                     // cache of groundHeight
    /* +0xAC */ float m_groundHeight;
};

// The root of a fire plan: marks the bot as firing at the target while it lives and owns its
// child. Destruction clears the bot's firing, repair, artillery-simulation and flag state and
// tells a target bot it is no longer an engineer's target. Wrapper name index 0. PE vtable
// 102839F8. [INFERRED] names.
class BAPFireWrapper : public BAPWrapper
{
public:
    // PE 101AD320. The child is claimed without a null check, as shipped.
    BAPFireWrapper(Bot* bot, unsigned int target, bool flag2C, bool flag2D, int weapon, int int28,
                   BotScript* child, int id);
    virtual ~BAPFireWrapper();                                         // PE 101AD3B0
    virtual int getWrapperIndex();                                     // PE 101AD3A0: 0

    /* +0x20 */ unsigned int m_target;
    /* +0x24 */ int m_weapon;               // [UNRESOLVED] no audited reader
    /* +0x28 */ int m_int28;                // [UNRESOLVED]
    /* +0x2C */ bool m_flag2C;              // [UNRESOLVED]
    /* +0x2D */ bool m_flag2D;              // [UNRESOLVED]
    /* +0x30 */ int m_int30;                // = -1 [UNRESOLVED]
    /* +0x34 */ Bot* m_bot;
    /* +0x38 */ bool m_useStaticAttack;     // the bot's setting at construction
};

class IPIControlInfo;
class IPIMobile;
class IPIPhysical;

// Steers and throttles a car toward a steering angle; shared by the car move entries and the
// run-over statement. PE 101CD1B0; [INFERRED] name. See the definition for the full contract.
bool driveCar(Information* equipment, IPIControlInfo* control, IPIMobile* mobile, IPIPhysical* physical,
              io::PlayerInput* input, float cruise, float throttle, float steering, float speedWeight,
              float maxSpeedFactor);

// Drives a car at a soldier, aiming where the soldier will be when the car arrives. Finishes
// at once each tick; fails when the target is gone or no vehicle path reaches it. Type 41.
// PE vtable 10284B60. [INFERRED] class name from the statement type.
class BAPRunOverObject : public BotScript
{
public:
    // PE 101C1F50.
    BAPRunOverObject(unsigned int target, unsigned long long inputMask, int id);
    virtual ~BAPRunOverObject();                                       // PE 101C1FB0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101C1FF0
    virtual BAPStatementType getType();                                // PE 101C1FA0: 41
    virtual bool isComposite();                                        // PE 101C1F90: false

    /* +0x18 */ unsigned int m_target;
    // [UNRESOLVED] no reader: the generator passes the vehicle's drive controls.
    /* +0x20 */ unsigned long long m_driveControls;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H
