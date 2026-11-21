// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotSenses operations.

#include "BotSenses.h"

#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "BotMain.h"
#include "SensingAgent.h"
#include <cmath>
#include "../Main/AIRandom.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/IBotManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Core/Debug.h"
#include "../../../../Math/Mat4.h"

#include <algorithm>

namespace dice {
namespace hfe {
namespace ai {

namespace {
float distanceSquared(const Vec3& a, const Vec3& b);
} // anonymous namespace

// Creates the sight frustum and initializes sensing collections for the supplied bot.
BotSenses::BotSenses(BotMain* owner)
    : m_bot(owner)
    // the camera. See the block above.
    , m_frustum(checkPointer(new Frustum()))
    , m_double12(-1000000.0)
    , m_int32(0)
    , m_int36(0)
    , m_int40(0)
    , m_attackerStrength(0.0f)
    , m_attackerStrengthOwnKind(0.0f)
{
    // the constructor'S last statement - see the block above. The binary copies the list's head
    // pointer; in source that is the list's own end. The cursor at +80 is deliberately not here.
    m_cursor60 = m_list56.end();
}

// PE 101BD6A0. Deletes the sensed-object and fire records and resets per-life sensing state.
void BotSenses::reset()
{
    for (SensedObjectMap::iterator it = m_sensedObjects.begin(); it != m_sensedObjects.end(); )
    {
        // PE 101BD6C0: a record that added a contact removes it from the object's information.
        if (it->second->m_contactAdded)
        {
            if (Information* const information = informationManager->getInformation(it->second->getHandle()))
                information->removeContact();
        }
        delete it->second;
        m_sensedObjects.erase(it++);
    }
    m_sensedObjects.clear();

    m_map44.clear();
    m_list56.clear();
    m_cursor60 = m_list56.end();
    m_sortBuffer.clear();

    m_int40 = 0;
    m_int36 = 0;
    m_int32 = 0;
    m_double12 = -1000000.0;

    m_set64.clear();
    m_list76.clear();
    // The constructor never did this; the cursor at +80 becomes valid here for the first time.
    m_cursor80 = m_list76.end();

    for (std::list<FireEntry>::iterator it = m_list84.begin(); it != m_list84.end(); ++it)
    {
        delete it->first;   // PE 101BD840
    }
    m_list84.clear();

    m_attackerStrength = 0.0f;
    m_attackerStrengthOwnKind = 0.0f;

    m_map100.clear();
}

// Resets senses and deletes the frustum; member containers release their own storage.
BotSenses::~BotSenses()
{
    reset();
    delete m_frustum;
}

// [FRONTIER] Remaining sight, hearing and communication operations require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

SensedObject* BotSenses::getSensedObject(unsigned int handle) const
{
    const SensedObjectMap::const_iterator it = m_sensedObjects.find(handle);
    if (it == m_sensedObjects.end())
        return 0;
    return it->second;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] The record layouts are not recovered; these stop when
// the senses first hold a record. [RETIRE-WHEN: bot-senses-records] Recover FireObject and
// SensedObject and validate a bot that sees and hears an enemy.
float FireObject::getStrength(int kind) const
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    return static_cast<float>(m_strengths[kind] / ((now - m_time) * m_decay + 1.0f));
}

// PE 101B8840. The target's mobile plug-in is not null-checked once a sound sphere reaches the
// bot. The self-noise term takes the larger of 1 and the speed ratio, as shipped.
float BotSenses::getHearingProbability(unsigned int /*handle*/, Information* info)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
        return 0.0f;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - m_bot->getDouble172() < 1.5)
        return 0.0f;
    if (IPIPhysical* const physical = info->getPhysical())
    {
        if (physical->getHealth() <= 0.0f)
            return 0.0f;
    }
    IPIMobile* const mobile = info->getMobile();
    IPIArmament* const armament = info->getArmament();
    if (!armament && !mobile)
        return 0.0f;
    if (IAIObjectPhysical* const objectPhysical = info->getAIObject()->getPhysical())
    {
        if (objectPhysical->getHealth() <= 0.0f)
            return 0.0f;
    }
    IPIMobile* const ownMobile = equipment->getMobile();
    float radius = mobile ? mobile->getSoundSphereRadius() : 0.0f;
    float weaponRadius = 0.0f;
    if (armament)
    {
        WeaponInfo* const weapon = armament->getLastFiredWeapon();
        if (weapon && now - weapon->getTimeOfFiring() < 5.0)
            weaponRadius = weapon->getSoundSphereRadius();
    }
    if ((info->getObjectTypeFlags() >> 21) & 1)
    {
        const int pose = info->getSoldierPose();
        if (pose >= 2 && pose <= 3)
            radius = 0.0f;
    }
    if (radius < weaponRadius)
        radius = weaponRadius;
    float selfNoise;
    if (ownMobile)
    {
        const float ratio = ownMobile->getSpeedMagnitude() / ownMobile->getMaxSpeed();
        selfNoise = 1.0f < ratio ? ratio : 1.0f;
    }
    else
    {
        selfNoise = 0.0f;
    }
    const Vec3& at = info->getAIObject()->getPosition();
    const Vec3& own = equipment->getPosition();
    const float dx = at.x - own.x;
    const float dy = at.y - own.y;
    const float dz = at.z - own.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance > radius)
        return 0.0f;
    distance = distance / mobile->getSoundSphereRadiusMax();
    float probability;
    if (ownMobile)
        probability = ownMobile->getHearingProbabilityMin() * (1.0f - distance) +
                      ownMobile->getHearingProbabilityMax() * distance;
    else
        probability = 1.0f;
    const float threshold = (1.0f - selfNoise) * probability;
    if (!(AIRandom::m_instance->nextFloat() > threshold))
        return 0.0f;
    return probability;
}

