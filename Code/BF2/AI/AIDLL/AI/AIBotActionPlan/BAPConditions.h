// Conditions tested by action-plan statements, and the ones the idle plans build.
// [INFERRED] placement and class names: the attested condition sources in AIBotActionPlan use the
// BAPCon prefix (BAPConLineOfFire.cpp); these classes carry no __FILE__ string of their own.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H

#include <iosfwd>
#include <list>

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

// A condition over other conditions. PE vtable 10282E2C; the destructor (PE 1019C090) frees
// the list nodes but not the conditions.
class BAPConditionList : public BAPCondition
{
public:
    BAPConditionList() {}
    virtual ~BAPConditionList();

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

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONDITIONS_H
