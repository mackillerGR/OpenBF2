// Conditions tested by action-plan statements, and the ones the idle plans build.
// [INFERRED] placement and class names: the attested condition sources in AIBotActionPlan use the
// BAPCon prefix (BAPConLineOfFire.cpp); these classes carry no __FILE__ string of their own.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H

#include <iosfwd>
#include <list>

#include "../../../../Math/Vec3.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "AimSolver.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class WeaponInfo;
class AIStrategicObject;
class Information;

// Condition kinds as the printer spells them ("&&", "!", "true", ...). No enumerator name is
// attested, so the values stay opaque.
enum BAPConditionType : int;
std::ostream& operator<<(std::ostream& out, BAPConditionType type);

// PE vtable 1027E650, four slots.
class BAPCondition
{
public:
    BAPCondition() : m_owner(0) {}

    virtual bool evaluate(Bot* bot) = 0;
    virtual BAPConditionType getType() = 0;
    // PE 101A7C60: prints the condition kind.
    virtual void printDebug(std::ostream& out);
    virtual ~BAPCondition();

    // The statement or condition that holds this one. [UNRESOLVED] declared type.
    /* +0x04 */ void* m_owner;
};

// Prints the condition through its printDebug. PE 101A7C80.
std::ostream& operator<<(std::ostream& out, BAPCondition& condition);

// A condition over other conditions. PE vtable 10282E2C; the destructor (PE 1019C090) frees
// the list nodes but not the conditions.
class BAPConditionList : public BAPCondition
{
public:
    BAPConditionList() {}
    virtual ~BAPConditionList();
    // Adds a condition and claims it.
    virtual void addCondition(BAPCondition* condition) = 0;

protected:
    /* +0x08 */ std::list<BAPCondition*> m_conditions;
};

// Kind 40, "true". PE vtable 1027E6A8.
class BAPConTrue : public BAPCondition
{
public:
    virtual bool evaluate(Bot* bot);
    virtual BAPConditionType getType();
    virtual ~BAPConTrue();
};

// Kind 6, "false". PE vtable 1027E660.
class BAPConFalse : public BAPCondition
{
public:
    virtual bool evaluate(Bot* bot);
    virtual BAPConditionType getType();
    virtual ~BAPConFalse();
};

// Kind 14, "!": the negation of its single condition, which it owns. PE vtable 102836C4.
class BAPConNot : public BAPConditionList
{
public:
    explicit BAPConNot(BAPCondition* condition);         // PE 101A9B70
    virtual bool evaluate(Bot* bot);
    virtual BAPConditionType getType();
    virtual void printDebug(std::ostream& out);
    virtual ~BAPConNot();
    // Takes the first condition only. PE 101A9CD0.
    virtual void addCondition(BAPCondition* condition);
};

// Kind 4, "&&": whether every condition holds, testing in order and stopping at the first that
// does not. Owns its conditions. PE vtable 10283928. [INFERRED] name.
class BAPConAnd : public BAPConditionList
{
public:
    BAPConAnd() {}                                       // PE 101ACD20
    virtual bool evaluate(Bot* bot);                     // PE 101ACC30
    virtual BAPConditionType getType();
    virtual void printDebug(std::ostream& out);          // PE 101ACBB0
    virtual ~BAPConAnd();                                // PE 101ACC70
    virtual void addCondition(BAPCondition* condition);  // PE 101ACD60
};

// Kind 35, "||": whether any condition holds, stopping at the first that does. Owns its
// conditions. PE vtable 10282E40. [INFERRED] name.
class BAPConOr : public BAPConditionList
{
public:
    BAPConOr() {}                                        // PE 1019C1E0
    virtual bool evaluate(Bot* bot);                     // PE 1019BF80
    virtual BAPConditionType getType();
    virtual void printDebug(std::ostream& out);          // PE 1019BE70
    virtual ~BAPConOr();                                 // PE 1019C0E0
    virtual void addCondition(BAPCondition* condition);  // PE 1019C220
};

