// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining CommunicationHub operations.

#include "CommunicationHub.h"

#include "AIDllFrontier.h"
#include "CommonSingletonServer.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"                 // IAIPlayer::getPlayer
#include "../../../AIInterface/BFKnowledge/BFGame.h"                   // IAIGame::enemySpottedByAI
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
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

// PE 100837E0. Passes a bot's first sighting of a primary enemy to the game, at most once a minute
// per strategic area and side: an armour (4) or air (5) contact counts within 100 m of an area and
// may also go out through the shared once-a-minute slot; anything else counts within 30 m. Inside
// the minute an area's record only upgrades from strength 0 or 1 to 2. One report per call.
// The information is not null-checked before isPrimary, as shipped.
void CommunicationHub::enemySpottedByAI(IAIPlayer* player, IAIObject* object, int side)
{
    Information* const information =
        informationManager->getInformation(object->getInformation(side));
    if (!information->isPrimary())
        return;
    IPIPhysical* const physical = information->getPhysical();
    if (!physical)
        return;
    const int strength = physical->getStrengthType();
    const int sideIndex = side - 1;
    const std::map<std::string, AIStrategicArea*>& areas =
        AIStrategicObjectManager::m_instance->getStrategicAreas();
    float radiusSq;
    if (strength == 5 || strength == 4)
        radiusSq = 10000.0f;
    else
        radiusSq = 900.0f;
    const double now =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    int index = 0;
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = areas.begin();
         it != areas.end(); ++it, ++index)
    {
        const Vec3& position = object->getPosition();
        Vec2 position2d;
        position2d.x = position.x;
        position2d.y = position.z;
        // [INFERRED] the PE calls a non-virtual accessor of the side-1 position (PE 100DEB80);
        // AIStrategicArea does not override getPosition, so this reads the same member.
        const Vec2& areaPosition = it->second->getPosition(1);
        const float dx = position2d.x - areaPosition.x;
        const float dy = position2d.y - areaPosition.y;
        if (!(dy * dy + dx * dx < radiusSq))
            continue;
        if (strength == 5 || strength == 4)
        {
            if (now - m_time44 > 60.0)
            {
                m_time44 = now;
                static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
                    ->enemySpottedByAI(player->getPlayer(), object->getObject());
                return;
            }
        }
        std::pair<CombatStrengthType, double>& record = m_combatStrength[sideIndex][index];
        if (now - record.second > 60.0)
        {
            record.first = static_cast<CombatStrengthType>(strength);
        }
        else if ((record.first == 0 || record.first == 1) && strength == 2)
        {
            record.first = static_cast<CombatStrengthType>(2);
        }
        else
        {
            continue;
        }
        record.second = now;
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
            ->enemySpottedByAI(player->getPlayer(), object->getObject());
        return;
    }
}

// PE 10083330. A human spotted the object for the side: refresh that side's knowledge of it,
// creating the record (on the grid) when the side has none. The created record is not
// null-checked, as shipped.
void CommunicationHub::enemySpottedByHuman(IAIObject* object, unsigned char side) const
{
    Information* information = informationManager->getInformation(object->getInformation(side));
    if (!information)
        information = informationManager->getInformation(object->forceGetInformation(side, true));
    information->setTime(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
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
