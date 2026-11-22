// [CHOSEN, not recovered] Grouped sensing-pattern declarations.
// Split into their individual translation units when their plan creation is implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

#ifndef AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H
#define AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H

#include "../Main/AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

class BAPALook;
class Bot;
class BotScript;
class IPIControlInfo;
class SensingAgent;

class ISensingAgentPattern
{
public:
    ISensingAgentPattern() {}
    virtual ~ISensingAgentPattern();

    // Preserve virtual order: reset precedes the other pattern operations.
    virtual void reset(int) = 0;                                        // +0x08
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&) = 0;    // +0x0C
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int) = 0;   // +0x10
    virtual int getId() const = 0;                                      // +0x14

    // Uninitialised by construction - see the block above.
    /* +4 */ int m_id;
};

// The turret aimer. createPlan 821 / .
class SensingAgentTurretPattern : public ISensingAgentPattern
{
public:
    SensingAgentTurretPattern();
    virtual ~SensingAgentTurretPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;

private:
    // Shared opening of both plans; false with `plan` set to the value to return. [INFERRED]
    // helper: both shipped bodies inline it.
    bool initPlan(SensingAgent* agent, Bot* bot, IPIControlInfo*& control, BotScript*& plan);
};

// The mouse-look aimer, and the heaviest of the four: createPlan 2,293 / .
class SensingAgentMouseFreeLookPattern : public ISensingAgentPattern
{
public:
    SensingAgentMouseFreeLookPattern();
    virtual ~SensingAgentMouseFreeLookPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;
};

// The on-foot aimer - the only one of the four with helpers of its own.
class SensingAgentSoldierPattern : public ISensingAgentPattern
{
public:
    SensingAgentSoldierPattern();
    virtual ~SensingAgentSoldierPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;

    // Unique to this pattern and called through its vtable (PE +0x14 / +0x18): they bracket the
    // plan it builds. initPlan answers whether to build; endPlan returns the finished plan.
    virtual bool initPlan(SensingAgent *, Bot *, IPIControlInfo *&, BotScript *&);
    virtual BotScript* endPlan(BAPALook *, Bot *);
};

// The null object. Both createPlan overloads are and do nothing useful.
class SensingAgentNullPattern : public ISensingAgentPattern
{
public:
    SensingAgentNullPattern();
    virtual ~SensingAgentNullPattern();

    // Preserve virtual order: reset occupies the first slot.
    virtual void reset(int);
    virtual BotScript* createPlan(SensingAgent *, Bot *, const Vec3&);
    virtual BotScript* createPlan(SensingAgent *, Bot *, unsigned int);
    virtual int getId() const;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSINGAGENTPATTERNS_H
