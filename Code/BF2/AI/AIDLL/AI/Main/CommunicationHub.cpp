// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining CommunicationHub operations.

#include "CommunicationHub.h"

#include "AIDllFrontier.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"   // reset: one record per area

namespace dice {
namespace hfe {
namespace ai {

CommunicationHub* CommunicationHub::m_instance = 0;

// Constructs two empty strength vectors and publishes only the first hub instance.
// Call reset before using the three timestamps, which construction leaves uninitialized.
CommunicationHub::CommunicationHub()
{
    // m_time4, m_time12 and m_time44 deliberately not initialised - the binary writes none of
    // them. The two vectors default-construct on their own.
    if (!m_instance)
        m_instance = this;
}

// it does test `== this` before nulling the static, unlike `AerialHeightMap` one step earlier.
// The two vectors are then torn down in reverse, which is the compiler's own work.
CommunicationHub::~CommunicationHub()
{
    if (m_instance == this)
        m_instance = 0;
}

// Stamps the three times with -1000.0 and gives each side one combat-strength record per strategic
// area, type 6 at -1000.0: overwritten in place when side 0 already has records, appended otherwise.
// The overwrite trusts both vectors to be long enough, as in retail. PE 10083DF0.
void CommunicationHub::reset()
{
    m_time4 = -1000.0;
    m_time12 = -1000.0;
    m_time44 = -1000.0;
    const int areas = static_cast<int>(AIStrategicObjectManager::getInstance()->getStrategicAreas().size());
    const std::pair<CombatStrengthType, double> blank(static_cast<CombatStrengthType>(6), -1000.0);
    if (!m_combatStrength[0].empty())
    {
        for (int i = 0; i < areas; ++i)
        {
            m_combatStrength[0][i] = blank;
            m_combatStrength[1][i] = blank;
        }
    }
    else
    {
        for (int i = areas; i > 0; --i)
        {
            m_combatStrength[0].push_back(blank);
            m_combatStrength[1].push_back(blank);
        }
    }
}

// [FRONTIER] The message methods stop until their strategic-object dependencies are implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

void CommunicationHub::enemySpottedByAI(IAIPlayer*, IAIObject*, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommunicationHub::enemySpottedByAI");
}

void CommunicationHub::enemySpottedByHuman(IAIObject*, unsigned char) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommunicationHub::enemySpottedByHuman");
}

void CommunicationHub::filterAndSendAIRadioMessage(BotMain*, int, ChannelMessageType,
                                                   AIRadioChannelType, AIRadioMessageType,
                                                   unsigned char, int, unsigned char,
                                                   unsigned char)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommunicationHub::filterAndSendAIRadioMessage");
}

void CommunicationHub::filterAndReceiveAIRadioMessage(const AIRadioMessageSettings&, int, int, bool)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommunicationHub::filterAndReceiveAIRadioMessage");
}

} // namespace ai
} // namespace hfe
} // namespace dice
