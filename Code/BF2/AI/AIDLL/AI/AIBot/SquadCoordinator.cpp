// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SquadCoordinator operations.

#include "SquadCoordinator.h"

#include "SquadFormation.h"
#include "SquadMember.h"
#include "BotMain.h"
#include "CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AIMain.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"      // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/BFSquad.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../Main/AIRandom.h"
#include "../Main/AISettingsInterfaces.h"
#include "SquadManager.h"
#include "SensingAgent.h"
#include "BotSenses.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// [INFERRED] local helper name. PE's Vec3 subtraction helper spills all three
// deltas to float, then its length-squared helper keeps (dx*dx+dy*dy)+dz*dz wide
// through the ordered comparison. ELF keeps the subtractions wide and adds
// (dy*dy+dx*dx)+dz*dz. Only the MSVC x86 branch is runtime-validated here.
static bool maneuverMemberWithinRange(const Vec3& position, const Vec3& squadPosition)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const member = &position;
    const Vec3* const squad = &squadPosition;
    const float threshold = 8100.0f;
    float deltaX, deltaY, deltaZ;
    bool within = false;
    __asm {
        mov eax, squad
        mov edx, member
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 5
        jp notWithin
        mov within, 1
    notWithin:
    }
    return within;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(squadPosition.x) - position.x);
    const long double dy = static_cast<float>(static_cast<long double>(squadPosition.y) - position.y);
    const long double dz = static_cast<float>(static_cast<long double>(squadPosition.z) - position.z);
#else
    const long double dx = static_cast<long double>(squadPosition.x) - position.x;
    const long double dy = static_cast<long double>(squadPosition.y) - position.y;
    const long double dz = static_cast<long double>(squadPosition.z) - position.z;
#endif
#if defined(_WIN32)
    return (dx * dx + dy * dy) + dz * dz < 8100.0L;
#else
    return (dy * dy + dx * dx) + dz * dz < 8100.0L;
#endif
#endif
}

// [INFERRED] local helper name. A shooter's squared distance from where it first fired, ordered
// strictly above 25 (five metres). PE spills only dx to float and sums (dz*dz+dy*dy)+dx*dx;
// ELF keeps all three differences wide and sums (dy*dy+dx*dx)+dz*dz. Equality/NaN keep the record.
static bool shooterMovedAway(const Vec3& firePosition, const Vec3& position)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const fire = &firePosition;
    const Vec3* const member = &position;
    const float threshold = 25.0f;
    float deltaX;
    bool moved = false;
    __asm {
        mov eax, fire
        mov edx, member
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fmul st(0), st(0)
        fxch st(1)
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaX
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp threshold
        fnstsw ax
        test ah, 41h
        jnz notMoved
        mov moved, 1
    notMoved:
    }
    return moved;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(firePosition.x) - position.x);
#else
    const long double dx = static_cast<long double>(firePosition.x) - position.x;
#endif
    const long double dy = static_cast<long double>(firePosition.y) - position.y;
    const long double dz = static_cast<long double>(firePosition.z) - position.z;
#if defined(_WIN32)
    return (dz * dz + dy * dy) + dx * dx > 25.0L;
#else
    return (dy * dy + dx * dx) + dz * dz > 25.0L;
#endif
#endif
}

// [INFERRED] local helper name. The leader's squared distance to the goto position, narrowed to
// float. PE subtracts through its float-returning Vec3 operator (position - goto); ELF keeps
// goto - position wide. Both sum (dx*dx+dy*dy)+dz*dz.
static float leaderGotoDistanceSq(const Vec3& position, const Vec3& gotoPosition)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const p = &position;
    const Vec3* const g = &gotoPosition;
    float deltaX, deltaY, deltaZ, distance;
    __asm {
        mov eax, p
        mov edx, g
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fstp distance
    }
    return distance;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(position.x) - gotoPosition.x);
    const long double dy = static_cast<float>(static_cast<long double>(position.y) - gotoPosition.y);
    const long double dz = static_cast<float>(static_cast<long double>(position.z) - gotoPosition.z);
#else
    const long double dx = static_cast<long double>(gotoPosition.x) - position.x;
    const long double dy = static_cast<long double>(gotoPosition.y) - position.y;
    const long double dz = static_cast<long double>(gotoPosition.z) - position.z;
#endif
    return static_cast<float>((dx * dx + dy * dy) + dz * dz);
#endif
}

// [INFERRED] local helper name. Squared member distance ordered strictly below the (float)
// squared radius. PE: control point - position through its float Vec3 operator, then
// (dx*dx+dy*dy)+dz*dz; ELF: position - control point wide, then (dy*dy+dx*dx)+dz*dz.
static bool insideControlPointRadius(const Vec3& controlPoint, const Vec3& position, float radiusSq)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    const Vec3* const c = &controlPoint;
    const Vec3* const p = &position;
    float deltaX, deltaY, deltaZ;
    bool inside = false;
    __asm {
        mov eax, c
        mov edx, p
        fld dword ptr [eax]
        fsub dword ptr [edx]
        fstp deltaX
        fld dword ptr [eax+4]
        fsub dword ptr [edx+4]
        fstp deltaY
        fld dword ptr [eax+8]
        fsub dword ptr [edx+8]
        fstp deltaZ
        fld deltaX
        fmul st(0), st(0)
        fld deltaY
        fmul st(0), st(0)
        faddp st(1), st(0)
        fld deltaZ
        fmul st(0), st(0)
        faddp st(1), st(0)
        fcomp radiusSq
        fnstsw ax
        test ah, 5
        jp outside
        mov inside, 1
    outside:
    }
    return inside;
#else
#if defined(_WIN32)
    const long double dx = static_cast<float>(static_cast<long double>(controlPoint.x) - position.x);
    const long double dy = static_cast<float>(static_cast<long double>(controlPoint.y) - position.y);
    const long double dz = static_cast<float>(static_cast<long double>(controlPoint.z) - position.z);
    return (dx * dx + dy * dy) + dz * dz < static_cast<long double>(radiusSq);