// Kind 52, "Zoomed": whether the active weapon of the bot's equipment is zoomed. PE vtable
// 102836F8.
class BAPConZoomed : public BAPCondition
{
public:
    virtual bool evaluate(Bot* bot);
    virtual BAPConditionType getType();
    virtual ~BAPConZoomed();
};

// Kind 37, "Position": whether the bot's equipment is within the radius of a point, in the
// horizontal plane or in space. True without equipment. PE vtable 10282E78. [INFERRED] name.
class BAPConPosition : public BAPCondition
{
public:
    BAPConPosition(const Vec2& point, float radius);     // PE 1019C470: x and z
    BAPConPosition(const Vec3& point, float radius);     // PE 1019C3D0
    virtual bool evaluate(Bot* bot);                     // PE 1019C4C0
    virtual BAPConditionType getType();
    virtual ~BAPConPosition();                           // PE 1019C420

private:
    // The owned point: a Vec2 (x, z) or, when m_spatial is set, a Vec3.
    /* +0x08 */ union { Vec2* m_flatPoint; Vec3* m_point; };
    /* +0x0C */ float m_radiusSq;
    /* +0x10 */ bool m_spatial;
};

// Kind 28, "ObjectMoved": whether the object moved farther than the distance since the position
// last recorded, which it then records. False when the object is unknown. PE vtable 102831A0.
// [INFERRED] name.
class BAPConObjectMoved : public BAPCondition
{
public:
    BAPConObjectMoved(unsigned int object, float distance);  // PE 101A7200
    virtual bool evaluate(Bot* bot);                     // PE 101A7260
    virtual BAPConditionType getType();
    virtual ~BAPConObjectMoved();

private:
    /* +0x08 */ unsigned int m_object;
    /* +0x0C */ Vec3 m_lastPosition;   // starts at the origin
    /* +0x18 */ float m_distanceSq;
};

// Kind 29, "ObjectOccupied": whether the object's unit is occupied. PE vtable 102837F8.
// [INFERRED] name.
class BAPConObjectOccupied : public BAPCondition
{
public:
    explicit BAPConObjectOccupied(unsigned int object);  // PE 101AC3A0
    virtual bool evaluate(Bot* bot);                     // PE 101AC410
    virtual BAPConditionType getType();
    virtual ~BAPConObjectOccupied();

private:
    /* +0x08 */ unsigned int m_object;
};

// Kind 20, "ObjectEnemyOccupied": whether the object is occupied and not of the bot's side.
// PE vtable 10283828. [INFERRED] name.
class BAPConObjectEnemyOccupied : public BAPCondition
{
public:
    explicit BAPConObjectEnemyOccupied(unsigned int object);  // PE 101AC450
    virtual bool evaluate(Bot* bot);                     // PE 101AC4C0
    virtual BAPConditionType getType();
    virtual ~BAPConObjectEnemyOccupied();

private:
    /* +0x08 */ unsigned int m_object;
};

// Kind 17, "ObjectBehind": whether, in the horizontal plane, the direction from the object to
// the bot's equipment lies at most `threshold` (a cosine) along the object's forward. True when
// the object is unknown. PE vtable 10283898. [INFERRED] name.
class BAPConObjectBehind : public BAPCondition
{
public:
    BAPConObjectBehind(unsigned int object, float threshold, bool estimate);  // PE 101AC620
    virtual bool evaluate(Bot* bot);                     // PE 101AC6A0
    virtual BAPConditionType getType();
    virtual ~BAPConObjectBehind();

private:
    /* +0x08 */ unsigned int m_object;
    /* +0x0C */ float m_threshold;
    // Use a mobile object's position estimate. [INFERRED] meaning.
    /* +0x10 */ bool m_estimate;
};

