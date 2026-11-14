// [INFERRED] Source placement.

#include "CommunicationAgent.h"

#include "../Main/AIDllFrontier.h"
#include "BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/CommunicationHub.h"
#include "../../../AIInterface/Module/BFConsole.h"   // IBFModuleCourier, AIRadioMessageSettings

namespace dice {
namespace hfe {
namespace ai {

// Caches voice timing settings for the eighteen radio message types.
CommunicationAgent::CommunicationAgent(BotMain* owner)
    : m_bot(owner)
{
    // eighteen. See the header block.
    m_receivedMessages.resize(18);
    m_receiveTimeToLive.resize(18);
    m_maxSendPeriod.resize(18);

    IBFModuleCourier* const courier =
        static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier);

    for (unsigned int type = 0; type <= 17; ++type)
    {
        int voice;
        if (courier->getVoiceFromAiMessageType(static_cast<AIRadioMessageType>(type), voice))
        {
            AIRadioMessageSettings settings;
            courier->getAIMessageSettings(voice, settings);
            // Crossed, and in this order - see the block above.
            m_receiveTimeToLive[type] = settings.m_receiveTimeToLive;
            m_maxSendPeriod[type]     = settings.m_maxSendPeriod;
        }
        else
        {
            m_receiveTimeToLive[type] = 0.0f;
            m_maxSendPeriod[type]     = 0.0f;
        }
    }

    reset();
}

// Resets message state and acquires the shared communication hub.
void CommunicationAgent::reset()
{
    // The shared hub, re-read on every reset. See the block above.
    m_hub = CommunicationHub::m_instance;

    m_nextSendTime.resize(0);
    m_nextSendTime.resize(18);

    for (unsigned int type = 0; type <= 17; ++type)
        m_receivedMessages[type].clear();

    m_queuedTypes.clear();

    // No clear on a priority_queue - see the block above.
    while (!m_queue.empty())
        m_queue.pop();

    m_lastUpdateTime = 0.0;
    m_int92 = -1;
    // This order is the binary's.
    m_double104 = 0.0;
    m_double96 = 0.0;
}

// PE 1014AAD0. Called for one bot per BotManager communication tick.
void CommunicationAgent::update()
{
    const double time = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    expireReceivedMessages(time);
    // A clock that ran backwards restarts the gate.
    if (time < m_lastUpdateTime)
        m_lastUpdateTime = time;
    if (time - m_lastUpdateTime > 1.0)
    {
        m_lastUpdateTime = time;
        updateOutgoing(time);
    }
}

// PE 101495C0. Clears lapsed send periods and drops the oldest inbox entries whose
// receive time-to-live has run out.
void CommunicationAgent::expireReceivedMessages(double time)
{
    for (unsigned int type = 0; type < 18; ++type)
    {
        if (time > m_nextSendTime[type])
            m_nextSendTime[type] = 0.0;
        std::list<AIRadioMessageInfo>& inbox = m_receivedMessages[type];
        std::list<AIRadioMessageInfo>::iterator it = inbox.begin();
        while (it != inbox.end() && m_receiveTimeToLive[type] + it->m_time > time)
            ++it;
        inbox.erase(it, inbox.end());
    }
}

// PE 1014A5F0. Raises the support request for the current equipment, then sends every queued
// message that is due - unless someone was heard saying the same thing within its send period.
void CommunicationAgent::updateOutgoing(double time)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    requestSupport(equipment);
    releaseFire(equipment);