#else
    const long double dx = static_cast<long double>(position.x) - controlPoint.x;
    const long double dy = static_cast<long double>(position.y) - controlPoint.y;
    const long double dz = static_cast<long double>(position.z) - controlPoint.z;
    return (dy * dy + dx * dx) + dz * dz < static_cast<long double>(radiusSq);
#endif
#endif
}

// [INFERRED]
// void return: the manager discards it and exits do not materialize a common result.
// Timestamp and mission-2 assignment checks precede even the empty-member return.
void SquadCoordinator::update(double time)
{
    m_d124 = time;
    if (m_i120 == 2 && !IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
    {
        if (m_i40 == 0)
        {
            if (m_strategicAreaAssignment &&
                m_strategicAreaAssignment->getStatus(m_side + 1) != 0)
            {
                if (!m_strategicAreaAssignment->isAreaTakeable())
                {
                    // Both the main singleton and side are fresh after the area callback;
                    // the second side read is later still, after getSAI has returned.
                    SAI* const sai = IAIMain::m_instance->getSAI(m_side + 1);
                    if (!sai->ownAllTakeableCps(m_side + 1))
                        unAssignSquad();
                }
            }
            else if (m_strategicAreaAssignment &&
                     m_strategicAreaAssignment->getStatus(m_side + 1) == 0)
            {
                unAssignSquad();
            }
        }
        else if (m_i40 == 1 && m_strategicAreaAssignment &&
                 m_strategicAreaAssignment->getStatus(m_side + 1) == 0)
        {
            // Opposite one-based side is captured before getSAI; it is not reloaded
            // after that callback, unlike the ownAllTakeableCps argument above.
            const int otherSide = (m_side == 0) + 1;
            if (IAIMain::m_instance->getSAI(m_side + 1)->haveAnyOwnedAndTakeableCps(otherSide))
                unAssignSquad();
        }
    }

    if (m_squadMembers.empty())
        return;

    // Both images reload the member vector after each call, without a null-member guard.
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        m_squadMembers[index]->updateFormationStatus();
    m_formation->updateClosestSlots();

    // The second live loop forwards the original double, not the mutable stored time.
    // It is skipped if the preceding callbacks emptied the vector; maneuver still follows.
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        m_squadMembers[index]->update(time);

    updateSquadManeuver();

    m_formation->update(time);
    updateSquadPosition();
    updateShotsFired();
    // Each timed tail receives the original double argument, not the stored m_d124.
    updateRepairRequest(time);
    updateNeedOrdersMessage(time);
    updateIncomingMessages();
}

// [INFERRED] Updates the squad position from its living anchor, otherwise its first
// living member. Retains the old position when nobody is alive. Anchor/leader indices
// must be valid; lookups are unchecked and position-validity results are ignored.
void SquadCoordinator::updateSquadPosition()
{
    Vec3 position = {};
    const int anchorIndex = m_formation->getSquadMemberIndexAtSlot(0);
    SquadMember* member = anchorIndex == -1 ? getSquadLeader() : m_squadMembers[anchorIndex];
    if (!member->getPlayer()->getIsAlive())
    {
        member = 0;
        for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        {
            if (m_squadMembers[index]->getPlayer()->getIsAlive())
            {
                member = m_squadMembers[index];
                break;
            }
        }
    }
    if (member)
        member->getPosition(position);
    else
        position = m_v108;
    m_v108 = position;
}

// [INFERRED] Removes fire records with invalid shooter positions. Expired records or
// shooters more than five metres away also clear member paths and the movement flag.
// Reuses the position local, deletes records before nodes, and re-reads the list end.
void SquadCoordinator::updateShotsFired()
{
    Vec3 position = {};
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); )
    {
        SquadMemberFire* const fire = *it;
        if (!fire->m_member->getPosition(position))
        {
            delete *it;
            m_list12.erase(it++);
        }
        else if (m_d124 > fire->m_time || shooterMovedAway(fire->m_position, position))
        {
            delete *it;
            m_list12.erase(it++);
            // Not updateSquadPaths: this inline walk has no null-member test.
            for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
                m_squadMembers[index]->setHavePath(false);
            m_formation->updateMovementFlag(true);
        }
        else
            ++it;
    }
}

// Strict ordered expiry clears the request and
// its confirmation together; the position and deadline are left as they were.
void SquadCoordinator::updateRepairRequest(double time)
{
    if (m_b172 && time > m_d188)
    {
        m_b172 = false;
        m_b196 = false;
    }
}

// Requests orders for an idle bot squad leader after twenty seconds without orders.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding SquadCoordinator operation.
void SquadCoordinator::updateNeedOrdersMessage(double time)
{
    if (IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && !isSquadTakingFlag())
    {
        const int orderState = m_i40;
        if (orderState == 0 || orderState == 2 || orderState == 4)
        {
            SquadMember* const leader = getSquadLeader();
            float distanceSq = 1000.0f;
            Vec3 position = {};
            if (leader)
            {
                if (leader->getPosition(position))
                    distanceSq = leaderGotoDistanceSq(position, m_v28);
                if (leader->isBot() && !leader->isMoving() &&
                    leader->getPlayer()->getIsAlive() && !leader->getPlayer()->isManDown() &&
                    (distanceSq < 225.0f ||
                     (m_strategicAreaAssignment &&
                      m_strategicAreaAssignment->getStatus(m_side + 1) == 0)))
                {
                    // The one path that keeps the running timer.
                    if (!(time > m_needOrderMessageTimer + 20.0))
                        return;
                    static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                        ->requestNewCommanderOrder(m_squadIndex + 1, leader->getPlayer());
                    CommunicationAgent* const radio = leader->getBot()->getCommunicationAgent();
                    (void)radio;
                    BF2_AI_FRONTIER("dice::hfe::ai::SquadCoordinator::updateNeedOrdersMessage (CommunicationAgent::enqueueRadioMessage)");
                }
            }
        }
    }
    resetNeedOrderMessageTimer(time);
}