// Kind 18, "ObjectDistance": whether the distance between the bot's equipment and the object is
// within [min, max] (min starts at 0), or with `outside` beyond max. True when the object is
// unknown. PE vtable 102838C8. [INFERRED] name.
class BAPConObjectDistance : public BAPCondition
{
public:
    // PE 101AC7F0.
    BAPConObjectDistance(unsigned int object, float distance, bool estimate, bool flat, bool outside);
    virtual bool evaluate(Bot* bot);                     // PE 101AC9D0
    virtual BAPConditionType getType();
    virtual void printDebug(std::ostream& out);          // PE 101AC890
    virtual ~BAPConObjectDistance();

private:
    /* +0x08 */ unsigned int m_object;
    /* +0x0C */ float m_minSq;
    /* +0x10 */ float m_maxSq;
    // Use a mobile object's position estimate; measure in the horizontal plane; test beyond the
    // maximum. [INFERRED] meanings.
    /* +0x14 */ bool m_estimate;
    /* +0x15 */ bool m_flat;
    /* +0x16 */ bool m_outside;
    // The squared distance of the last evaluation, printed for debugging.
    /* +0x18 */ float m_lastSq;
};

// Kind 12: whether the squared distance of the bot's current equipment to a fixed position lies within [min, max] (min
// starts at 0), measured in the horizontal plane when `flat`. PE vtable 1028422C. [INFERRED]
// name. The retail debug print (PE 101B59E0) is not reproduced; the base print applies.
class BAPConPositionDistance : public BAPCondition
{
public:
    // PE 101B5AA0.
    BAPConPositionDistance(const Vec3& position, float distance, bool flat);
    virtual bool evaluate(Bot* bot);                     // PE 101B5B70
    virtual BAPConditionType getType();                  // PE 101B59D0: 12
    virtual ~BAPConPositionDistance();

private:
    /* +0x08 */ float m_minSq;
    /* +0x0C */ float m_maxSq;
    /* +0x10 */ bool  m_flat;
    // The squared distance of the last evaluation.
    /* +0x14 */ float m_lastSq;
    /* +0x18 */ Vec3  m_position;
};

// Kind 22: whether a known object's health is at or below a threshold. False for an unknown
// object. PE vtable 10283AE8. [INFERRED] name.
class BAPConObjectHealthBelow : public BAPCondition
{
public:
    BAPConObjectHealthBelow(unsigned int object, float threshold);   // PE 101AD9A0
    virtual bool evaluate(Bot* bot);                     // PE 101ADA50
    virtual BAPConditionType getType();                  // PE 101AD9E0: 22
    virtual void printDebug(std::ostream& out);          // PE 101AD9F0
    virtual ~BAPConObjectHealthBelow();

private:
    /* +0x08 */ unsigned int m_object;
    /* +0x0C */ float m_threshold;
};

// Kind 50: whether the bot may fire. PE vtable 10284130. [INFERRED] name.
class BAPConFireAllowed : public BAPCondition
{
public:
    BAPConFireAllowed();                                 // PE 101B4D60
    virtual bool evaluate(Bot* bot);                     // PE 101B4D80
    virtual BAPConditionType getType();                  // PE 101B4DA0: 50
    virtual ~BAPConFireAllowed();
};

// Kind 51: whether the bot's input holds sprint (bound and non-zero). PE vtable 10284290.
// [INFERRED] name.
class BAPConSprinting : public BAPCondition
{
public:
    BAPConSprinting();                                   // PE 101B5D20
    virtual bool evaluate(Bot* bot);                     // PE 101B5D80
    virtual BAPConditionType getType();                  // PE 101B5D40: 51
    virtual ~BAPConSprinting();
};

// Kind 52: whether the active weapon of the bot's current equipment is zoomed. False without
// weapon info; the equipment information is not null-checked, as shipped. PE vtable 102836F8.
// [INFERRED] name.
class BAPConWeaponZoomed : public BAPCondition
{
public:
    BAPConWeaponZoomed();                                // PE 101A9D20
    virtual bool evaluate(Bot* bot);                     // PE 101A9D80
    virtual BAPConditionType getType();                  // PE 101A9D40: 52
    virtual ~BAPConWeaponZoomed();
};