// PE 101BCEF0.
void BotSenses::updateHearing(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();
    if (now - m_bot->getDouble172() < 1.5)
        return;
    const double deadline = timer->getUpdateTime() + slice;
    if (!informationManager->getInformation(m_bot->getCurrentEquipment()))
        return;
    const unsigned int count = static_cast<unsigned int>(m_list76.size());
    const int side = m_bot->getSide();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (!(timer->getUpdateTime() < deadline))
            return;
        SensedObject* const object = m_list76.front().second;
        m_list76.pop_front();
        const unsigned int handle = object->m_handle;
        Information* const info = informationManager->getInformation(handle);
        if (!info || (!info->getMobile() && !info->getArmament()))
        {
            m_set64.erase(handle);
            object->m_senseFlags &= ~1u;
            if (!(object->m_senseFlags & 3))
            {
                m_sensedObjects.erase(object->m_handle);
                delete object;
            }
            continue;
        }
        const float probability = getHearingProbability(handle, info);
        if (probability > 0.0f)
        {
            object->m_lastSeen = now;
            object->m_senseFlags |= 0x10;
            object->m_lastSense = 0x10;
            Information* const primary = informationManager->getInformation(info->getPrimaryObject(side));
            const float certainty = probability < 0.1f ? 0.1f : probability;
            if (primary->setTime(timer->getGameTime(), certainty))
                primary->getAIObject()->addInformationToGrid(side);
            int iterator = -1;
            for (Information* seat = informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
                 seat; seat = informationManager->getInformation(primary->getSecondaryObject(iterator, side)))
            {
                seat->setTime(timer->getGameTime(), probability < 0.1f ? 0.1f : probability);
            }
            m_list76.push_back(TimedObject(now, object));
        }
        else if (object->m_senseFlags & 0x10)
        {
            object->m_senseFlags &= ~0x10u;
            object->m_lastSeen = now;
            m_list76.push_back(TimedObject(now, object));
        }
        else if (now - object->m_lastSeen > 30.0)
        {
            m_set64.erase(object->m_handle);
            object->m_senseFlags &= ~1u;
            if (!(object->m_senseFlags & 3))
            {
                m_sensedObjects.erase(object->m_handle);
                delete object;
            }
        }
        else
        {
            m_list76.push_back(TimedObject(now, object));
        }
    }
}

// PE 101BC1F0.
void BotSenses::updateFire(const double&)
{
    m_attackerStrength = 0.0f;
    m_attackerStrengthOwnKind = 0.0f;
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    IPIPhysical* const physical = equipment ? equipment->getPhysical() : 0;
    const int kind = physical ? physical->getStrengthType() : 0;
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);

    double shots = static_cast<double>(static_cast<unsigned int>(m_list84.size()));
    if (!(shots >= 1.0))
        shots = 1.0;
    double window = 60.0 / shots;
    if (5.0 < window)
        window = 5.0;
    for (std::list<FireEntry>::iterator it = m_list84.begin(); it != m_list84.end();)
    {
        FireObject* const shot = it->first;
        const float weight = shot->m_heavy ? 10.0f : 1.0f;
        for (int k = 0; k < 6; ++k)
            m_attackerStrength += shot->getStrength(k) * weight;
        if (physical)
            m_attackerStrengthOwnKind += shot->getStrength(kind) * weight;
        bool drop;
        if (timer->getGameTime() - shot->m_time > window)
        {
            std::map<unsigned int, FireObject*>::iterator found = m_map100.find(it->second);
            drop = found == m_map100.end() || found->second != shot;
        }
        else
        {
            std::pair<std::map<unsigned int, FireObject*>::iterator, bool> inserted =
                m_map100.insert(std::make_pair(it->second, static_cast<FireObject*>(0)));
            if (inserted.second || inserted.first->second->m_time < shot->m_time)
            {
                inserted.first->second = shot;
                drop = false;
            }
            else
            {
                drop = inserted.first->second != shot;
            }
        }
        if (drop)
        {
            delete shot;
            it = m_list84.erase(it);
        }
        else
        {
            ++it;
        }
    }

    m_recentAttackerStrength = 0.0f;
    double shooters = static_cast<double>(static_cast<unsigned int>(m_map100.size()));
    if (!(shooters >= 1.0))
        shooters = 1.0;
    double recent = 30.0 / shooters;
    if (!(15.0 < recent))
        recent = 15.0;
    const double now = timer->getGameTime();
    for (std::map<unsigned int, FireObject*>::iterator it = m_map100.begin(); it != m_map100.end();)
    {
        if (now - it->second->m_time < recent)
        {
            m_recentAttackerStrength += it->second->getStrength(kind);
            ++it;
        }
        else
        {
            m_map100.erase(it++);
        }
    }
}

// PE 101BFA10.
void BotSenses::updateSensing(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    double deadline = timer->getUpdateTime() + slice;
    const double hearingSlice = slice * 0.1;
    updateHearing(hearingSlice);
    timer->getUpdateTime();
    updateFire(deadline);
    const double remaining = deadline - timer->getUpdateTime();
    updateSeen(remaining);
}

