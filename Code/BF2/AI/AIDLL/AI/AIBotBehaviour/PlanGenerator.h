// Builds the action plan a behaviour hands to a bot.
// [INFERRED] placement and member names: the image carries no __FILE__ string for this class.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_PLANGENERATOR_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_PLANGENERATOR_H

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class BotScript;

// PE vtable 10282FB4, over an interface vtable 10282FA0 of the same five slots. Keeps one flag
// per bot, indexed by bot id and sized by the configured bot count.
class PlanGenerator
{
public:
    // PE 101A5CE0. Every flag starts clear.
    PlanGenerator();
    // PE 101A5C90.
    virtual ~PlanGenerator();

    // The plan for the bot, or null. Ownership passes to the caller.
    virtual BotScript* generate(Bot* bot) = 0;
    // The bot's flag: whether the generator has a plan to offer. PE 100EE9D0.
    virtual bool hasPlan(Bot* bot);
    // Clears the bot's flag. PE 100EE9B0.
    virtual void reset(Bot* bot);
    // Sets the bot's flag. PE 100EE9F0.
    virtual void setHasPlan(Bot* bot);

protected:
    /* +0x04 */ bool* m_hasPlan;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_PLANGENERATOR_H
