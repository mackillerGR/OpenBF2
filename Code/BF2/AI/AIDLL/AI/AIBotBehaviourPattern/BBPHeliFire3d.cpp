// [INFERRED] the class comment; the source path is attested by the image's __FILE__ string.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../Main/AIDllFrontier.h"

#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

BBPHeliFire3d::BBPHeliFire3d(int behaviourId)
{
    m_behaviourId = behaviourId;
}

BBPHeliFire3d::~BBPHeliFire3d()
{
}

int BBPHeliFire3d::getFireType()
{
    return 4;
}

// PE 10116440.
// Retail reports the request as unsupported, then stores it like the 3d generator.
void BBPHeliFire3d::setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPHeliFire3d.cpp", 125, "")
        << "LaserGuided missiles unsuported by helicopters.";
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_designators[bot->getBotId()] = designator;
    m_positions[bot->getBotId()] = position;
}

// PE 10116120.
void BBPHeliFire3d::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA,
                                    bool flagB)
{
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_weapons[bot->getBotId()] = weapon;
    m_flagB = flagB;
    m_flagA = flagA;
    m_designators[bot->getBotId()] = -1;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-generators]
BotScript* BBPHeliFire3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPHeliFire3d::generate");
}

} // namespace ai
} // namespace hfe
} // namespace dice