// PE 101B8750. An unknown pose is a logged error, as shipped.
void BotSenses::getBotSoldierSensePoint(Vec3& target, Vec3& offset, Information* info)
{
    const int pose = info->getSoldierPose();
    if (pose == 3)
    {
        offset.x = 0.0f;
        offset.y = -0.7f;
        offset.z = 0.0f;
    }
    else if (pose == 2 || pose == 1)
    {
        offset.x = offset.y = offset.z = 0.0f;
    }
    else
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotSenses.cpp", 207, "") << "Unknown pose";
    }
    const Vec3& at = info->getPosition();
    target.x = at.x + offset.x;
    target.y = at.y + offset.y;
    target.z = at.z + offset.z;
}

void BotSenses::probeRequestedPose(int requested, int current, const Vec3& eye, const Vec3& target,
                                   const std::vector<IAIObject*>& ignore, bool targetIsBot,
                                   float tolerance, unsigned int handle)
{
    float y;
    if (requested == 2)
        y = current == 1 ? eye.y - 0.53f : eye.y + 0.38f;
    else if (requested == 3)
        y = current == 1 ? eye.y - 0.91f : eye.y - 0.38f;
    else
        return;
    Vec3 raised;
    raised.x = eye.x;
    raised.y = y;
    raised.z = eye.z;
    bool clear = false;
    const bool open = !static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
                           ->collideLineWithWorld(raised, target, ignore, targetIsBot, tolerance, true, clear);
    m_bot->setPoseStatus(requested, handle, open, clear);
}

