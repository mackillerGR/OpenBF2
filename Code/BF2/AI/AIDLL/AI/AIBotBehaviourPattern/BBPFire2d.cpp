// [INFERRED] the class comment; the source path is attested by the image's __FILE__ string.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../Main/AIDllFrontier.h"

#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

BBPFire2d::BBPFire2d(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_bool28 = checkPointer(new bool[configuredBotCount()]);
}

BBPFire2d::~BBPFire2d()
{
    delete[] m_bool28;
}

int BBPFire2d::getFireType()
{
    return 2;
}

// PE 1010DB80.
// Retail reports the request as unsupported, then stores it like the 3d generator.
void BBPFire2d::setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire2d.cpp", 141, "")
        << "LaserGuided missiles unsuported by artillry.";
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_designators[bot->getBotId()] = designator;
    m_positions[bot->getBotId()] = position;
}

// PE 1010DB50.
void BBPFire2d::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA, bool flagB)
{
    setObjectTarget(bot, target, weapon, flagA, flagB, false);
}

void BBPFire2d::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA, bool flagB,
                         bool flagC)
{
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_bool28[bot->getBotId()] = flagC;
    m_weapons[bot->getBotId()] = weapon;
    m_flagB = flagB;
    m_flagA = flagA;
    m_designators[bot->getBotId()] = -1;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-generators]
BotScript* BBPFire2d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPFire2d::generate");
}

} // namespace ai
} // namespace hfe
} // namespace dice