// Routes a human leader's call to one bot when the formation has left the leader.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding SquadCoordinator operation.
void SquadCoordinator::updateIncomingMessages()
{
    SquadMember* const leader = getSquadLeader();
    if (!leader || leader->isBot())
        return;
    if (!m_formation->shouldLeaveSquadLeader())
        return;

    bool found = false;
    SquadMember* member = 0;
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        member = m_squadMembers[index];
        if (member->isBot() && member->getPlayer()->getIsAlive())
        {
            found = true;
            break;
        }
    }
    if (!found)
        return;

    CommunicationAgent* const radio = member->getBot()->getCommunicationAgent();
    const std::list<AIRadioMessageInfo>& messages =
        radio->getReceivedMessages(static_cast<AIRadioMessageType>(12));
    if (messages.empty())
        return;
    if (messages.front().m_senderPlayerId != leader->getPlayerId())
        return;

    m_formation->joinSquadLeader();
    BF2_AI_FRONTIER("dice::hfe::ai::SquadCoordinator::updateIncomingMessages (CommunicationAgent::enqueueRadioMessage)");
}

// Returns whether a living, standing squad member is inside the assigned control point
// for an area this side does not own. Requires a valid member position and a successful
// control-point query; squares the radius in float.
bool SquadCoordinator::isSquadTakingFlag()
{
    bool taking = false;
    if (m_strategicAreaAssignment && m_strategicAreaAssignment->getStatus(m_side + 1) != 0)
    {
        Vec3 controlPoint = {};
        Vec3 position = {};
        float radius;
        if (m_strategicAreaAssignment->getControlPointPosAndRadius(controlPoint, radius))
        {
            radius = radius * radius;
            for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
            {
                SquadMember* const member = m_squadMembers[index];
                if (member->getPlayer()->getIsAlive() && !member->getPlayer()->isManDown() &&
                    member->getPosition(position) &&
                    insideControlPointRadius(controlPoint, position, radius))
                {
                    taking = true;
                    break;
                }
            }
        }
    }
    return taking;
}

// [INFERRED] void result: discarded by update, no common return materialization.
void SquadCoordinator::updateSquadManeuver()
{
    m_i148 = chooseSquadManeuver();
    switch (m_i148)
    {
    case 0:
        for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
        {
            SquadMember* const member = m_squadMembers[index];
            if (member->isBot())
                member->getBot()->setAllowFire(true);
        }
        break;
    case 1:
        squadManeuverAlternateFire();
        break;
    case 2:
        squadManeuverStaticFlank();
        break;
    }
}

namespace {

// PE 1006EEC0, the conditional Vec3 normaliser (file-local copies live in other TUs too).
void normaliseDirection(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (std::fabs(lengthSquared) < 1.1920928955078125e-07f)
    {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
}

Information* lookupInformation(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

} // namespace

// PE 101A4DF0. Every four seconds at most: every bot may fire, then the firing bot foot soldiers
// within 90 of the squad position are collected. With two or more, a four-step cycle holds half
// of them back at a time - the first half for 1.5 s, a random 1-3 s pause, the second half for
// 1.5 s, another pause. A held member stops firing only while it faces its target (cosine above
// 0.85) from more than 40 away. A stale target handle skips the member; the facing lookup is
// not null-checked.
void SquadCoordinator::squadManeuverAlternateFire()
{
    if (!(m_d124 > m_d140))
        return;
    m_d140 = m_d124 + 4.0;
    std::vector<SquadMember*> firing;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member->isBot())
            member->getBot()->setAllowFire(true);
    }
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* member = m_squadMembers[index];
        if (!member->isBot() || !member->getPlayer()->getIsAlive())
            continue;
        Bot* const bot = member->getBot();
        Information* const information = lookupInformation(bot->getCurrentEquipment());
        if (!bot->isFiring() || !information->isPrimary() || !information->getMobile())
            continue;
        const unsigned int flags = information->getObjectTypeFlags();
        if ((flags & 0x10u) || (flags & 0x20u) || (flags & 0x80u))
            continue;
        if (!member->getPosition(position))
            continue;
        const float dx = m_v108.x - position.x;
        const float dy = m_v108.y - position.y;
        const float dz = m_v108.z - position.z;
        if (!(dx * dx + dy * dy + dz * dz < 8100.0f))
            continue;
        firing.push_back(member);
    }
    if (firing.size() <= 1)
        return;

    Vec3 direction;
    direction.x = 0.0f;
    direction.y = 0.0f;
    direction.z = 0.0f;
    unsigned int first = 0;
    unsigned int end = 0;
    const unsigned int count = static_cast<unsigned int>(firing.size());
    switch (m_i152)
    {
    case 0:
        end = count >> 1;
        m_d140 = m_d124 + 1.5;
        m_i152 = 1;
        break;
    case 1:
    {
        const double draw = static_cast<double>(AIRandom::m_instance->next());
        ++m_i152;
        m_d140 = draw * 2.3283064365386963e-10 * 2.0 + m_d124 + 1.0;
        break;
    }
    case 2:
        first = count >> 1;
        end = count;
        m_d140 = m_d124 + 1.5;
        m_i152 = 3;
        break;
    case 3:
    {
        const double draw = static_cast<double>(AIRandom::m_instance->next());
        m_i152 = 0;
        m_d140 = draw * 2.3283064365386963e-10 * 2.0 + m_d124 + 1.0;
        break;
    }
    }

    for (unsigned int index = first; index < end; ++index)
    {
        SquadMember* const member = firing[index];
        Information* const target = lookupInformation(member->getBot()->getFiringTarget());
        if (!target || !member->getPosition(position))
            continue;
        const Vec3& targetPosition = target->getPosition();
        direction.x = targetPosition.x - position.x;
        direction.y = targetPosition.y - position.y;
        direction.z = targetPosition.z - position.z;
        normaliseDirection(direction);
        const Vec3 facing = lookupInformation(member->getBot()->getCurrentEquipment())->getFaceing();
        if (!(direction.x * facing.x + direction.z * facing.z + direction.y * facing.y > 0.85000002f))
            continue;
        const Vec3& again = target->getPosition();
        const float tx = again.x - position.x;
        const double ty = static_cast<double>(again.y) - position.y;
        const double tz = static_cast<double>(again.z) - position.z;
        if (!(static_cast<double>(tx) * tx + ty * ty + tz * tz > 1600.0f))
            continue;
        firing[index]->getBot()->setAllowFire(false);
    }
}