// PE 101BE980. Walks the seen list from the cursor, re-tracing each record: the remembered
// point, then a fresh random one. A record seen again moves to the back with its position and
// time refreshed and is shared with the vehicle's other bots; one not seen is marked reported,
// loses its contact after two seconds and is dropped once too old. While the bot fires at a
// seen target it also tests the sight line for its requested pose. Stops at the slice, after
// a full lap or when the list empties.
void BotSenses::updateSeen(const double& remaining)
{
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    IBotManager* const manager = IBotManager::instance;
    const bool render = manager->getRenderBotSensing();
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    const double now = timer->getGameTime();
    if (m_list56.empty())
        return;
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    const bool ownSoldier = ((equipment->getObjectTypeFlags() >> 21) & 1) != 0;
    const float near = equipment->getPhysical()->getRadius() * 3.0f;
    // Unlike see, the third entry is never trimmed: a stale exposed soldier stays ignored.
    std::vector<IAIObject*> ignore;
    ignore.push_back(equipment->getAIObject()->getPrimaryObject());
    ignore.push_back(0);
    ignore.push_back(0);
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    Vec3 eye;
    eye.x = camera.m[12];
    eye.y = camera.m[13] - 0.1f;
    eye.z = camera.m[14];
    const TimedList::iterator lapStart = m_cursor60;
    const int side = m_bot->getSide();

    for (;;)
    {
        if (m_cursor60 == m_list56.end())
            m_cursor60 = m_list56.begin();
        const TimedList::iterator node = m_cursor60++;
        SensedObject* const object = node->second;
        Information* const info = informationManager->getInformation(object->m_handle);

        bool keep = false;
        bool forgetNow = !info || !info->getAIObject() || !info->isRootObject() ||
                         info->getSide() == m_bot->getSide() || !info->getSide();
        if (!forgetNow && info->getPhysical() && info->getPhysical()->getHealth() <= 0.0f)
            forgetNow = true;
        if (forgetNow)
        {
            m_list56.erase(node);
            m_map44.erase(object->m_handle);
            object->m_senseFlags &= ~2u;
            if (!(object->m_senseFlags & 3))
            {
                m_sensedObjects.erase(object->m_handle);
                delete object;
            }
        }
        else
        {
            const bool primary = info->isPrimary();
            const bool soldier = ((info->getObjectTypeFlags() >> 21) & 1) != 0;
            const float radius = info->getAIObject()->getBoundingRadius();
            const Vec3& at = info->getAIObject()->getPosition();
            const float d2 = distanceSquared(eye, at);
            bool inView;
            if (!primary && !soldier)
            {
                inView = false;
            }
            else if (now - object->m_double40 <= 5.0)
            {
                inView = true;
            }
            else
            {
                const float reach = radius + radius + near;
                if (ownSoldier && d2 <= reach * reach)
                    inView = true;
                else if (!m_frustum->inside(info->getAIObject()->getPosition(), radius))
                    inView = false;
                else if (!info->getUnit())
                    inView = true;
                else
                    inView = info->getUnit()->getCullDistanceSqr() >= d2;
            }
            const unsigned int flags = equipment->getObjectTypeFlags();
            const bool soldierView = ((flags >> 4) & 1) || ((flags >> 5) & 1);
            bool seen = inView || (soldierView && isCloseBehind(equipment, info->getAIObject()));

            Vec3 target;
            target.x = target.y = target.z = 0.0f;
            Vec3 normal;
            normal.x = normal.y = normal.z = 0.0f;
            bool clear = false;
            Bot* const targetBot = manager->getBot(info->getAIObject());
            const bool targetIsBot = targetBot ? targetBot->getLodLevel() >= 1 : false;
            unsigned int part = static_cast<unsigned int>(-1);
            bool traced = false;

            if (seen)
            {
                ignore[1] = info->getAIObject();
                if (IPIUnit* const unit = info->getUnit())
                {
                    if (soldier)
                    {
                        Information* const occupied =
                            informationManager->getInformation(unit->getOccupiedObject(m_bot->getSide()));
                        if (occupied && occupied->getUnit() && occupied->getUnit()->hasExposedSoldier())
                            ignore[2] = occupied->getAIObject();
                    }
                    else if (unit->hasExposedSoldier())
                    {
                        Information* const occupier =
                            informationManager->getInformation(unit->getOccupier(m_bot->getSide()));
                        if (occupier)
                            ignore[2] = occupier->getAIObject();
                    }
                }

                // The remembered point first.
                bool tryRandom = true;
                if (soldier && targetIsBot)
                {
                    getBotSoldierSensePoint(target, normal, info);
                    seen = !environment->collideLineWithWorld(eye, target, ignore, targetIsBot, 0.75f, true, clear);
                    const int current = equipment->getSoldierPose();
                    const int requested = m_bot->getRequestedSolderPose();
                    if (seen && current != requested && current != 4 &&
                        m_bot->getFiringTarget() == object->m_handle)
                        probeRequestedPose(requested, current, eye, target, ignore, targetIsBot, 0.75f,
                                           object->m_handle);
                    tryRandom = false;
                }
                else if (!soldier)
                {
                    target = info->getAIObject()->getPosition();
                    normal.x = normal.y = normal.z = 0.0f;
                    seen = !environment->collideLineWithWorld(eye, target, ignore, targetIsBot, 0.75f, true, clear);
                    if (!seen && targetIsBot)
                        tryRandom = false;
                    else if (!seen)
                    {
                        normal = object->m_senseNormal;
                        Vec3 offset;
                        offset.x = offset.y = offset.z = 0.0f;
                        const Mat4& transform = info->getAIObject()->getTransformation();
                        offset.x = normal.x * transform.m[0] + normal.y * transform.m[4] + normal.z * transform.m[8] + transform.m[12];
                        offset.y = normal.x * transform.m[1] + normal.y * transform.m[5] + normal.z * transform.m[9] + transform.m[13];
                        offset.z = normal.x * transform.m[2] + normal.y * transform.m[6] + normal.z * transform.m[10] + transform.m[14];
                        target = offset;
                        seen = !environment->collideLineWithWorld(eye, target, ignore, false, 0.75f, true, clear);
                    }
                }
                else
                {
                    const unsigned int rememberedPart = object->m_part;
                    const bool found = environment->getSoldierSensePosition(target, normal, info->getAIObject(),
                                                                            rememberedPart);
                    part = object->m_part;
                    if (found)
                    {
                        seen = !environment->collideLineWithWorld(eye, target, ignore, false, 0.75f, true, clear);
                        const int current = equipment->getSoldierPose();
                        const int requested = m_bot->getRequestedSolderPose();
                        if (seen && current != requested && m_bot->getFiringTarget() == object->m_handle)
                            probeRequestedPose(requested, current, eye, target, ignore, false, 0.8f,
                                               object->m_handle);
                    }
                    else
                    {
                        seen = false;
                    }
                }

                // A fresh random point when the remembered one failed.
                if (!seen && tryRandom)
                {
                    if (soldier)
                    {
                        environment->pickSoldierRandomSensePosition(target, normal, info->getAIObject(), part);
                        object->m_part = part;
                    }
                    else
                    {
                        environment->pickRandomSensePosition(target, normal, eye, info->getAIObject());
                    }
                    seen = !environment->collideLineWithWorld(eye, target, ignore, false, 0.75f, true, clear);
                    const int current = equipment->getSoldierPose();
                    const int requested = m_bot->getRequestedSolderPose();
                    if (seen && requested != current && m_bot->getFiringTarget() == object->m_handle)
                        probeRequestedPose(requested, current, eye, target, ignore, false, 0.8f,
                                           object->m_handle);
                }
                traced = true;
            }

            if (seen)
            {
                info->setTime(now);
                if (object->m_senseFlags & 8)
                {
                    if (!object->m_contactAdded)
                    {
                        info->addContact();
                        object->m_contactAdded = true;
                    }
                    object->m_senseFlags &= ~8u;
                }
                object->m_position = target;
                object->m_positionTime = now;
                object->m_lastPosition = target;
                object->m_lastUpdate = now;
                object->m_senseNormal = normal;
                if (!clear)
                {
                    object->m_senseFlags |= 0x40;
                    object->m_lastSense = 0x40;
                }
                else
                {
                    object->m_senseFlags &= ~0x40u;
                }
                Information* const vehicle = equipment->isPrimary()
                    ? equipment
                    : informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
                Bot* const driver = manager->getBot(vehicle->getAIObject());
                if (driver && driver != static_cast<Bot*>(m_bot))
                {
                    driver->getSensingAgent()->getSenses()->addSeen(object->m_handle, object->m_senseNormal,
                                                                    part, info, false, clear);
                    if (manager->getRenderBotSensing())
                    {
                        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: ai-debug-render]
                        BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::updateSeen (sensing render)");
                    }
                }
                int iterator = -1;
                for (unsigned int seat = vehicle->getFirstSecondaryObject(iterator, m_bot->getSide());;
                     seat = vehicle->getSecondaryObject(iterator, m_bot->getSide()))
                {
                    Information* const seatInformation = informationManager->getInformation(seat);
                    if (!seatInformation)
                        break;
                    Bot* const passenger = manager->getBot(seatInformation->getAIObject());
                    if (passenger && passenger != static_cast<Bot*>(m_bot))
                    {
                        passenger->getSensingAgent()->getSenses()->addSeen(
                            object->m_handle, object->m_senseNormal, part, info, false, clear);
                        if (manager->getRenderBotSensing())
                        {
                            // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: ai-debug-render]
                            BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::updateSeen (sensing render)");
                        }
                    }
                }
                keep = true;
            }
            else if (!(object->m_senseFlags & 8))
            {
                object->m_firstSensed = now;
                object->m_senseFlags |= 8;
                object->m_lastSense = 8;
                keep = true;
            }
            else
            {
                const double age = now - object->m_firstSensed;
                Information* const lost = informationManager->getInformation(object->m_handle);
                if (age > 2.0 && object->m_contactAdded)
                {
                    object->m_contactAdded = false;
                    lost->removeContact();
                    int iterator;
                    for (Information* seat = informationManager->getInformation(
                             lost->getFirstSecondaryObject(iterator, m_bot->getSide()));
                         seat; seat = informationManager->getInformation(
                             lost->getSecondaryObject(iterator, m_bot->getSide())))
                        seat->removeContact();
                }
                Information* const own = informationManager->getInformation(m_bot->getCurrentEquipment());
                const bool antiAircraft = own->getArmament() && ((lost->getObjectTypeFlags() >> 4) & 1) &&
                                          own->getArmament()->isAntiAircraft() > 0;
                bool forget;
                if (age > 60.0 || !primary)
                    forget = true;
                else if (!own->getMobile() && age > 5.0)
                    forget = true;
                else if (antiAircraft && age > 5.0)
                    forget = true;
                else
                    forget = m_bot->getIsGoingForFlag() && age > 5.0;
                keep = !forget;
            }
            (void)traced;

            if (render)
            {
                // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: ai-debug-render]
                BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::updateSeen (sensing render)");
            }

            if (keep)
            {
                m_list56.erase(node);
                m_list56.push_back(TimedObject(now, object));
                m_map44[object->m_handle] = --m_list56.end();
            }
            else
            {
                m_list56.erase(node);
                m_map44.erase(object->m_handle);
                object->m_senseFlags &= ~2u;
                if (!(object->m_senseFlags & 3))
                {
                    m_sensedObjects.erase(object->m_handle);
                    delete object;
                }
            }
        }

        const double elapsed = timer->getUpdateTime() - start;
        if (m_cursor60 == lapStart)
            break;
        if (!(elapsed < remaining))
            break;
        if (m_list56.empty())
            break;
    }
}

