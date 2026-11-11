// Combines per-behavior urgency generators for a bot.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_URGENCYMERGER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_URGENCYMERGER_H

namespace dice {
namespace hfe {
namespace ai {

class Bot;

// Source of a behaviour urgency for one bot. The PE vtable 10282A00 holds the destructor and
// eight pure virtuals; the class owns nothing.
class UrgencyGenerator
{
public:
    virtual ~UrgencyGenerator();

    // The urgency last computed for the bot.
    virtual float getUrgency(Bot* bot) = 0;
    // The urgency in force when the bot last committed to this generator.
    virtual float getActiveUrgency(Bot* bot) = 0;
    // Commits the computed urgency as the active one and returns it.
    virtual float updateActiveUrgency(Bot* bot) = 0;
    virtual bool  hasChangedTarget(Bot* bot) = 0;
    // True when computed / active urgency left the range [low, high].
    virtual bool  quotientChanged(Bot* bot, float low, float high) = 0;
    virtual bool  hasChanged(Bot* bot, float low, float high) = 0;
    virtual UrgencyGenerator* getChosenGenerator() = 0;
    virtual void  reset(Bot* bot) = 0;
};

// Picks the generator with the highest urgency among a bot's behaviours. PE vtable 10285218.
// The generator array may hold null entries; the merger owns neither array.
class UrgencyMerger : public UrgencyGenerator
{
public:
    // BotManager's pool passes null weights; no recovered method reads them.
    UrgencyMerger(int count, float* weights, UrgencyGenerator** generators);
    virtual ~UrgencyMerger();

    // Both forward to the chosen generator and yield zero without one.
    virtual float getUrgency(Bot* bot);
    virtual float getActiveUrgency(Bot* bot);
    // Commits every generator, chooses the one with the highest positive urgency and returns
    // that urgency; zero and no choice when none is positive.
    virtual float updateActiveUrgency(Bot* bot);
    // True when every present generator reports it.
    virtual bool  hasChangedTarget(Bot* bot);
    virtual bool  quotientChanged(Bot* bot, float low, float high);
    // True when any present generator's quotient or target changed.
    virtual bool  hasChanged(Bot* bot, float low, float high);
    virtual UrgencyGenerator* getChosenGenerator();
    virtual void  reset(Bot* bot);
    // The merger's own virtual, after the interface's eight.
    virtual int   getSize();

private:
    /*  4 */ int                m_count;
    /*  8 */ float*             m_weights;
    /* 12 */ UrgencyGenerator** m_generators;
    /* 16 */ UrgencyGenerator*  m_chosen;        // = 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_URGENCYMERGER_H