// PE 101A4310. Every bot may fire. Once per flank, on a mission with the lead slot filled and an
// assigned area, a 65% draw sends the lead slot's member along the area's nearest flank pair
// when both points are valid for it.
void SquadCoordinator::squadManeuverStaticFlank()
{
    for (unsigned int index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member->isBot())
            member->getBot()->setAllowFire(true);
    }
    if (m_b157 || m_i120 != 2 || !m_formation->isLeadSlotOccupied() || !m_strategicAreaAssignment)
        return;
    m_b157 = true;
    if (!(static_cast<double>(AIRandom::m_instance->next()) * 2.3283064365386963e-10 < 0.64999998f))
        return;
    SquadMember* const member = m_squadMembers[m_formation->getSquadMemberIndexAtSlot(0)];
    if (!m_strategicAreaAssignment->getClosestFlankPositions(m_v108, m_positions[0], m_positions[1]))
        return;
    if (!isPositionValidForMember(member, m_positions[0]))
        return;
    if (!isPositionValidForMember(member, m_positions[1]))
        return;
    m_b156 = true;
    member->setHavePath(false);
}

// Return carries the full integer selector; the source enum name remains unknown.
int SquadCoordinator::chooseSquadManeuver()
{
    int firingMembers = 0;
    Vec3 position = {};
    const unsigned int count = static_cast<unsigned int>(m_squadMembers.size());
    for (unsigned int index = 0; index < count; ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member->isBot() && member->getPlayer()->getIsAlive())
        {
            Bot* const bot = member->getBot();
            // [PLATFORM] ELF captures the manager before the equipment callback.
            // PE loads it afterward, and only when the low handle word is nonzero.
#if defined(_WIN32)
            const unsigned int handle = bot->getCurrentEquipment();
            Information* const information = (handle & 0xffffu)
                ? informationManager->getInformation(handle) : nullptr;
#else
            InformationManager* const manager = informationManager;
            const unsigned int handle = bot->getCurrentEquipment();
            Information* const information = manager->getInformation(handle);
#endif
            if (bot->isFiring())
            {
                // No information-null guard exists in either image.
                const unsigned int flags = information->getObjectTypeFlags();
                if (!(flags & 0x10u) && !(flags & 0x20u) && !(flags & 0x80u) &&
                    member->getPosition(position) &&
                    maneuverMemberWithinRange(position, m_v108))
                    ++firingMembers;
            }
        }
    }
    if (firingMembers > 1)
        return 1;
    if (m_i120 != 2)
        return 0;

    // PE 101A29E1. A static flank is chosen while the squad's position is more than 50 and
    // under sqrt(30000) from the goal (taken at terrain height), the mission flag is set and the side holds
    // the assigned area outright (status 1). Entering it clears both flank flags.
    const float targetX = m_v28.x;
    const float targetZ = m_v28.z;
    const float height =
        static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(targetX, targetZ);
    const double dx = static_cast<double>(targetX) - m_v108.x;
    const double dy = static_cast<double>(height) - m_v108.y;
    const double dz = static_cast<double>(targetZ) - m_v108.z;
    const double distanceSq = dz * dz + dy * dy + dx * dx;
    if (!(distanceSq < 30000.0f))
        return 0;
    if (!(distanceSq > 2500.0f))
        return 0;
    if (!m_b56 || !m_strategicAreaAssignment || m_strategicAreaAssignment->getStatus(m_side + 1) != 1)
        return 0;
    if (m_i148 != 2)
    {
        m_b156 = false;
        m_b157 = false;
    }
    return 2;
}

// [INFERRED] void return, as above.
// Null entries are skipped; a live unsigned index walk invalidates each real member.
void SquadCoordinator::updateSquadPaths()
{
    for (std::size_t index = 0; index < m_squadMembers.size(); ++index)
    {
        SquadMember* const member = m_squadMembers[index];
        if (member)
            member->setHavePath(false);
    }
}

// Borrowed formation pointer, unchanged.
SquadFormation* SquadCoordinator::getSquadFormation()
{
    return m_formation;
}

// Return the address of the stored
// destination. [INFERRED] const-reference spelling; callers read the borrowed Vec3.
const Vec3& SquadCoordinator::getSquadGotoPos()
{
    return m_v28;
}

// Full-width field read; the original
// return enum spelling is unresolved, so int records the observed payload without new labels.
int SquadCoordinator::getMissionStatus()
{
    return m_i120;
}

// The assignment is borrowed and nullable.
bool SquadCoordinator::isSquadAssignedToArea(int areaIndex)
{
    return m_strategicAreaAssignment && m_strategicAreaAssignment->getIndex() == areaIndex;
}

// Each qualifying
// live member needs an IAIObject information-handle query before any strength can be added.
int SquadCoordinator::getStrategicStrength(StrategicStrengthType type)
{
    float total = 0.0f;
    for (std::size_t index = 0; index < m_squadMembers.size(); ++index)
    {
        IAIPlayer* const player = m_squadMembers[index]->getPlayer();
        if (!player->getIsAlive() || player->isManDown())
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;

        // PE 101A3680: the object's information handle for this side resolves to a unit whose
        // strategic strength of the requested type is added.
        Information* const information =
            informationManager->getInformation(vehicle->getInformation(m_side + 1));
        if (!information || !information->getUnit())
            continue;
        total = total + information->getUnit()->getStrategicStrengths()[type];
    }
    return static_cast<int>(total + 0.5f);
}

// Stores side and squad identity before constructing the formation, which queries this coordinator.

SquadCoordinator::SquadCoordinator(int side, int squadIndex)
    : m_squadMembers()
    , m_list12()
    , m_squadIndex(squadIndex)
    , m_side(side)
    , m_strategicAreaAssignment(0)
    , m_i40(4)
    , m_i44(0)
    , m_d48(-20.0)
    , m_pendingArea(0)
    , m_i76(4)
    , m_i80(0)
    , m_i96(-1)
    , m_list104()
    , m_i120(0)
    , m_d124(0.0)
    , m_d132(0.0)
    , m_d140(0.0)
    , m_i148(0)
    , m_i152(0)
    , m_b156(false)
    , m_b157(false)
    , m_positions()
    , m_b172(false)
    , m_b196(false)
    , m_b220(false)
{
    m_v28.x = 0.0f;
    m_v28.y = 0.0f;
    m_v28.z = 0.0f;
    m_v64.x = 0.0f;
    m_v64.y = 0.0f;
    m_v64.z = 0.0f;
    m_v108.x = 0.0f;
    m_v108.y = 0.0f;
    m_v108.z = 0.0f;
    m_v176.x = 0.0f;
    m_v176.y = 0.0f;
    m_v176.z = 0.0f;
    m_v200.x = 0.0f;
    m_v200.y = 0.0f;
    m_v200.z = 0.0f;

    m_formation = new SquadFormation(this);

    m_v108.x = 0.0f;
    m_v108.y = 0.0f;
    m_v108.z = 0.0f;

    // two of them, pushed one after the other. See the header.
    Vec3 zero;
    zero.x = 0.0f;
    zero.y = 0.0f;
    zero.z = 0.0f;
    m_positions.push_back(zero);
    m_positions.push_back(zero);

    m_v200.x = 0.0f;
    m_v200.y = 0.0f;
    m_v200.z = 0.0f;

    resetNeedOrderMessageTimer(
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());

    m_b220 = false;
}

int SquadCoordinator::getSide()
{
    return m_side;
}

// Copy the stored squad position
// (retail ELF +0x6C / PE +0x8C), without testing squadHasPosition. [INFERRED]
// void return: both callers discard it; EAX holds different scratch values per port.
void SquadCoordinator::getSquadPosition(Vec3& position)
{
    position = m_v108;
}

// The stored leader index is
// used directly, including the absence of an empty-vector or negative-index guard.
SquadMember* SquadCoordinator::getSquadLeader()
{
    return m_squadMembers[m_i96];
}

// Signed count consumed by the manager's comparisons.
int SquadCoordinator::getNumberOfSquadMembers()
{
    return static_cast<int>(m_squadMembers.size());
}

// non-const in the mangled signature.
int SquadCoordinator::getSquadId()
{
    return m_squadIndex;
}

// Zero-distance test with 0.001f, including true for NaN.
// PE sums z squared, y squared, then x squared. No square root or component-wise epsilon.
bool SquadCoordinator::squadHasPosition()
{
    const float x = -m_v108.x;
    const float y = -m_v108.y;
    const float z = -m_v108.z;
    return !(z * z + y * y + x * x < 0.001f);
}

// PE inlined at : four stores in this order.
void SquadCoordinator::unAssignSquad()
{
    m_strategicAreaAssignment = 0;
    m_i40 = 4;
    m_i44 = 0;
    m_i120 = 0;
}

// Allocation is not checkPointer-wrapped. The human arm
// queries the one-based side's commander before publishing the member in the vector.
void SquadCoordinator::addSquadMember(IAIPlayer* player)
{
    SquadMember* member = new SquadMember(this, player, m_squadIndex, m_side,
                                         static_cast<int>(m_squadMembers.size()));
    if (!member->isBot())
    {
        if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            unAssignSquad();
    }
    m_squadMembers.push_back(member);
    if (member->isAllowedInFormation())
    {
        m_formation->joinFormation(member->getMemberIndex());
        m_formation->updateMovementFlag(true);
    }
    member->updateMemberPos();
}

// Level reset: the maneuver and repair state clear, the never-happened time returns to -20, the
// formation rejoins its leader, the fire records go, the slots are rebuilt and every member resets.
// The need-orders timer restarts at the current game time. PE 101A2470.
void SquadCoordinator::reset()
{
    m_d48 = -20.0;
    m_strategicAreaAssignment = 0;
    m_i40 = 4;
    m_i44 = 0;
    m_i76 = 4;
    m_pendingArea = 0;
    m_i80 = 0;
    m_i120 = 0;
    m_formation->joinSquadLeader();
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); )
    {
        delete *it;
        it = m_list12.erase(it);
    }
    m_formation->rebuildSlots();
    m_v108.x = m_v108.y = m_v108.z = 0.0f;
    m_d132 = 0.0;
    m_i148 = 0;
    m_d140 = 0.0;
    m_i152 = 0;
    m_b156 = false;
    m_b157 = false;
    m_b172 = false;
    m_b196 = false;
    m_v200.x = m_v200.y = m_v200.z = 0.0f;
    m_needOrderMessageTimer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_b220 = false;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->resetMember();
}

// Removes the member at index without deleting it: its fire records go first, then its formation
// slot, then the vector entry; the remaining members are renumbered by position before the leader
// and the formation slots are rebuilt. No range guard, as in retail. PE 101A45A0.
void SquadCoordinator::removeSquadMember(int index)
{
    SquadMember* const member = m_squadMembers[index];
    removeSquadMemberFire(member);
    if (m_formation->isInFormation(member->getMemberIndex()))
        m_formation->leaveFormation(member->getMemberIndex());
    m_squadMembers.erase(m_squadMembers.begin() + index);
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setMemberIndex(static_cast<int>(i));
    updateSquadLeader();
    m_formation->rebuildSlots();
}

// Writes the order position of the strategic object with the greatest gravity at m_v28 for the
// member's vehicle type; false, leaving the output alone, without a member, an object or a mobile
// unit vehicle. PE 101A1960; the name is [INFERRED].
bool SquadCoordinator::getMemberOrderPos(SquadMember* member, Vec3& position)
{
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(m_v28, m_side + 1);
    if (!member || !object)
        return false;
    IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
    if (!vehicle)
        return false;
    Information* const information = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    if (!information || !information->getUnit() || !information->getMobile())
        return false;
    position = object->getOrderPos(information->getMobile()->getVehicleNumber(), m_side + 1);
    return true;
}

// PE 101A3000. The leader is read before either branch and is not range-checked. The squad-order
// arm treats positions as equal when every component differs by at most FLT_EPSILON.
void SquadCoordinator::setMission(int type, const Vec3& position, int value, bool commander)
{
    SquadMember* const leader = m_squadMembers[m_i96];
    if (commander)
    {
        const bool busy = (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && isAnyMemberNearGoal()) ||
                          m_i120 != 0;
        if (busy && !IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            return;
        m_i120 = 1;
        m_d132 = m_d124 + 10.0;
        m_v64 = position;
        m_b92 = commander;
        m_i76 = type;
        m_i80 = value;
        m_pendingArea = 0;
        return;
    }
    if (m_squadMembers.empty())
        return;
    const float epsilon = 1.1920929e-07f;
    const bool same = std::fabs(position.x - m_v200.x) <= epsilon && std::fabs(position.y - m_v200.y) <= epsilon &&
                      std::fabs(position.z - m_v200.z) <= epsilon;
    if (same || leader->hasBot())
        return;
    m_b220 = true;
    m_v200 = position;
    clearMemberPaths();
    m_formation->leaveSquadLeader();
}

// PE 101A1D60.
void SquadCoordinator::getPendingMission(PendingMission& mission) const
{
    mission.area = m_pendingArea;
    mission.position = m_v64;
    mission.type = m_i76;
    mission.value = m_i80;
    mission.time = m_d84;
    mission.commander = m_b92;
}

// PE 101A1750.
void SquadCoordinator::requestRepair(const Vec3& position)
{
    m_b172 = true;
    m_v176 = position;
    m_d188 = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + 60.0;
    m_b196 = false;
}

// PE 101A2BF0.
void SquadCoordinator::clearMemberPaths()
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i])
            m_squadMembers[i]->setHavePath(false);
    }
}

// PE 101A1F40. Each candidate draws x then z; the draw is u32 * width * 2^-31 from the low edge.
bool SquadCoordinator::findValidPositionNear(SquadMember* member, Vec3& position, float halfWidth)
{
    if (isPositionValidForMember(member, position))
        return true;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    Vec3 candidate;
    candidate.x = 0.0f;
    candidate.y = 0.0f;
    candidate.z = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        candidate.x = static_cast<float>(static_cast<double>(AIRandom::m_instance->next()) * halfWidth *
                                         4.656612873077393e-10 +
                                         (static_cast<double>(position.x) - halfWidth));
        candidate.z = static_cast<float>(static_cast<double>(AIRandom::m_instance->next()) * halfWidth *
                                         4.656612873077393e-10 +
                                         (static_cast<double>(position.z) - halfWidth));
        candidate.y = terrain->getTerrainAltitude(candidate.x, candidate.z);
        if (isPositionValidForMember(member, candidate))
        {
            position = candidate;
            return true;
        }
    }
    return false;
}

float SquadCoordinator::getSupportDemand(SquadMember*, unsigned int, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SquadCoordinator::getSupportDemand");
}

// PE 101A3120. The closest object to the pending position is looked up after the assignment is
// cleared, so the "already assigned" test only passes when no object is found. A position
// accepted without an object skips the accepted-flag test. The leader index is not range-checked.
bool SquadCoordinator::acceptMission(SquadMember* member)
{
    const bool result = false;
    bool accepted = false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_strategicAreaAssignment = 0;
    AIStrategicArea* const closest = static_cast<AIStrategicArea*>(
        AIStrategicObjectManager::getInstance()->getClosestObject(m_v64, m_side + 1));
    if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && closest && m_i76 == 0 &&
        !closest->getStatus(m_side + 1))
    {
        m_strategicAreaAssignment = 0;
        m_i44 = 0;
        m_i120 = 0;
        m_i40 = 4;
        return false;
    }
    if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander() && m_i40 == m_i76 &&
        m_strategicAreaAssignment == closest)
        return true;

    bool announce = false;
    if (closest)
    {
        const Vec2& centre = closest->getPosition(m_side + 1);
        const float px = centre.x;
        const float pz = centre.y;
        const float height = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getTerrainAltitude(px, pz);
        float reach = closest->m_size[m_side];
        reach = static_cast<float>(
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getIgnoreCommanderDistance() + reach);
        bool take = closest->isInsideArea(m_v64);
        if (!take)
        {
            const float dx = m_v64.x - px;
            const double dy = static_cast<double>(m_v64.y) - height;
            const double dz = static_cast<double>(m_v64.z) - pz;
            const double distance = std::sqrt(static_cast<double>(dx) * dx + dz * dz + dy * dy);
            take = distance < reach && closest->getStatus(m_side + 1);
        }
        if (take)
        {
            m_v64.x = px;
            m_v64.y = height;
            m_v64.z = pz;
            m_v28 = m_v64;
            m_d48 = now;
            m_strategicAreaAssignment = closest;
            m_i120 = 2;
            m_i40 = m_i76;
            m_i44 = m_i80;
            m_b56 = m_b92;
            accepted = true;
        }
    }

    if (!m_strategicAreaAssignment)
    {
        Vec3 position = m_v64;
        if (findValidPositionNear(member, position, 30.0f))
        {
            m_v64 = position;
            m_i120 = 2;
            m_v28 = m_v64;
            m_i40 = m_i76;
            m_i44 = m_i80;
            m_d48 = now;
            m_b56 = m_b92;
            announce = true;
        }
        else
        {
            if (member->hasBot())
            {
                member->getBot()->getCommunicationAgent()->enqueueRadioMessage(7, 4, 3, 2.0f, 0);
                if (member->isSquadLeader())
                {
                    IBFSquad* const squad = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
                    if (m_i76 == 0)
                        squad->declineCommanderAttackOrder(m_squadIndex + 1, member->getPlayer());
                    else if (m_i76 == 1)
                        squad->declineCommanderDefendOrder(m_squadIndex + 1, member->getPlayer());
                    else if (m_i76 == 2)
                        squad->declineCommanderMoveOrder(m_squadIndex + 1, member->getPlayer());
                }
            }
            m_strategicAreaAssignment = 0;
            m_i40 = 4;
            m_i44 = 0;
            m_i120 = 0;
            clearMemberPaths();
        }
    }
    if (!announce && !accepted)
        return result;

    SquadMember* const leader = m_squadMembers[m_i96];
    if (leader->hasBot())
    {
        CommunicationAgent* const agent = leader->getBot()->getCommunicationAgent();
        if (IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
            agent->enqueueRadioMessage(6, 4, 3, 2.5f, 0);
        if (m_i40 == 0)
        {
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->issueSquadAttackOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->acceptCommanderAttackOrder(m_squadIndex + 1, member->getPlayer());
            agent->enqueueRadioMessage(16, 3, 3, 2.0f, 0);
        }
        else if (m_i40 == 1)
        {
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->issueSquadDefendOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->acceptCommanderDefendOrder(m_squadIndex + 1, member->getPlayer());
            agent->enqueueRadioMessage(15, 3, 3, 2.0f, 0);
        }
        else if (m_i40 == 2)
        {
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->issueSquadMoveOrder(m_v28, m_squadIndex + 1, m_side + 1, 0);
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
                ->acceptCommanderMoveOrder(m_squadIndex + 1, member->getPlayer());
            agent->enqueueRadioMessage(17, 3, 3, 2.0f, 0);
        }
    }
    else if (!IAIMain::m_instance->getSAI(m_side + 1)->isPlayerCommander())
    {
        if (m_i40 == 0 || m_i40 == 1)
            SquadManager::m_instance->queueCommanderMessage(m_side, m_squadIndex, m_i40, 3.0);
    }
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
        m_squadMembers[i]->setHavePath(false);
    return true;
}

// Removes the first member with the same player id, without deleting either object. PE 101A4730.
void SquadCoordinator::removeSquadMember(SquadMember* member)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        const int playerId = member->getPlayerId();
        if (playerId == m_squadMembers[i]->getPlayerId())
        {
            removeSquadMember(static_cast<int>(i));
            return;
        }
    }
}

// Deletes every fire record of this member's player and drops it from the list. PE 101A2D30.
void SquadCoordinator::removeSquadMemberFire(SquadMember* member)
{
    const int playerId = member->getPlayer()->getId();
    for (std::list<SquadMemberFire*>::iterator it = m_list12.begin(); it != m_list12.end(); )
    {
        if ((*it)->m_playerId == playerId)
        {
            delete *it;
            it = m_list12.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Stop on the first engine-recognized leader; later members
// are not visited. No found leader leaves the old index intact. A first leader skips the
// change arm, but the single-member trailing-position reset still runs.
void SquadCoordinator::updateSquadLeader()
{
    const int oldLeader = m_i96;
    bool found = false;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        m_squadMembers[i]->setIsSquadLeader(
            static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)->isSquadLeader(
                m_squadIndex + 1, m_squadMembers[i]->getPlayer()));
        if (m_squadMembers[i]->isSquadLeader())
        {
            m_i96 = static_cast<int>(i);
            found = true;
            break;
        }
    }
    if (!found)
        return;

    if (oldLeader != m_i96 && oldLeader != -1 && !m_squadMembers.empty())
    {
        m_squadMembers[m_i96]->updateMemberPos();
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
        // Next: formation->joinSquadLeader, updateSquadPosition, updateSquadPaths
        // then the human-leader/commander policy. Those movement bodies remain unrecovered.
        BF2_AI_FRONTIER("dice::hfe::ai::SquadCoordinator::updateSquadLeader (SquadFormation::joinSquadLeader)");
    }
    if (m_squadMembers.size() == 1)
        m_formation->resetTrailingPositions();
}

// - one indexed load through the vector's data pointer.
// no bounds check. Same shape as `BotManager::getBot`, and the same reason to leave it alone.
SquadMember* SquadCoordinator::getSquadMember(int index)
{
    return m_squadMembers[index];
}

// First matching player wins. The output index is written
// only on success, and the vector/member is fetched again after the player-id call.
// PE 101A2C30. The primary is asked for with the coordinator's side made one-based.
void SquadCoordinator::shareTarget(int playerId, unsigned int target)
{
    Vec3 normal;
    normal.x = normal.y = normal.z = 0.0f;
    Information* const information = informationManager->getInformation(target);
    if (!information)
        return;
    const unsigned int primaryHandle = information->getPrimaryObject(m_side + 1);
    Information* const primary = informationManager->getInformation(primaryHandle);
    if (!primary)
        return;
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        SquadMember* const member = m_squadMembers[i];
        if (member && member->hasBot() && member->getPlayerId() != playerId)
            member->getBot()->getSensingAgent()->getSenses()->addSeen(
                primaryHandle, normal, static_cast<unsigned int>(-1), primary, false, false);
    }
}

SquadMember* SquadCoordinator::getSquadMember(int playerId, int& index)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->getPlayerId() == playerId)
        {
            index = static_cast<int>(i);
            return m_squadMembers[i];
        }
    }
    return 0;
}

// Skip null members, but do not guard their player.
// Compare the complete returned integer; no alive/bot/leader filter exists. Both images
// refresh the member vector after a nonmatching virtual getSpawnKit callback.
bool SquadCoordinator::hasMemberWithKit(KitType type)
{
    for (unsigned int i = 0; i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i] && m_squadMembers[i]->getPlayer()->getSpawnKit() == type)
            return true;
    }
    return false;
}

// Only pointer presence is tested;
// there is no area validity, membership, side or mission-state query.
// PE 101A12B0.
bool SquadCoordinator::hasNoMission()
{
    return m_i120 == 0;
}

// PE 101A1280. The pending twin of isSquadAssignedToArea.
bool SquadCoordinator::isSquadPendingAssignedToArea(int areaIndex)
{
    return m_pendingArea && m_pendingArea->getIndex() == areaIndex;
}

bool SquadCoordinator::isSquadAssignedToAnyArea()
{
    return m_strategicAreaAssignment != 0;
}

// Return the stored pointer unchanged.
// ELF callers use it directly as AIStrategicArea::getRouteList/orderNormalBot's this;
// the paired kit callers read its area index at +0xF0. No ownership is transferred.
AIStrategicArea* SquadCoordinator::getSquadStrategicAreaAssignment()
{
    return m_strategicAreaAssignment;
}

// - one store of a double.
void SquadCoordinator::resetNeedOrderMessageTimer(double now)
{
    m_needOrderMessageTimer = now;
}

// PE 101A1DA0. The vehicle is not null-checked, as shipped.
bool SquadCoordinator::isPositionValidForMember(SquadMember* member, const Vec3& position)
{
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    Information* const vehicle = informationManager->getInformation(
        member->getPlayer()->getCurrentVehicle()->getInformation(m_side + 1));
    IPIMobile* mobile = vehicle->getMobile();
    unsigned int polygon;
    unsigned int quadTree;
    if (mobile)
    {
        mobile->getLastValidPosition();
        const int equipmentType = vehicle->getUnit()->getEquipmentType();
        const int map = mobile->getVehicleNumber();
        if (!pathfinding->getPolygon(position, map, polygon, quadTree, equipmentType))
            return false;
        const int equipmentType2 = vehicle->getUnit()->getEquipmentType();
        const int map2 = mobile->getVehicleNumber();
        return pathfinding->isValidPosition(map2, equipmentType2, position, quadTree, true);
    }

    Information* const soldier = informationManager->getInformation(
        member->getPlayer()->getSoldier()->getInformation(m_side + 1));
    mobile = soldier->getMobile();
    const unsigned int lastQuadTree = mobile->getLastValidPosition().quadTree;
    const int equipmentType = soldier->getUnit()->getEquipmentType();
    const int map = mobile->getVehicleNumber();
    if (!pathfinding->getPolygon(position, map, polygon, quadTree, equipmentType))
        return false;
    if (lastQuadTree == static_cast<unsigned int>(-1) || quadTree != lastQuadTree)
        return false;
    const int equipmentType2 = soldier->getUnit()->getEquipmentType();
    const int map2 = mobile->getVehicleNumber();
    return pathfinding->isValidPosition(map2, equipmentType2, position, lastQuadTree, true);
}

// PE 101A17A0. The material query's own result is ignored.
bool SquadCoordinator::isMaterialFourAt(SquadMember* member, const Vec3& position)
{
    IAIObject* const vehicleObject = member->getPlayer()->getCurrentVehicle();
    if (!vehicleObject)
        return true;
    Information* const vehicle =
        informationManager->getInformation(vehicleObject->getInformation(m_side + 1));
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    IPIMobile* const mobile = vehicle->getMobile();
    unsigned char material;
    const int equipmentType = vehicle->getUnit()->getEquipmentType();
    pathfinding->getMaterial(position, mobile->getVehicleNumber(), equipmentType, material);
    return material == 4;
}

// PE 101A1230.
bool SquadCoordinator::isMissionStatusTwo()
{
    return m_i120 == 2;
}

// PE 101A1930.
bool SquadCoordinator::getSquadGotoPos(Vec3& position)
{
    if (m_i44 != 0)
        return false;
    position = m_v28;
    return true;
}

// PE 101A2FA0.
bool SquadCoordinator::isAnyMemberNearGoal()
{
    if (!m_strategicAreaAssignment || !m_strategicAreaAssignment->getStatus(m_side + 1))
        return false;
    for (unsigned int i = 0; !m_squadMembers.empty() && i < m_squadMembers.size(); ++i)
    {
        if (m_squadMembers[i]->isNearSquadGoal())
            return true;
    }
    return false;
}

// PE 101A1A10. The vehicle's information must carry unit and mobile plug-ins.
bool SquadCoordinator::getObjectOrderPos(SquadMember* member, AIStrategicObject* object, Vec3& position)
{
    if (!member || !object)
        return false;
    IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
    if (!vehicle)
        return false;
    Information* const info = informationManager->getInformation(vehicle->getInformation(m_side + 1));
    if (!info || !info->getUnit() || !info->getMobile())
        return false;
    position = object->getOrderPos(info->getMobile()->getVehicleNumber(), m_side + 1);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
