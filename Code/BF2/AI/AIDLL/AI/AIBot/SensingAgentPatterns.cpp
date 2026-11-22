// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SensingAgentPatterns operations.

#include "SensingAgentPatterns.h"

#include "../Main/AIDllFrontier.h"
#include "SensingAgent.h"
#include "BotMain.h"
#include "../AIBotActionPlan/BAPConditions.h"
#include "../AIBotActionPlan/BAPStatements.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// The base owns the shared id field used by every pattern's getId/reset pair.
// Constructors and destructors perform no additional member initialization or cleanup.
ISensingAgentPattern::~ISensingAgentPattern()
{
}

SensingAgentTurretPattern::SensingAgentTurretPattern()
{
}

SensingAgentTurretPattern::~SensingAgentTurretPattern()
{
}

int SensingAgentTurretPattern::getId() const
{
    return m_id;
}

void SensingAgentTurretPattern::reset(int id)
{
    m_id = id;
}

SensingAgentMouseFreeLookPattern::SensingAgentMouseFreeLookPattern()
{
}

SensingAgentMouseFreeLookPattern::~SensingAgentMouseFreeLookPattern()
{
}

int SensingAgentMouseFreeLookPattern::getId() const
{
    return m_id;
}

void SensingAgentMouseFreeLookPattern::reset(int id)
{
    m_id = id;
}

SensingAgentSoldierPattern::SensingAgentSoldierPattern()
{
}

SensingAgentSoldierPattern::~SensingAgentSoldierPattern()
{
}

int SensingAgentSoldierPattern::getId() const
{
    return m_id;
}

void SensingAgentSoldierPattern::reset(int id)
{
    m_id = id;
}

SensingAgentNullPattern::SensingAgentNullPattern()
{
}

SensingAgentNullPattern::~SensingAgentNullPattern()
{
}

int SensingAgentNullPattern::getId() const
{
    return m_id;
}

void SensingAgentNullPattern::reset(int id)
{
    m_id = id;
}

// [FRONTIER] Pattern plan creation and soldier helpers require the look/aim plan runtime.
// [RETIRE-WHEN: aidll-02-skeleton] Replace each stop when its plan behavior is recovered.
namespace {

// Five degrees, from the turret pattern's own degree constant (PE 102AAA80) times 5 (PE 1027B20C).
const float kTurretDegree = 0.0174532924f;

} // anonymous namespace

// Keeps the agent's plan when it is already the pattern's; otherwise drops it, clears the script
// variables and fetches the equipment's control info. False with `plan` set to what to return.
bool SensingAgentTurretPattern::initPlan(SensingAgent* agent, Bot* bot, IPIControlInfo*& control, BotScript*& plan)
{
    ISensingAgentPattern* const pattern = this;
    plan = agent->m_plan;
    if (plan)
    {
        if (plan->getId() == pattern->getId())
            return false;
        delete plan;
        agent->m_plan = 0;
    }
    agent->m_bapContext->clear();
    plan = 0;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return false;
    control = equipment->getControlInfo();
    return control != 0;
}

// PE 1012F1A0. Looks along the direction with a five-degree dead zone until told otherwise.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (!initPlan(agent, bot, control, plan))
        return plan;
    BAPALookDirection* const look = checkPointer(new BAPALookDirection(
        direction, kTurretDegree * 5.0f, checkPointer(new BAPConFalse), control->getLookControls(), getId()));
    look->m_owner = this;
    return look;
}

// PE 1012F2C0. As above, looking at an object.
BotScript* SensingAgentTurretPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int object)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (!initPlan(agent, bot, control, plan))
        return plan;
    BAPALookAtObject* const look = checkPointer(new BAPALookAtObject(
        bot, object, kTurretDegree * 5.0f, checkPointer(new BAPConFalse), control->getLookControls(), getId(), 0,
        0.0f));
    look->m_owner = this;
    return look;
}

BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent *, Bot *, const Vec3&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SensingAgentMouseFreeLookPattern::createPlan");
}

BotScript* SensingAgentMouseFreeLookPattern::createPlan(SensingAgent *, Bot *, unsigned int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SensingAgentMouseFreeLookPattern::createPlan");
}

// PE 1012EB40. Looks along the direction until told otherwise, then merges the soldier's inputs.
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, const Vec3& direction)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (!initPlan(agent, bot, control, plan))
        return plan;
    BAPConFalse* const until = new BAPConFalse;
    const int id = getId();
    BAPALook* const look = new BAPALookDirection(direction, 0.0f, checkPointer(until),
                                                 control->getLookControls(), id);
    return endPlan(checkPointer(look), bot);
}

// PE 1012EBF0. As above, looking at an object.
BotScript* SensingAgentSoldierPattern::createPlan(SensingAgent* agent, Bot* bot, unsigned int object)
{
    IPIControlInfo* control = 0;
    BotScript* plan = 0;
    if (!initPlan(agent, bot, control, plan))
        return plan;
    BAPConFalse* const until = new BAPConFalse;
    const int id = getId();
    BAPALook* const look = new BAPALookAtObject(bot, object, 0.0f, checkPointer(until),
                                                control->getLookControls(), id, 0, 0);
    return endPlan(checkPointer(look), bot);
}

// PE 1012EA10. Keeps the agent's plan when it is already this pattern's; otherwise drops it,
// clears the script variables and fetches the equipment's control info.
bool SensingAgentSoldierPattern::initPlan(SensingAgent* agent, Bot* bot, IPIControlInfo*& control,
                                          BotScript*& plan)
{
    if (plan)
    {
        if (agent->m_plan->getId() == getId())
            return false;
        if (plan)
            delete plan;
        agent->m_plan = 0;
    }
    agent->m_bapContext->clear();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment)
    {
        control = equipment->getControlInfo();
        if (control)
            return true;
    }
    plan = 0;
    return false;
}

// PE 1012EAB0. The look runs beside the input merger.
BotScript* SensingAgentSoldierPattern::endPlan(BAPALook* look, Bot*)
{
    BAPParallel* const plan = checkPointer(new BAPParallel(getId(), -1, false));
    BAPMergeSoldierInputs* const merge = checkPointer(new BAPMergeSoldierInputs(getId()));
    plan->addStatement(look);
    plan->addStatement(merge);
    plan->m_owner = this;
    return plan;
}

// Null-pattern planning still requires an implementation; these overloads stop visibly.
// PE 1012E860 / 1012E900, identical: keep this pattern's plan, or release the look.
static BotScript* createNullPlan(SensingAgentNullPattern* pattern, SensingAgent* agent,
                                 BotScript*& agentPlan, BAPContext* context)
{
    BotScript* const plan = agentPlan;
    if (plan)
    {
        if (plan->getId() == pattern->getId())
            return plan;
        delete plan;
        agentPlan = 0;
    }
    context->clear();
    BAPResetControls* const reset = checkPointer(new BAPResetControls(false, true, false, pattern->getId()));
    reset->m_owner = pattern;
    return reset;
}

BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot*, const Vec3&)
{
    return createNullPlan(this, agent, agent->m_plan, agent->m_bapContext);
}

BotScript* SensingAgentNullPattern::createPlan(SensingAgent* agent, Bot*, unsigned int)
{
    return createNullPlan(this, agent, agent->m_plan, agent->m_bapContext);
}

} // namespace ai
} // namespace hfe
} // namespace dice
