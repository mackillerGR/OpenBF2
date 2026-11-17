// Conditions tested by action-plan statements, and the ones the idle plans build.
// [INFERRED] placement and class names: the attested condition sources in AIBotActionPlan use the
// BAPCon prefix (BAPConLineOfFire.cpp); these classes carry no __FILE__ string of their own.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H

#include <iosfwd>
#include <list>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;

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

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
