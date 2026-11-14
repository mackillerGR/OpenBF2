// [INFERRED] the class comment; the source path is attested by the image's __FILE__ string.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../Main/AIDllFrontier.h"

#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

BBPFireInfantery::BBPFireInfantery(int behaviourId)
{
    m_behaviourId = behaviourId;
    const int bots = configuredBotCount();
    FireInfanteryTarget* const targets = new FireInfanteryTarget[bots];
    for (int i = 0; i < bots; ++i)
    {
        FireInfanteryTarget& t = targets[i];
        t.m_bool0C = false;
        for (int k = 0; k < 4; ++k)
            t.m_bool14[k] = false;
        t.m_int00 = -1;
        t.m_int04 = -1;
        t.m_int08 = -1;
        for (int k = 0; k < 3; ++k)
        {
            t.m_int18[k] = 0;
            t.m_bool24[k] = false;
        }
        for (int k = 0; k < 4; ++k)
            t.m_int28[k] = 0;
    }
    m_targetState = checkPointer(targets);
    m_aim = checkPointer(new FireInfanteryAim[configuredBotCount()]);
}

BBPFireInfantery::~BBPFireInfantery()
{
    delete[] m_targetState;
    delete[] m_aim;
}

int BBPFireInfantery::getFireType()
{
    return 5;
}

// PE 10105300.
void BBPFireInfantery::setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon)
{
    const unsigned int id = bot->getBotId();
    m_hasPlan[id] = true;
    m_targets[id] = static_cast<unsigned int>(-1);
    m_weapons[id] = weapon;
    m_designators[id] = designator;
    m_positions[id] = position;
    m_aim[id].m_int08 = 0;
    m_aim[id].m_int18 = 0;
    m_aim[id].m_double20 = 0.0;
    m_aim[id].m_double28 = 10.0;
    m_targetState[id].m_int00 = -1;
}

FireInfanteryAim::FireInfanteryAim()
    : m_int08(0)
    , m_int18(0)
    , m_double20(0.0)
    , m_double28(10.0)
    , m_int58(-1)
    , m_int5C(-1)
{
    for (int k = 0; k < 9; ++k)
        m_int30[k] = 0;
}

// PE 10103FC0. The flags are not used. A bot without a plan also restarts its aim.
void BBPFireInfantery::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool, bool)
{
    const unsigned int id = bot->getBotId();
    m_targetChanged[id] = target != m_targets[id];
    m_targets[id] = target;
    m_weapons[id] = weapon;
    if (!m_hasPlan[id])
    {
        m_aim[id].m_int08 = 0;
        m_aim[id].m_int18 = 0;
        m_aim[id].m_double20 = 0.0;
        m_aim[id].m_double28 = 10.0;
        m_targetState[id].m_int00 = -1;
    }
    m_hasPlan[id] = true;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-generators]
BotScript* BBPFireInfantery::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPFireInfantery::generate");
}

} // namespace ai
} // namespace hfe
} // namespace dice