// Kind 46: heat hysteresis on the weapon held in a plan value. True while the heat stays below
// `high`; once it reaches `high` it answers false until the heat falls to `low` or below. PE
// vtable 10283E6C. [INFERRED] name.
class BAPConWeaponHeat : public BAPCondition
{
public:
    // PE 101B2EF0.
    BAPConWeaponHeat(BotScriptValue<WeaponInfo*>* weapon, float high, float low);
    virtual bool evaluate(Bot* bot);                     // PE 101B2F70
    virtual BAPConditionType getType();                  // PE 101B2F30: 46
    virtual ~BAPConWeaponHeat();

private:
    /* +0x08 */ BotScriptValue<WeaponInfo*>* m_weapon;
    /* +0x0C */ float m_high;
    /* +0x10 */ float m_low;
    /* +0x14 */ bool  m_overheated;
};

// Kind 44: whether the active weapon of the bot's current equipment is the weapon in a plan
// value and is ready to fire. The equipment information is not null-checked, as shipped. PE
// vtable 10284328. [INFERRED] name.
class BAPConWeaponReady : public BAPCondition
{
public:
    explicit BAPConWeaponReady(BotScriptValue<WeaponInfo*>* weapon);  // PE 101B61B0
    virtual bool evaluate(Bot* bot);                     // PE 101B6220
    virtual BAPConditionType getType();                  // PE 101B61E0: 44
    virtual ~BAPConWeaponReady();

private:
    /* +0x08 */ BotScriptValue<WeaponInfo*>* m_weapon;
};

// Kind 53: whether the active weapon is the weapon in a plan value and its state is 1. PE vtable
// 10284358. [INFERRED] name; the meaning of state 1 is [UNRESOLVED].
class BAPConWeaponStateOne : public BAPCondition
{
public:
    explicit BAPConWeaponStateOne(BotScriptValue<WeaponInfo*>* weapon);  // PE 101B62B0
    virtual bool evaluate(Bot* bot);                     // PE 101B6320
    virtual BAPConditionType getType();                  // PE 101B62E0: 53
    virtual ~BAPConWeaponStateOne();

private:
    /* +0x08 */ BotScriptValue<WeaponInfo*>* m_weapon;
};

// Kind 16: whether a strategic object's status for a side is non-zero, or zero with `invert`.
// PE vtable 10284260. [INFERRED] name.
class BAPConStrategicStatus : public BAPCondition
{
public:
    // PE 101B5C70.
    BAPConStrategicStatus(AIStrategicObject* object, int side, bool invert);
    virtual bool evaluate(Bot* bot);                     // PE 101B5CA0
    virtual BAPConditionType getType();                  // PE 101B5CE0: 16
    virtual ~BAPConStrategicStatus();

private:
    /* +0x08 */ AIStrategicObject* m_object;
    /* +0x0C */ int  m_side;
    /* +0x10 */ bool m_invert;
};

// Kind 26: whether the bot may fire at a sensed target: the target is sensed without the +8
// sense flag; unless the active weapon fires through transparent surfaces the target must also
// carry one of the 0x48 sense flags; and for a bot whose equipment has type flag bit 21, no
// friend may stand in the line of fire. PE vtable 102841E0. [INFERRED] names. The retail debug
// print (PE 101B5520) is not reproduced; the base print applies.
class BAPConCanFireAt : public BAPCondition
{
public:
    // The second flag is stored but no audited body reads either flag. PE 101B54A0.
    BAPConCanFireAt(unsigned int target, bool flag);
    virtual bool evaluate(Bot* bot);                     // PE 101B5870
    virtual BAPConditionType getType();                  // PE 101B5510: 26
    virtual ~BAPConCanFireAt();

private:
    // Whether a friend of the bot stands in the cone from the active weapon's muzzle to the
    // target; records the time on the bot (0 when none). PE 101B5550.
    bool friendInLineOfFire(Bot* bot, Information* own, Information* target);

