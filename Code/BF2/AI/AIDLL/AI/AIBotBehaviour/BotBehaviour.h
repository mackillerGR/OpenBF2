// Base of the bot behaviours: per-bot urgency bookkeeping and plan hand-over.
// [INFERRED] placement and member names: the image carries no __FILE__ string for this class.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H

#include "UrgencyMerger.h"

namespace dice {
namespace hfe {
namespace ai {

class BotScript;
class Information;
class PlanGenerator;
class UrgeCurve;

// PE vtable 10282A24. One instance serves every bot that uses a vehicle type; the per-bot arrays
// are indexed by bot id and sized by the configured bot count.
class BotBehaviour : public UrgencyGenerator
{
public:
    // PE 101987A0. Derived constructors set the id, the priority, the generator and, where the
    // behaviour tracks one, the target array.
    BotBehaviour(float* weights, UrgeCurve* urgeCurve, int bots);
    // PE 10198880. Deletes the generator; the target array is left to the derived class.
    virtual ~BotBehaviour();

    virtual float getUrgency(Bot* bot);                                  // PE 10198570
    virtual float getActiveUrgency(Bot* bot);                            // PE 10198590
    virtual float updateActiveUrgency(Bot* bot);                         // PE 101985B0
    virtual bool  hasChangedTarget(Bot* bot);                            // PE 101985F0
    // True when the active urgency is zero and the computed one positive, when the computed one
    // is zero, or when computed / active lies below `low` or above `high`. PE 10198610.
    virtual bool  quotientChanged(Bot* bot, float low, float high);
    // True when both the target and the quotient changed. PE 101986E0.
    virtual bool  hasChanged(Bot* bot, float low, float high);
    virtual UrgencyGenerator* getChosenGenerator();                      // PE 100EBC40: this
    // Resets the generator and the bot's urgency, target-changed flag and target. PE 10198720.
    virtual void  reset(Bot* bot);

    // Recomputes and returns the bot's urgency. [INFERRED] name and the meaning of `elapsed`;
    // [UNRESOLVED] the flag's meaning.
    virtual float calculateUrgency(Bot* bot, bool flag, float elapsed) = 0;
    // The plan the bot should run: its current one while it is locked, otherwise a freshly
    // generated one stamped with this behaviour. Without a plan to offer, the bot's current plan
    // is deleted and null returned. PE 101988E0.
    virtual BotScript* getPlan(Bot* bot);
    // Whether the behaviour applies to a bot using this equipment. PE 100EBC50: true.
    virtual bool  isApplicable(Information* equipment, Bot* bot);

    unsigned char getBehaviourId() const { return m_behaviourId; }
    unsigned char getPriority() const { return m_priority; }
    PlanGenerator* getPlanGenerator() const { return m_generator; }

protected:
    /* +0x04 */ unsigned char  m_behaviourId;     // set by the derived constructor
    /* +0x05 */ unsigned char  m_priority;        // set by the derived constructor
    /* +0x08 */ unsigned int*  m_targets;         // = 0; per-bot information handle, -1 when none
    /* +0x0C */ PlanGenerator* m_generator;       // = 0; owned
    /* +0x10 */ float*         m_urgency;         // uninitialised until reset
    /* +0x14 */ float*         m_weights;         // borrowed behaviour-modifier set
    /* +0x18 */ bool*          m_targetChanged;   // = true
    /* +0x1C */ float*         m_activeUrgency;   // = 1e-5; reset stores 1e-4
    /* +0x20 */ UrgeCurve*     m_urgeCurve;       // borrowed
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BOTBEHAVIOUR_H
