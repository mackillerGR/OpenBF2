#include "BBTakeCoverInfantry.h"

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIBotBehaviourPattern/PlanGeneratorSupport.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-bot-behaviours] The threat
// accessors and the urgency body below stop by name until recovered and exercised by a bot that
// takes cover.

bool CoverThreatObject::getPosition(Vec3&) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CoverThreatObject::getPosition");
}

int CoverThreatObject::getWord() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CoverThreatObject::getWord");
}

bool CoverThreatObject::matches(const CoverThreat&) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CoverThreatObject::matches");
}

bool CoverThreatHandle::getPosition(Vec3&) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CoverThreatHandle::getPosition");
}

int CoverThreatHandle::getWord() const
{
    return static_cast<int>(m_handle);
}

bool CoverThreatHandle::matches(const CoverThreat&) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CoverThreatHandle::matches");
}

// The cover array is allocated twice and the first block is lost, as shipped.
BBTakeCoverInfantry::BBTakeCoverInfantry(int behaviourId, int priority, PlanGenerator* generator,
                                         UrgeCurve* urgeCurve, float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_botCount = configuredBotCount();
    m_cover = checkPointer(new void*[configuredBotCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[m_botCount]);
    m_cover = checkPointer(new void*[m_botCount]);
    m_objectThreats = checkPointer(new CoverThreatObject[m_botCount]);
    m_handleThreats = checkPointer(new CoverThreatHandle[m_botCount]);
    for (int i = 0; i < m_botCount; ++i)
    {
        m_targets[i] = static_cast<unsigned int>(-1);
        m_urgency[i] = 0.0f;
        m_cover[i] = 0;
    }
}

// The per-bot cover entries are released as plain blocks.
BBTakeCoverInfantry::~BBTakeCoverInfantry()
{
    for (int i = 0; i < m_botCount; ++i)
        ::operator delete(m_cover[i]);
    delete[] m_targets;
    delete[] m_cover;
    delete[] m_objectThreats;
    delete[] m_handleThreats;
}

float BBTakeCoverInfantry::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTakeCoverInfantry::calculateUrgency");
}

} // namespace ai
} // namespace hfe
} // namespace dice