    while (!m_queue.empty())
    {
        const CAPrioMessage message = m_queue.top();
        if (time < message.m_sendTime)
            return;
        const std::list<AIRadioMessageInfo>& inbox = m_receivedMessages[message.m_type];
        if (!inbox.empty())
        {
            const AIRadioMessageInfo& latest = inbox.front();
            if (m_maxSendPeriod[latest.m_type] > time - latest.m_time)
                return;
        }
        sendRadioMessage(message);
        if (!m_queue.empty())
            popQueuedMessage();
    }
}

// Inline in the shipped pop: the type leaves the index, then the heap.
void CommunicationAgent::popQueuedMessage()
{
    m_queuedTypes.erase(m_queue.top().m_type);
    m_queue.pop();
}

// PE 1014A230.
void CommunicationAgent::enqueueRadioMessage(int type, int channel, unsigned int priority,
                                             float delay, int flag)
{
    const double time = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (time < m_nextSendTime[type])
        return;

    CAPrioMessage message;
    message.m_type = type;
    message.m_channel = channel;
    message.m_priority = priority;
    message.m_flag = flag;
    message.m_sendTime = time + delay;
    message.m_delay = delay;

    if (m_queuedTypes.insert(std::make_pair(type, time + delay)).second)
    {
        m_queue.push(message);
        m_nextSendTime[type] = m_maxSendPeriod[type] + time;
    }
}

// PE 1014A2E0. A hurt bot on foot asks for a medic, a damaged vehicle asks for repair - unless
// it is moving fast or airborne, or carries a type 1 or 3 weapon that works on its own damage.
void CommunicationAgent::requestSupport(Information* equipment)
{
    const unsigned int primaryHandle = equipment->getPrimaryObject(m_bot->getSide());
    Information* const primary = informationManager->getInformation(primaryHandle);
    IPIMobile* const mobile = primary->getMobile();

    bool busy;
    if (m_bot->isAirBorn())
        busy = true;
    else if (((primary->getObjectTypeFlags() >> 4) & 1) && mobile && mobile->getSpeedMagnitude() > 3.0f)
        busy = true;
    else
        busy = false;

    if (!equipment->isPrimary() || busy)
        return;

    const float health = equipment->getPhysical()->getHealth();
    const int strengthType = equipment->getPhysical()->getStrengthType();
    const float threshold = strengthType ? 0.75f : 0.5f;
    if (!(health > 0.0f) || !(health < threshold))
        return;

    if (IPIArmament* const armament = equipment->getArmament())
    {
        const std::vector<unsigned int>& weapons = armament->getWeapons();
        for (size_t i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (!weapon)
                continue;
            const int weaponType = weapon->getWeaponType();
            if (weaponType != 1 && weaponType != 3)
                continue;
            if (static_cast<float>(weapon->getStrength(strengthType)) > 0.0f)
                return;
        }
    }

    if (!strengthType)
        enqueueRadioMessage(3, 0, 5, 0.0f, -1);
    else
        enqueueRadioMessage(4, 6, 5, 0.0f, 0);
}

// PE 10147840. Two seconds after m_double104, equipment with type flag 26 lets go of fire.
void CommunicationAgent::releaseFire(Information* equipment)
{
    if (!((equipment->getObjectTypeFlags() >> 26) & 1))
        return;
    const double time = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (time - m_double104 > 2.0)
        m_bot->m_playerInput->setInput(io::PIFire, 0.0f);
}

// PE 10146270. The player id is read twice, as shipped.
void CommunicationAgent::sendRadioMessage(const CAPrioMessage& message)
{
    const unsigned char playerId = static_cast<unsigned char>(m_bot->getPlayerId());
    const unsigned char sender = static_cast<unsigned char>(m_bot->getPlayerId());
    const unsigned char side = static_cast<unsigned char>(m_bot->getSide());
    const unsigned char flag = static_cast<unsigned char>(message.m_flag);
    int voice;
    if (static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)
            ->getVoiceFromAiMessageType(static_cast<AIRadioMessageType>(message.m_type), voice))
    {
        m_hub->filterAndSendAIRadioMessage(m_bot, playerId, static_cast<ChannelMessageType>(3),
                                           static_cast<AIRadioChannelType>(message.m_channel),
                                           static_cast<AIRadioMessageType>(message.m_type),
                                           static_cast<unsigned char>(voice), sender, side, flag);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
