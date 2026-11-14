#include "BotBehaviour.h"

#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBotActionPlan/BotScript.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

BotBehaviour::BotBehaviour(float* weights, UrgeCurve* urgeCurve, int bots)
{
    m_targets = 0;
    m_generator = 0;
    m_urgency = checkPointer(new float[bots]);
    m_weights = weights;
    m_targetChanged = checkPointer(new bool[bots]);
    m_activeUrgency = checkPointer(new float[bots]);
    m_urgeCurve = urgeCurve;
    for (int i = 0; i < bots; ++i)
    {
        m_activeUrgency[i] = 9.99999975e-06f;
        m_targetChanged[i] = true;
    }
}

BotBehaviour::~BotBehaviour()
{
    delete[] m_urgency;
    delete[] m_targetChanged;
    delete[] m_activeUrgency;
    if (m_generator)
        delete m_generator;
}

float BotBehaviour::getUrgency(Bot* bot)
{
    return m_urgency[bot->getBotId()];
}

float BotBehaviour::getActiveUrgency(Bot* bot)
{
    return m_activeUrgency[bot->getBotId()];
}

float BotBehaviour::updateActiveUrgency(Bot* bot)
{
    const float urgency = m_urgency[bot->getBotId()];
    m_activeUrgency[bot->getBotId()] = urgency;
    return urgency;
}

bool BotBehaviour::hasChangedTarget(Bot* bot)
{
    return m_targetChanged[bot->getBotId()];
}

bool BotBehaviour::quotientChanged(Bot* bot, float low, float high)
{
    if (m_activeUrgency[bot->getBotId()] == 0.0f)
        return m_urgency[bot->getBotId()] > 0.0f;
    if (m_urgency[bot->getBotId()] == 0.0f)
        return true;
    const float quotient = m_urgency[bot->getBotId()] / m_activeUrgency[bot->getBotId()];
    if (quotient < low)
        return true;
    return quotient > high;
}

bool BotBehaviour::hasChanged(Bot* bot, float low, float high)
{
    return hasChangedTarget(bot) && quotientChanged(bot, low, high);
}

UrgencyGenerator* BotBehaviour::getChosenGenerator()
{
    return this;
}

void BotBehaviour::reset(Bot* bot)
{
    if (m_generator)
        m_generator->reset(bot);
    m_urgency[bot->getBotId()] = 0.0f;
    m_activeUrgency[bot->getBotId()] = 9.99999975e-05f;
    m_targetChanged[bot->getBotId()] = true;
    if (m_targets)
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-execution] Replace with the
// PE 101988E0 body once BotScript's owner field and destructor slot are declared.
// PE 101988E0. A locked plan stands. Otherwise a generator with a plan for the bot makes a new
// one; without, the bot's current plan is destroyed.
BotScript* BotBehaviour::getPlan(Bot* bot)
{
    if (bot->getPlanLock())
        return bot->getPlan();
    if (m_generator->hasPlan(bot))
    {
        if (bot->getPlan())
            bot->getPlan()->m_owner = m_generator;
        BotScript* const script = m_generator->generate(bot);
        if (script)
        {
            script->m_owner = this;
            if (bot->getPlan() && bot->getPlan() != script)
            {
                bot->getPlan()->m_owner = this;
                bot->getSensingAgent()->setPlanReplaced();
            }
        }
        return script;
    }
    if (bot->getPlan())
    {
        if (bot->getPlan())
            delete bot->getPlan();
        bot->setPlan(0);
    }
    return 0;
}

bool BotBehaviour::isApplicable(Information*, Bot*)
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