// PE 101E1510. The sense point, part, first-sensed and last-seen times are left unset.
void BotSenses::forgetObject(unsigned int handle)
{
    Information* const info = informationManager->getInformation(handle);
    if (!info)
        return;
    unsigned int primary = info->getPrimaryObject(m_bot->getSide());
    Information* const primaryInfo = informationManager->getInformation(primary);
    const std::map<unsigned int, TimedList::iterator>::iterator found = m_map44.find(primary);
    if (found == m_map44.end())
        return;
    SensedObject* const object = found->second->second;
    if (m_cursor60->second == object)
        m_cursor60 = m_list56.end();
    if (found->second != m_list56.end())
        m_list56.erase(found->second);
    m_map44.erase(found);
    if (object->m_contactAdded)
    {
        primaryInfo->removeContact();
        object->m_contactAdded = false;
    }
    object->m_senseFlags &= ~2u;
    if (!(object->m_senseFlags & 3u))
    {
        m_sensedObjects.erase(object->m_handle);
        delete object;
    }
}

bool BotSenses::isAttacking(unsigned int handle, double& time) const
{
    const std::map<unsigned int, FireObject*>::const_iterator found = m_map100.find(handle);
    if (found == m_map100.end())
    {
        time = -1000.0;
        return false;
    }
    time = found->second->m_time;
    return true;
}

SensedObject::SensedObject(unsigned int handle, bool direct, bool sided)
    : m_handle(handle)
    , m_double40(-1000.0)
    , m_senseFlags((direct ? 0u : 8u) + (direct ? 0u : 0x10u) + (direct ? 2u : 1u) + (sided ? 4u : 0u))
    , m_lastSense(2)
    , m_lastUpdate(0.0)
    , m_positionTime(0.0)
    , m_contactAdded(false)
{
    m_senseNormal.x = m_senseNormal.y = m_senseNormal.z = 0.0f;
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_lastPosition.x = m_lastPosition.y = m_lastPosition.z = 0.0f;
}

namespace {

// One entry per view kind and step of the view cycle: the sight cone's field of view and the
// camera-turn threshold below which the cycle restarts. PE 102ADAB8, writable. [INFERRED] names.
struct ViewSetting
{
    float fov;
    float threshold;
};
ViewSetting s_viewSettings[2][3] = {
    { { 1.7453293f, 0.0f }, { 1.0471976f, 0.0f }, { 0.5235988f, 0.0f } },
    { { 1.3089969f, 0.0f }, { 0.7853982f, 0.0f }, { 0.2617994f, 0.0f } },
};

// Enemy and neutral objects within view distance; a non-soldier view also needs a unit's
// cull distance, a soldier view lets units through only inside 300. Objects of type 0x15 must
// be physical and alive. PE vtable 10284428, PE 101B85D0. [INFERRED] name.
class SightPredicate : public AIObjectPredicate
{
public:
    bool predicate(IAIObject* object) override
    {
        const int side = object->getSide();
        if (!side)
        {
            const Vec3& at = object->getPosition();
            const float dx = at.x - m_position.x;
            const float dy = at.y - m_position.y;
            const float dz = at.z - m_position.z;
            return dz * dz + dy * dy + dx * dx < m_rangeSquared;
        }
        if (side == m_side)
            return false;
        if (object->isOfType(static_cast<InformationType>(0x15)))
        {
            if (!object->isPhysical())
                return false;
            if (!(object->getPhysical()->getHealth() > 0.0f))
                return false;
        }
        const Vec3& at = object->getPosition();
        const float dx = m_position.x - at.x;
        const float dy = m_position.y - at.y;
        const float dz = m_position.z - at.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (object->isUnit() && !m_soldierView)
        {
            if (!(distanceSquared < m_rangeSquared))
                return false;
            return object->getUnit()->getCullDistanceSqr() > distanceSquared;
        }
        if (!m_soldierView)
            return distanceSquared < m_rangeSquared;
        if (!(distanceSquared < m_rangeSquared))
            return false;
        return distanceSquared < 90000.0f;
    }

