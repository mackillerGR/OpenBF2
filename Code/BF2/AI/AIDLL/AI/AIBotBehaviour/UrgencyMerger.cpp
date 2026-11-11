// [INFERRED] Source placement.

#include "UrgencyMerger.h"


namespace dice {
namespace hfe {
namespace ai {

// one instruction in the binary - the vptr store C++ emits on its own. The class owns nothing
// which is what makes it a pure interface rather than a partial implementation.
UrgencyGenerator::~UrgencyGenerator()
{
}

// Four stores after the vptr. `m_weights` is whatever the caller passed and BotManager passes
// null, so a merger from the bot pool has no weight array at all - whatever `getUrgency` does with
// that is the next thing to read, and it is not guessed at here.
UrgencyMerger::UrgencyMerger(int count, float* weights, UrgencyGenerator** generators)
    : m_count(count)
    , m_weights(weights)
    , m_generators(generators)
    , m_chosen(0)
{
}

// PE 101DAF50. Neither array is released.
UrgencyMerger::~UrgencyMerger()
{
}

// PE 101DAD10.
float UrgencyMerger::getUrgency(Bot* bot)
{
    if (m_chosen)
        return m_chosen->getUrgency(bot);
    return 0.0f;
}

// PE 101DAD40.
float UrgencyMerger::getActiveUrgency(Bot* bot)
{
    if (m_chosen)
        return m_chosen->getActiveUrgency(bot);
    return 0.0f;
}

// PE 101DAD60. A missing generator counts as zero; ties keep the earlier generator.
float UrgencyMerger::updateActiveUrgency(Bot* bot)
{
    float best = 0.0f;
    m_chosen = 0;
    for (int i = 0; i < m_count; ++i)
    {
        const float urgency = m_generators[i] ? m_generators[i]->updateActiveUrgency(bot) : 0.0f;
        if (urgency > best)
        {
            best = urgency;
            m_chosen = m_generators[i]->getChosenGenerator();
        }
    }
    return best;
}

// PE 101DADD0.
bool UrgencyMerger::hasChangedTarget(Bot* bot)
{
    bool result = true;
    for (int i = 0; i < m_count && result; ++i)
    {
        if (m_generators[i])
            result = m_generators[i]->hasChangedTarget(bot);
    }
    return result;
}

// PE 101DAE20.
bool UrgencyMerger::quotientChanged(Bot* bot, float low, float high)
{
    bool result = true;
    for (int i = 0; i < m_count && result; ++i)
    {
        if (m_generators[i])
            result = m_generators[i]->quotientChanged(bot, low, high);
    }
    return result;
}

// PE 101DAE70.
bool UrgencyMerger::hasChanged(Bot* bot, float low, float high)
{
    bool result = false;
    for (int i = 0; i < m_count && !result; ++i)
    {
        if (m_generators[i])
        {
            result = m_generators[i]->quotientChanged(bot, low, high)
                     || m_generators[i]->hasChangedTarget(bot);
        }
    }
    return result;
}

// PE 101DAF40.
UrgencyGenerator* UrgencyMerger::getChosenGenerator()
{
    return m_chosen;
}

// PE 101DAED0.
void UrgencyMerger::reset(Bot* bot)
{
    for (int i = 0; i < m_count; ++i)
    {
        if (m_generators[i])
            m_generators[i]->reset(bot);
    }
}

// PE 101DAD30.
int UrgencyMerger::getSize()
{
    return m_count;
}

} // namespace ai
} // namespace hfe
} // namespace dice