    /* +0x08 */ unsigned int m_target;
    /* +0x0C */ bool m_flagC;                             // 1 [UNRESOLVED]
    /* +0x0D */ bool m_flagD;                             // the second argument [UNRESOLVED]
};

// Kind 32: whether the bot's camera already points along the aim solution for a target. A plan
// value pair can supply the direction instead of the solver. With `trackFailures`, a miss records
// the static-attack failure times on the bot and, outside the artillery simulation, clears its
// artillery flags. PE vtable 102843D0. [INFERRED] names.
class BAPConAimOnTarget : public BAPCondition
{
public:
    // `equipment` -1 means the bot's current equipment. PE 101B6D70.
    BAPConAimOnTarget(unsigned int target, unsigned int equipment, BotScriptValue<bool>* valid,
                      BotScriptValue<Vec3>* direction, bool trackFailures);
    virtual bool evaluate(Bot* bot);                     // PE 101B70E0
    virtual BAPConditionType getType();                  // PE 101B6DC0: 32
    virtual ~BAPConAimOnTarget();                        // PE 101B6DD0

private:
    // The aim direction: the plan values when both are set, otherwise the ballistic solution for
    // the active weapon. PE 101B6E00.
    bool aimDirection(Bot* bot, Vec3& direction);

    /* +0x08 */ unsigned int m_target;
    /* +0x0C */ unsigned int m_equipment;
    /* +0x10 */ AimSolver m_solver;
    /* +0x80 */ BotScriptValue<bool>* m_valid;
    /* +0x84 */ BotScriptValue<Vec3>* m_direction;
    /* +0x88 */ bool m_trackFailures;
};

// Kind 41: a magazine test on the weapon in a plan value against an int plan value. With
// `changed`: without magazines, whether the int value is non-zero; otherwise whether the first
// magazine's shot count differs from it. Without `changed`: true without magazines, otherwise
// whether the first magazine holds at most that many shots. A negative count answers false. The
// weapon's magazine queries repeat as shipped. PE vtable 10284030. [INFERRED] names.
class BAPConMagazine : public BAPCondition
{
public:
    // PE 101B46F0.
    BAPConMagazine(BotScriptValue<WeaponInfo*>* weapon, BotScriptValue<int>* shots, bool changed);
    virtual bool evaluate(Bot* bot);                     // PE 101B4780
    virtual BAPConditionType getType();                  // PE 101B4730: 41
    virtual ~BAPConMagazine();

private:
    /* +0x08 */ BotScriptValue<WeaponInfo*>* m_weapon;
    /* +0x0C */ bool m_changed;
    /* +0x10 */ BotScriptValue<int>* m_shots;
};

// Kind 38: a value shared between copies. The original evaluates its inner condition and stores
// the result; a copy made with share() has no inner condition and answers the stored value. The
// last holder frees the shared value. PE vtable 10283E58. [INFERRED] names. The retail debug
// print (PE 101B2DD0) is not reproduced; the base print applies.
class BAPConShared : public BAPCondition
{
public:
    // The inner condition is not null-checked, as shipped. PE 101B2CC0.
    explicit BAPConShared(BAPCondition* inner);
    virtual bool evaluate(Bot* bot);                     // PE 101B2D80
    virtual BAPConditionType getType();                  // PE 101B2DC0: 38
    virtual ~BAPConShared();                             // PE 101B2E30

    // A copy reading the same value. PE 101B2E80.
    BAPConShared* share();

private:
    struct Value
    {
        bool m_value;
        int  m_holders;
    };
    explicit BAPConShared(Value* value);

    /* +0x08 */ Value* m_value;
    /* +0x0C */ BAPCondition* m_inner;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