    /* +0x04 */ int   m_side;
    /* +0x08 */ Vec3  m_position;
    /* +0x14 */ float m_rangeSquared;
    /* +0x18 */ bool  m_soldierView;
};

bool closerFirst(const std::pair<float, unsigned int>& a, const std::pair<float, unsigned int>& b)
{
    return a.first < b.first;
}

float distanceSquared(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // anonymous namespace

// PE 101B8AA0.
bool BotSenses::isCloseBehind(Information* equipment, IAIObject* object)
{
    Information* const primary = informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
    if (!primary)
        return false;
    const Mat4& transform = primary->getTransformation();
    if (!object)
        return false;
    Information* const information = informationManager->getInformation(object->getInformation(equipment->getSide()));
    if (!information || !information->isPrimary())
        return false;
    const Vec3& at = object->getPosition();
    const float dx = transform.m[12] - at.x;
    const float dy = transform.m[13] - at.y;
    const float dz = transform.m[14] - at.z;
    if (!(dz * dz + dy * dy + dx * dx < 40000.0f))
        return false;
    return dz * transform.m[10] + dy * transform.m[9] + dx * transform.m[8] > 0.0f;
}

// PE 101BD8F0. A first sighting creates the record (reported until seen directly), stores the
// object's position and joins the seen list; a direct sighting also adds a contact, reporting
// a new one over the radio, while a reported one alerts the sensing agent. A record already
// in the seen list is refreshed only when it was reported and is now seen directly.
void BotSenses::addSeen(unsigned int handle, const Vec3& normal, unsigned int part,
                        Information* info, bool direct, bool clear)
{
    const int side = m_bot->getSide();
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (info->setTime(now))
        info->getAIObject()->addInformationToGrid(side);

    std::map<unsigned int, TimedList::iterator>::iterator seen = m_map44.find(handle);
    if (seen == m_map44.end())
    {
        SensedObject* object;
        SensedObjectMap::iterator found = m_sensedObjects.find(handle);
        if (found == m_sensedObjects.end())
        {
            object = checkPointer(new SensedObject(handle, true, info->getSide() != 0));
            object->m_firstSensed = now;
            object->m_senseFlags |= 8;
            object->m_lastSense = 8;
            m_sensedObjects.insert(SensedObjectMap::value_type(handle, object));
        }
        else
        {
            object = found->second;
        }
        object->m_senseFlags |= 2;
        object->m_lastSense = 2;
        object->m_position = info->getPosition();
        object->m_positionTime = now;
        object->m_lastPosition = info->getPosition();
        object->m_lastUpdate = now;
        if (direct)
        {
            object->m_senseFlags &= ~8u;
            if (!object->m_contactAdded)
            {
                if (!info->hasContact())
                    m_bot->getCommunicationAgent()->reportContact(info);
                info->addContact();
                object->m_contactAdded = true;
            }
        }
        else
        {
            m_bot->getSensingAgent()->setAlerted();
        }
        if (!clear)
        {
            object->m_senseFlags |= 0x40;
            object->m_lastSense = 0x40;
        }
        object->m_lastSeen = now;
        object->m_senseNormal = normal;
        object->m_part = part;
        m_list56.push_back(TimedObject(now, object));
        m_map44[handle] = --m_list56.end();
        return;
    }

    SensedObject* const object = seen->second->second;
    if (!(object->m_senseFlags & 8) || !direct)
        return;
    TimedList::iterator node = seen->second;
    if (m_cursor60 == node)
        ++m_cursor60;
    m_list56.erase(node);
    if (!object->m_contactAdded)
    {
        info->addContact();
        object->m_contactAdded = true;
    }
    object->m_senseNormal = normal;
    object->m_part = part;
    object->m_position = info->getPosition();
    object->m_positionTime = now;
    object->m_lastPosition = info->getPosition();
    object->m_senseFlags &= ~8u;
    object->m_lastUpdate = now;
    m_list56.push_back(TimedObject(now, object));
    seen->second = --m_list56.end();
}

// PE 101BC540.
void BotSenses::addHeard(unsigned int handle)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
        return;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - m_bot->getDouble172() < 1.5)
        return;
    if (handle == m_bot->getCurrentEquipment())
        return;
    Information* const info = informationManager->getInformation(handle);
    if (!info)
        return;
    if (info->getSide() == m_bot->getSide())
        return;
    if (!((equipment->getObjectTypeFlags() >> 21) & 1) && ((info->getObjectTypeFlags() >> 21) & 1))
        return;
    const float chance = getHearingProbability(handle, info);
    if (!(chance > 0.0f))
        return;
    const int side = m_bot->getSide();
    if (m_set64.insert(handle).second)
    {
        SensedObjectMap::iterator found = m_sensedObjects.find(handle);
        info->getAIObject()->addInformationToGrid(info->getSide());
        SensedObject* object;
        if (found == m_sensedObjects.end())
        {
            object = checkPointer(new SensedObject(handle, false, info->getSide() != 0));
            m_sensedObjects.insert(SensedObjectMap::value_type(handle, object));
            m_bot->getSensingAgent()->setAlerted();
        }
        else
        {
            object = found->second;
            object->m_senseFlags |= 1;
            object->m_lastSense = 1;
        }
        object->m_handle = handle;
        object->m_lastSeen = now;
        object->m_senseFlags |= 1;
        object->m_lastSense = 1;
        object->m_position = info->getPosition();
        object->m_positionTime = now;
        Information* const primary = informationManager->getInformation(info->getPrimaryObject(side));
        if (primary->setTime(now, std::max(chance, 0.1f)))   // not null-checked, as shipped
            primary->getAIObject()->addInformationToGrid(side);
        m_list76.push_back(TimedObject(now, object));
        return;
    }

    // Already queued: the record exists; neither lookup is checked, as shipped.
    SensedObject* const object = m_sensedObjects.find(handle)->second;
    object->m_senseFlags |= 1;
    object->m_lastSense = 1;
    object->m_position = info->getPosition();
    object->m_positionTime = now;
    object->m_lastSeen = now;
    informationManager->getInformation(info->getPrimaryObject(side))->setTime(now, std::max(chance, 0.1f));
}

// PE 101BDDC0.
void BotSenses::see(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    m_bot->getName();
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    m_narrowView = (equipment->getObjectTypeFlags() >> 22) & 1;
    if (m_int36 == 3)
    {
        m_bot->setQuadSeen();
        m_int36 = 0;
        const Mat4& previous = m_frustum->m_transformation;
        const float turn = camera.m[10] * previous.m[10] + camera.m[9] * previous.m[9] +
                           camera.m[8] * previous.m[8];
        if (m_int32 == 0)
            m_int32 = 1;
        else if (m_int32 == 1)
            m_int32 = 2;
        else
            m_int32 = 0;
        if (!(turn > s_viewSettings[m_narrowView][m_int32].threshold))
            m_int32 = 0;
    }
    IAIPlayer* const player = m_bot->getPlayer();
    player->getFieldOfView();
    const float viewDistance = player->getViewDistance();
    m_frustum->setPerspective(s_viewSettings[m_narrowView][0].fov, 1.0f, 0.01f, viewDistance, camera);

    switch (m_int36)
    {
    case 0:
    {
        m_vector124.resize(0);
        const int side = m_bot->getSide();
        const unsigned int flags = equipment->getObjectTypeFlags();
        const bool soldierView = ((flags >> 4) & 1) || ((flags >> 5) & 1);
        SightPredicate predicate;
        predicate.m_side = side;
        predicate.m_position = equipment->getPosition();
        predicate.m_soldierView = soldierView;
        predicate.m_rangeSquared = viewDistance * viewDistance;
        const float near = equipment->getPhysical()->getRadius() * 3.0f;
        const float nearSquared = near * near;
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
            ->getWorldPlayerOccupiedObjects(m_vector124, &predicate);
        m_sortBuffer.resize(0);
        const Vec3& position = equipment->getPosition();
        for (std::vector<IAIObject*>::iterator it = m_vector124.begin(); it != m_vector124.end(); ++it)
        {
            IAIObject* const object = *it;
            Information* const information =
                informationManager->getInformation(object->getInformation(m_bot->getSide()));
            const float radius = object->getBoundingRadius();
            if (!m_frustum->inside(object->getPosition(), radius))
            {
                bool add;
                if (((equipment->getObjectTypeFlags() >> 21) & 1) ||
                    (information && !((information->getObjectTypeFlags() >> 21) & 1)))
                {
                    add = !(distanceSquared(position, object->getPosition()) > nearSquared);
                }
                else
                {
                    add = soldierView && isCloseBehind(equipment, object);
                }
                if (!add)
                    continue;
            }
            const unsigned int handle = object->forceGetInformation(side, false);
            const Vec3& at = object->getPosition();
            m_sortBuffer.push_back(std::make_pair(distanceSquared(position, at), handle));
            if (object->isUnit())
            {
                IAIObjectUnit* const unit = object->getUnit();
                if (unit->hasExposedSoldier() && unit->isOccupied())
                {
                    IAIObject* const occupier = unit->getOccupier();
                    const unsigned int occupierHandle = occupier->forceGetInformation(side, false);
                    m_sortBuffer.push_back(
                        std::make_pair(distanceSquared(occupier->getPosition(), position), occupierHandle));
                }
            }
            else
            {
                Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotSenses.cpp", 413, "")
                    << object->getName() << " has no unit.";
            }
            int iterator;
            for (IAIObject* child = object->getFirstSecondaryObject(iterator); child;
                 child = object->getSecondaryObject(iterator))
            {
                IAIObjectUnit* const unit = child->getUnit();
                if (!unit || !unit->hasExposedSoldier() || !unit->isOccupied())
                    continue;
                IAIObject* const occupier = unit->getOccupier();
                if (!occupier)
                    continue;
                const unsigned int occupierHandle = occupier->forceGetInformation(side, false);
                m_sortBuffer.push_back(
                    std::make_pair(distanceSquared(occupier->getPosition(), position), occupierHandle));
            }
        }
        m_int40 = 0;
        m_double12 = timer->getGameTime();
        m_int36 = 1;
        if (timer->getUpdateTime() - start > slice)
            return;
    }
    // fall through
    case 1:
        equipment->getPosition();
        std::sort(m_sortBuffer.begin(), m_sortBuffer.end(), closerFirst);
        m_int36 = 2;
        if (timer->getUpdateTime() - start > slice)
            return;
    // fall through
    case 2:
    {
        const Vec3& position = equipment->getPosition();
        // The trace ignores the equipment's primary object, the target and an exposed soldier.
        // Once a target has none, the list stays at two entries for the rest of the run.
        std::vector<IAIObject*> ignore;
        ignore.push_back(equipment->getAIObject()->getPrimaryObject());
        ignore.push_back(0);
        ignore.push_back(0);
        IBotManager* const manager = IBotManager::instance;
        const bool render = manager->getRenderBotSensing();
        const int side = m_bot->getSide();
        IAIObjectEnvironment* const environment =
            static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
        double elapsed = timer->getUpdateTime() - start;
        equipment->getPhysical()->getRadius();
        while (m_int40 < static_cast<int>(m_sortBuffer.size()) && elapsed < slice)
        {
            const unsigned int handle = m_sortBuffer[m_int40].second;
            Information* information = 0;
            if (m_map44.find(handle) == m_map44.end())
                information = informationManager->getInformation(handle);
            if (information && information->isRootObject())
            {
                IAIObject* const object = information->getAIObject();
                Bot* const targetBot = manager->getBot(object);
                const bool targetIsBot = targetBot ? targetBot->getLodLevel() >= 1 : false;
                object->getBoundingRadius();
                ignore[1] = object;
                bool exposed = false;
                if (IPIUnit* const unit = information->getUnit())
                {
                    if ((information->getObjectTypeFlags() >> 21) & 1)
                    {
                        Information* const occupied =
                            informationManager->getInformation(unit->getOccupiedObject(m_bot->getSide()));
                        if (occupied && occupied->getUnit() && occupied->getUnit()->hasExposedSoldier())
                        {
                            if (ignore.size() > 2)
                                ignore[2] = occupied->getAIObject();
                            exposed = true;
                        }
                    }
                    else if (unit->hasExposedSoldier())
                    {
                        Information* const occupier =
                            informationManager->getInformation(unit->getOccupier(m_bot->getSide()));
                        if (occupier)
                        {
                            if (ignore.size() > 2)
                                ignore[2] = occupier->getAIObject();
                            exposed = true;
                        }
                    }
                }
                if (!exposed)
                    ignore.resize(2);

                Vec3 eye;
                eye.x = camera.m[12];
                eye.y = camera.m[13] - 0.1f;
                eye.z = camera.m[14];
                const Vec3& at = object->getPosition();
                const float distance = std::sqrt(distanceSquared(position, at));
                const float samplesFloat = distance > 0.0f
                    ? object->getBoundingRadius() / distance * 30.0f : 1e14f;
                int samples = static_cast<int>(samplesFloat);
                if (samples < 1)
                    samples = 1;
                else if (samples > 10)
                    samples = 10;
                unsigned int part = static_cast<unsigned int>(-1);
                bool clear = false;
                Vec3 normal;
                bool visible = false;
                for (int i = 0; i < samples; ++i)
                {
                    const bool soldier = ((information->getObjectTypeFlags() >> 21) & 1) != 0;
                    Vec3 target;
                    target.x = target.y = target.z = 0.0f;
                    normal.x = normal.y = normal.z = 0.0f;
                    if (!soldier && i == 0 && samples != 1)
                        target = object->getPosition();
                    else if (soldier)
                        environment->pickSoldierRandomSensePosition(target, normal, object, part);
                    else
                        environment->pickRandomSensePosition(target, normal, eye, object);
                    visible = !environment->collideLineWithWorld(eye, target, ignore, targetIsBot,
                                                                 0.8f, true, clear);
                    if (render)
                    {
                        // The sight line goes to the debug renderer (PE 10080660).
                        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: ai-debug-render]
                        // Recover the renderer and validate with bot sensing rendering enabled.
                        BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::see (sensing render)");
                    }
                    if (visible)
                        break;
                }
                if (visible)
                {
                    Information* vehicle = equipment;
                    if (!vehicle->isPrimary())
                        vehicle = informationManager->getInformation(vehicle->getPrimaryObject(m_bot->getSide()));
                    if (Bot* const driver = manager->getBot(vehicle->getAIObject()))
                        driver->getSensingAgent()->getSenses()->addSeen(
                            handle, normal, part, information, driver == static_cast<Bot*>(m_bot), clear);
                    int iterator = -1;
                    for (unsigned int seat = vehicle->getFirstSecondaryObject(iterator, m_bot->getSide());;
                         seat = vehicle->getSecondaryObject(iterator, m_bot->getSide()))
                    {
                        Information* const seatInformation = informationManager->getInformation(seat);
                        if (!seatInformation)
                            break;
                        if (Bot* const passenger = manager->getBot(seatInformation->getAIObject()))
                            passenger->getSensingAgent()->getSenses()->addSeen(
                                handle, normal, part, information, passenger == static_cast<Bot*>(m_bot), clear);
                    }
                }
            }
            ++m_int40;
            elapsed = timer->getUpdateTime() - start;
        }
        if (m_int40 >= static_cast<int>(m_sortBuffer.size()))
        {
            m_int36 = 3;
            m_int40 = 0;
        }
        if (!(timer->getUpdateTime() - start > slice))
            timer->getUpdateTime();
        return;
    }
    case 3:
        timer->getUpdateTime();
        return;
    default:
        return;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
