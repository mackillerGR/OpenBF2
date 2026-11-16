#include "BotBehaviours.h"

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIBotBehaviourPattern/PlanGenerators.h"
#include "../AIBotBehaviourPattern/PlanGeneratorSupport.h"
#include "../Main/AIDllFrontier.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"

#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/CommunicationAgent.h"
#include "../Main/IBotManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../Game/Objects/IPlayer.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadFormation.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/WaypointList.h"
#include "../AIBot/BotSenses.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/AIMain.h"
#include "../Main/AIRandom.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "CustomCurve.h"
#include <algorithm>
#include <cstring>
#include <list>
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../AIInterface/BFKnowledge/IWeapon.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-bot-behaviours] Every
// calculateUrgency body and the per-class extra virtuals below stop by name until recovered and
// exercised by a bot that runs the behaviour.

// ---- SampledFunction -----------------------------------------------------------------------

SampledFunction::SampledFunction(Function function, float low, float high, float step)
    : m_function(function)
    , m_table(0)
    , m_low(low)
    , m_high(high)
    , m_step(step)
{
    build();
}

SampledFunction::~SampledFunction()
{
    delete[] m_table;
}

// PE 101B1CB0. The count truncates; the end points are sampled exactly.
void SampledFunction::build()
{
    if (m_table)
        delete[] m_table;
    m_count = static_cast<int>((m_high - m_low) / m_step);
    m_table = new float[m_count + 1];
    const double interval = static_cast<double>(m_high - m_low) / m_count;
    for (int i = 0; i <= m_count; ++i)
        m_table[i] = m_function(static_cast<float>(i * interval + m_low));
    m_table[0] = m_function(m_low);
    m_table[m_count] = m_function(m_high);
}

float SampledFunction::sample(float x, bool interpolate) const
{
    // The shipped body loads its second argument slot with fld; BBFire pushes the whole dword 1.
    const unsigned int rawOutside = interpolate ? 1u : 0u;
    float outside;
    std::memcpy(&outside, &rawOutside, sizeof(outside));
    if (m_low > x || x > m_high)
        return outside;
    const float step = (m_high - m_low) / static_cast<float>(m_count);
    const float inverse = 1.0f / step;
    const int index = static_cast<int>((x - m_low) * inverse);
    if (!interpolate)
        return m_table[index];
    if (index >= m_count)
        return outside;
    const float fraction = ((x - m_low) - static_cast<float>(index) * step) * inverse;
    return (m_table[index + 1] - m_table[index]) * fraction + m_table[index];
}

namespace {

// PE 10100B70: the fire curve BBFire samples.
float fireCurve(float value)
{
    return static_cast<float>(std::pow(static_cast<double>(value + value - 0.95f), 21.0) * 1.012f
                              + 1.0f);
}

} // anonymous namespace

namespace {

// True when the equipment carries a weapon of one of the two types. [CHOSEN, not recovered]
// Retail inlines the scan at each caller.
bool hasWeaponOfType(Information* equipment, int typeA, int typeB)
{
    IPIArmament* const armament = equipment->getArmament();
    const std::vector<unsigned int>& weapons = armament->getWeapons();
    for (size_t i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (weapon)
        {
            const int type = weapon->getWeaponType();
            if (type == typeA || type == typeB)
                return true;
        }
    }
    return false;
}

} // anonymous namespace

// ---- Behaviours ----------------------------------------------------------------------------

BBAvoid::BBAvoid(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBAvoid::~BBAvoid()
{
    delete[] m_targets;
}

namespace {

Information* lookupInformation(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

// PE 100EDB40: the vector turned by the matrix's rotation, rows as basis vectors.
void rotateByMatrix(const Mat4& matrix, const Vec3& in, Vec3& out)
{
    const float* const m = matrix.m;
    const float z = in.x * m[2] + in.z * m[10] + in.y * m[6];
    const float y = in.x * m[1] + in.z * m[9] + in.y * m[5];
    const float x = in.z * m[8] + in.y * m[4] + in.x * m[0];
    out.x = x;
    out.y = y;
    out.z = z;
}

// The equipment's primary object, as the given handle when it is primary itself.
unsigned int primaryHandle(Information* information, unsigned int handle, Bot* bot)
{
    return information->isPrimary() ? handle : information->getPrimaryObject(bot->getSide());
}

// PE 100EDD70. Time until the moving sphere pair overlaps, when that happens within the look-
// ahead: the closest approach must fall inside 1.5 times the squared combined radius. Writes the
// collision time and, on a hit, the point where it happens (relative), and returns 1; else 0.
float collisionCourse(const Vec3& relative, const Vec3& velocity, float lookAhead, float radius,
                      Vec3& point, float& collisionTime)
{
    const float radiusSq = radius * radius * 1.5f;
    const double speedSq = static_cast<double>(velocity.x) * velocity.x +
                           static_cast<double>(velocity.y) * velocity.y +
                           static_cast<double>(velocity.z) * velocity.z;
    if (speedSq < 1e-06)
        return 0.0f;
    const double speed = std::sqrt(speedSq);
    const double unit = 1.0 / speed;
    const float dx = static_cast<float>(unit * velocity.x);
    const float dy = static_cast<float>(unit * velocity.y);
    const double dz = unit * velocity.z;
    const float inverse = static_cast<float>(1.0 / speed);
    const float approach = static_cast<float>(
        ((-relative.z * dz + static_cast<double>(-relative.y) * dy) + static_cast<double>(-relative.x) * dx) *
        inverse);
    if (approach < 0.0f)
        return 0.0f;
    const float cx = approach * velocity.x + relative.x;
    const double cy = static_cast<double>(approach) * velocity.y + relative.y;
    const double cz = static_cast<double>(approach) * velocity.z + relative.z;
    const double closestSq = (cz * cz + cy * cy) + static_cast<double>(cx) * cx;
    if (!(closestSq < radiusSq))
        return 0.0f;
    const float time = static_cast<float>(approach - std::sqrt(std::fabs(closestSq - radiusSq)) * inverse);
    collisionTime = time;
    if (time > lookAhead)
        return 0.0f;
    if (time < 0.0f)
        return 0.0f;
    point.x = velocity.x * time + relative.x;
    point.y = velocity.y * time + relative.y;
    point.z = velocity.z * time + relative.z;
    return 1.0f;
}

} // anonymous namespace

// PE 100EDF10. Soldiers only. Every gathered obstacle that is not the bot's own vehicle, its
// change or repair target or another soldier is weighed: a hostile one - weak, flagged
// dangerous, or carrying no weapon of type 4 - inside its reach (its smallest radius plus 4,
// at most 10) adds a flat 100 and steers away from it; anything else adds its collision
// weight when it closes faster than 3 and will hit within the look-ahead. With nothing to
// avoid, a recent avoid position or a blinding gas sends the bot off at urgency 1000.
float BBAvoid::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    if (!flag)
        return holdUrgency(bot);

    Vec3 centre;
    centre.x = centre.y = centre.z = 0.0f;
    Vec3 relative;
    relative.x = relative.y = relative.z = 0.0f;
    Vec3 relativeVelocity;
    relativeVelocity.x = relativeVelocity.y = relativeVelocity.z = 0.0f;
    Vec3 hitPoint;
    hitPoint.x = hitPoint.y = hitPoint.z = 0.0f;
    const unsigned int previousTarget = m_targets[bot->getBotId()];
    Information* const equipment = lookupInformation(bot->getCurrentEquipment());
    const unsigned int ownPrimary = equipment->isPrimary() ? bot->getCurrentEquipment()
                                                          : equipment->getPrimaryObject(bot->getSide());
    equipment->getName();   // called and discarded, as shipped
    float urgency = 0.0f;
    float strongest = 0.0f;
    IPIMobile* const mobile = equipment->getMobile();
    if (!mobile || !((equipment->getObjectTypeFlags() >> 21) & 1))
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    IPIPhysical* const physical = equipment->getPhysical();
    Vec3 offset;
    offset.x = offset.y = offset.z = 0.0f;
    rotateByMatrix(equipment->getTransformation(), physical->getSphereLocalOffset(), offset);
    const Vec3& position = equipment->getPosition();
    centre.x = offset.x + position.x;
    centre.y = offset.y + position.y;
    centre.z = offset.z + position.z;
    const float ownRadius = physical->getSmallestRadius();
    const Vec3 ownSpeed = mobile->getSpeed();
    float lookAhead = mobile->getAvoidCollisionLookAhead();
    if (equipment->getAIObject())
    {
        if (equipment->getSoldierPose() == 3)
            lookAhead = lookAhead + lookAhead;
        else if (equipment->getSoldierPose() == 2)
            lookAhead = lookAhead * 1.5f;
    }

    unsigned int changeTarget = bot->getChangeTarget();
    if (Information* const change = lookupInformation(changeTarget))
    {
        if (!change->isPrimary())
            changeTarget = change->getPrimaryObject(bot->getSide());
    }
    unsigned int repairTarget = bot->getRepairTarget();
    if (Information* const repair = lookupInformation(repairTarget))
    {
        if (!repair->isPrimary())
            repairTarget = repair->getPrimaryObject(bot->getSide());
    }

    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();
    const unsigned int count = static_cast<unsigned int>(obstacles.size());
    for (unsigned int i = 0; i < count; ++i)
    {
        const unsigned int handle = obstacles[i];
        Information* const other = lookupInformation(handle);
        if (!other)
            continue;
        IPIMobile* const otherMobile = other->getMobile();
        if (other->getObjectSpawnerHolding() && other->getObjectSpawner(bot->getSide()) == bot->getCurrentEquipment())
            continue;
        const unsigned int otherPrimary = primaryHandle(other, handle, bot);
        IPIPhysical* const otherPhysical = other->getPhysical();
        if (!otherPhysical || ownPrimary == otherPrimary || handle == repairTarget || handle == changeTarget ||
            handle == bot->getWantedRepairTarget() || ((other->getObjectTypeFlags() >> 21) & 1))
            continue;

        Vec3 otherOffset;
        otherOffset.x = otherOffset.y = otherOffset.z = 0.0f;
        rotateByMatrix(other->getTransformation(), otherPhysical->getSphereLocalOffset(), otherOffset);
        const Vec3& otherPosition = other->getPosition();
        Vec3 otherCentre;
        otherCentre.x = otherOffset.x + otherPosition.x;
        otherCentre.y = otherOffset.y + otherPosition.y;
        otherCentre.z = otherOffset.z + otherPosition.z;
        relative.x = otherCentre.x - centre.x;
        relative.y = otherCentre.y - centre.y;
        relative.z = otherCentre.z - centre.z;
        Vec3 otherSpeed;
        otherSpeed.x = otherSpeed.y = otherSpeed.z = 0.0f;
        if (otherMobile)
            otherSpeed = otherMobile->getSpeed();
        relativeVelocity.x = otherSpeed.x - ownSpeed.x;
        relativeVelocity.y = otherSpeed.y - ownSpeed.y;
        relativeVelocity.z = otherSpeed.z - ownSpeed.z;
        const float closingSq = relativeVelocity.x * relativeVelocity.x + relativeVelocity.y * relativeVelocity.y +
                                relativeVelocity.z * relativeVelocity.z;
        const float combinedRadius = (otherPhysical->getSmallestRadius() + ownRadius) * 0.60000002f;
        const float otherHealth = otherPhysical->getHealth();
        const float bailValue = IBotManager::instance->getVehicleBailValue();
        WeaponInfo* weapon = 0;
        IPIArmament* const armament = equipment->getArmament();
        if (armament && armament->getActiveWeapon() != -1)
            weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));

        bool hostile;
        bool weak = otherHealth < bailValue;
        if (!weak && !((other->getObjectTypeFlags() >> 25) & 1))
        {
            hostile = false;
        }
        else if (weapon && weapon->getWeaponType() == 4)
        {
            hostile = false;
        }
        else
        {
            hostile = true;
        }
        if (!hostile)
        {
            const float speedSq = otherSpeed.x * otherSpeed.x + otherSpeed.y * otherSpeed.y + otherSpeed.z * otherSpeed.z;
            if (speedSq < 4.0f)
                continue;
        }

        const float distance = std::sqrt(relative.x * relative.x + relative.y * relative.y + relative.z * relative.z);
        const float smallest = otherPhysical->getSmallestRadius();
        const float capped = 10.0f < smallest + 4.0f ? 10.0f : smallest + 4.0f;
        const float otherSmallest = otherPhysical->getSmallestRadius();
        const float reach = otherSmallest < capped ? capped : otherSmallest;
        if (hostile)
        {
            if (!(distance < reach))
                continue;
            urgency = urgency + 100.0f;
            strongest = 100.0f;
            m_targets[bot->getBotId()] = handle;
            Vec3 direction;
            direction.x = direction.y = direction.z = 0.0f;
            Vec3 where;
            where.x = where.y = where.z = 0.0f;
            const float push = (reach - distance) * 0.2f + 0.5f;
            const float weight = 1.0f < push ? push : 1.0f;
            static_cast<AvoidCollisionPlanGenerator*>(m_generator)->setObstacle(bot, direction, handle, where, weight);
            continue;
        }

        float collisionTime;
        const float hit = collisionCourse(relative, relativeVelocity, lookAhead, combinedRadius, hitPoint, collisionTime);
        if (!(hit > 0.0f))
            continue;
        if (closingSq < 9.0f)
            continue;
        float scale;
        if (collisionTime < 1.1920929e-07f)
            scale = 1.0f;
        else
            scale = std::sqrt(relative.x * relative.x + relative.y * relative.y + relative.z * relative.z);
        const float mass = otherPhysical->getMass();
        const float closing = std::sqrt(relativeVelocity.x * relativeVelocity.x +
                                        relativeVelocity.y * relativeVelocity.y +
                                        relativeVelocity.z * relativeVelocity.z);
        const float weight = closing * mass * (hit / scale);
        urgency = urgency + weight;
        if (weight > strongest)
        {
            strongest = weight;
            m_targets[bot->getBotId()] = handle;
            static_cast<AvoidCollisionPlanGenerator*>(m_generator)
                ->setObstacle(bot, hitPoint, handle, relativeVelocity, collisionTime);
        }
    }

    Vec3 avoidPosition;
    avoidPosition.x = avoidPosition.y = avoidPosition.z = 0.0f;
    bool flee = false;
    if (urgency == 0.0f)
    {
        double timeLeft;
        flee = bot->getBotNavigation()->getAvoidPosition(avoidPosition, timeLeft);
    }
    if (!flee)
    {
        world::IPlayer* const player = bot->getPlayer()->getPlayer();
        flee = player->getBlurValue() > 0.0f && player->getIsGassed();
    }
    if (flee)
    {
        Vec3 none;
        none.x = none.y = none.z = 0.0f;
        static_cast<AvoidCollisionPlanGenerator*>(m_generator)
            ->setObstacle(bot, none, static_cast<unsigned int>(-1), none, 0.0f);
        m_urgency[bot->getBotId()] = 1000.0f;
        return 1000.0f;
    }

    if (urgency == 0.0f)
    {
        const bool offered = m_generator->hasPlan(bot);
        m_targetChanged[bot->getBotId()] = offered;
        m_generator->reset(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders())
        urgency = urgency * bot->getMemory()->m_float8;
    if ((equipment->getObjectTypeFlags() >> 21) & 1)
    {
        const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        if (now - bot->getCommunicationAgent()->getDouble96() < 5.0)
            urgency = urgency + urgency;
    }
    m_urgency[bot->getBotId()] = urgency * elapsed;
    m_targetChanged[bot->getBotId()] = previousTarget != m_targets[bot->getBotId()];
    bot->insertBehaviourInformation(0.5f, lookupInformation(m_targets[bot->getBotId()])->getPosition(), m_behaviourId);
    return m_urgency[bot->getBotId()];
}

BBMoveTo::BBMoveTo(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = 0;
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBMoveTo::~BBMoveTo()
{
}

// PE 100F10D0. A squad member settled at its formation position stays put until the squad moves
// out. Otherwise the urgency is the waypoint list's, scaled by obedience under orders, and 1000
// while the member flees artillery the bot stands in. The unflagged pass keeps the stored
// urgency (zero once the generator has no plan) and feeds it to the move inclination.
float BBMoveTo::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    SquadMember* const member = bot->getSquadMember();
    if (member->isAtFormationPosition() && !member->getSquad()->getSquadFormation()->shouldSquadMoveOut())
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    if (!flag)
    {
        m_targetChanged[bot->getBotId()] = false;
        if (!m_generator->hasPlan(bot))
            m_urgency[bot->getBotId()] = 0.0f;
        bot->addToMoveInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    Information* const equipment = lookupInformation(bot->getCurrentEquipment());
    float urgency;
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders())
    {
        WaypointList* const waypoints = bot->getWaypoints();
        const float obedience = bot->getMemory()->m_float8;
        urgency = waypoints->update(equipment->getPosition(), bot) * obedience;
    }
    else
    {
        WaypointList* const waypoints = bot->getWaypoints();
        urgency = waypoints->update(equipment->getPosition(), bot);
    }
    m_urgency[bot->getBotId()] = urgency;

    double elapsedInZone;
    int zoneSide;
    const int side = bot->getSide();
    if (SquadManager::m_instance->isInArtilleryDangerZone(equipment->getPosition(), side, elapsedInZone, zoneSide))
    {
        SquadMember* const fleeing = bot->getSquadMember();
        if (fleeing && fleeing->isFleeingArtillery())
            m_urgency[bot->getBotId()] = 1000.0f;
    }
    m_urgency[bot->getBotId()] = elapsed * m_urgency[bot->getBotId()];
    if (!(m_urgency[bot->getBotId()] > 0.0f) && !(m_urgency[bot->getBotId()] != m_urgency[bot->getBotId()]))
    {
        const bool offered = m_generator->hasPlan(bot);
        m_targetChanged[bot->getBotId()] = offered;
        m_generator->reset(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    m_targetChanged[bot->getBotId()] = bot->getWaypoints()->isActive();
    static_cast<GotoWaypointPlanGenerator*>(m_generator)->setWaypoint(bot);
    bot->addToMoveInclination(m_urgency[bot->getBotId()]);
    Vec3 waypoint;
    waypoint.x = waypoint.y = waypoint.z = 0.0f;
    if (bot->getWaypoints()->getCurrentWaypoint(waypoint))
        bot->insertBehaviourInformation(0.5f, waypoint, m_behaviourId);
    return m_urgency[bot->getBotId()];
}

// Built without weights, urge curve or generator.
BBMoveToFixed::BBMoveToFixed(int behaviourId, int priority)
    : BotBehaviour(0, 0, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_targets = 0;
    m_generator = 0;
    const int bots = configuredBotCount();
    m_float24 = checkPointer(new float[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_float24[i] = 0.0f;
    }
}

BBMoveToFixed::~BBMoveToFixed()
{
    delete[] m_float24;
}

float BBMoveToFixed::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMoveToFixed::calculateUrgency");
}

BBMedicAssist::BBMedicAssist(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBMedicAssist::~BBMedicAssist()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBMedicAssist::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMedicAssist::calculateUrgency");
}

BBRevive::BBRevive(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

BBRevive::~BBRevive()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBRevive::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRevive::calculateUrgency");
}

BBBridgeRepair::BBBridgeRepair(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBBridgeRepair::~BBBridgeRepair()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBBridgeRepair::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBBridgeRepair::calculateUrgency");
}

// PE 100FD160. A bot that is up and carries a type 1 or 2 weapon (the medic kit) can assist.
bool BBMedicAssist::isApplicable(Information* equipment, Bot* bot)
{
    if (bot->getManDown() || !equipment->getArmament())
        return false;
    return hasWeaponOfType(equipment, 1, 2);
}

// PE 10120410. Any bot carrying a type 3 weapon (the defibrillator) can revive.
bool BBRevive::isApplicable(Information* equipment, Bot*)
{
    if (!equipment->getArmament())
        return false;
    return hasWeaponOfType(equipment, 3, 3);
}

// PE 10122CC0. Only a living engineer (kit 4) that is up.
bool BBBridgeRepair::isUsable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive() || bot->getManDown())
        return false;
    return bot->getPlayer()->getSpawnKit() == 4;
}

BBIdle::BBIdle(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_double24 = new double[configuredBotCount()];
    m_bool28 = new bool[configuredBotCount()];
    m_float2C = new float[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_double24[i] = 0.0;
        m_bool28[i] = false;
        m_float2C[i] = 0.0f;
    }
    m_targets = 0;
}

BBIdle::~BBIdle()
{
    delete[] m_double24;
    delete[] m_bool28;
    delete[] m_float2C;
}

// PE 100F46C0. Restores every bot's state, not just this bot's, and clears the target array
// pointer; neither the generator nor the base state is reset.
void BBIdle::reset(Bot*)
{
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_double24[i] = 0.0;
        m_bool28[i] = false;
        m_float2C[i] = 0.0f;
    }
    m_targets = 0;
}

// PE 100F4910. Type 2 messages are ride requests; the commander's player id is 255. A request
// is acceptable when the requester is a soldier near the bot's primary vehicle (25 m, or 50 m for
// the commander and for some equipment), the vehicle has a free seat and the bot is not moving.
void BBIdle::answerRideRequests(Bot* bot, unsigned int equipmentHandle, Information* equipment,
                                PlanGenerator* generator, bool moving, float elapsed,
                                float& urgency, int& target, bool& keep)
{
    const std::list<AIRadioMessageInfo>& requests =
        bot->getCommunicationAgent()->getReceivedMessages(static_cast<AIRadioMessageType>(2));
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (std::list<AIRadioMessageInfo>::const_iterator it = requests.begin(); it != requests.end();
         ++it)
    {
        Bot* const requesterBot = IBotManager::instance->getBotFromPlayerId(it->m_senderPlayerId);
        Information* const primary = lookupInformation(equipment->getPrimaryObject(bot->getSide()));
        const int primaryPlayerId = primary->getPlayerId();
        CommunicationAgent* const requesterAgent = requesterBot ? requesterBot->getCommunicationAgent() : 0;
        const bool fromCommander = it->m_senderPlayerId == 255;
        if (target >= 0 && !fromCommander)
            continue;
        if ((equipment->getObjectTypeFlags() >> 21) & 1)
            continue;
        if (requesterBot && primaryPlayerId != requesterAgent->getInt92())
            continue;
        IAIPlayer* const requester =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                ->getPlayerFromId(it->m_senderPlayerId);
        if (!requester)
            continue;
        IAIObject* const vehicle = requester->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const requesterInformation = lookupInformation(vehicle->getHandle(bot->getSide()));
        if (!requesterInformation || !((requesterInformation->getObjectTypeFlags() >> 21) & 1))
            continue;
        float reachSquared;
        const unsigned int flags = equipment->getObjectTypeFlags();
        if (((flags >> 5) & 1) || ((flags >> 4) & 1) || fromCommander)
            reachSquared = 2500.0f;
        else
            reachSquared = 625.0f;
        const float* const a = requesterInformation->getTransformation().m;
        const float* const b = primary->getTransformation().m;
        const float dx = b[12] - a[12];
        const float dy = b[13] - a[13];
        const float dz = b[14] - a[14];
        const float distanceSquared = dx * dx + dy * dy + dz * dz;

        bool accept = fromCommander
                      || static_cast<IdlePlanGenerator*>(generator)->hasFreeSeat(equipmentHandle,
                                                                                  bot->getSide());
        if (accept && distanceSquared > reachSquared)
            accept = false;
        if (accept && moving)
            accept = false;
        if (!accept)
        {
            if (bot->getCommunicationAgent()->handleMessage(*it))
            {
                bot->getCommunicationAgent()->enqueueRadioMessage(7, 6, 3, 1.0f, -1);
                return;
            }
            continue;
        }

        if (!m_bool28[bot->getBotId()] || m_double24[bot->getBotId()] == 0.0)
            m_double24[bot->getBotId()] = now;
        const double waited = now - m_double24[bot->getBotId()];
        if (!((requesterInformation->getObjectTypeFlags() >> 21) & 1) || equipment != primary
            || !equipment->getMobile() || ((equipment->getObjectTypeFlags() >> 21) & 1))
            continue;
        if (!(waited < 10.0))
            continue;
        if (target >= 0 && !fromCommander)
            continue;
        urgency = elapsed * it->m_weight;
        keep = true;
        target = it->m_senderPlayerId;
        m_bool28[bot->getBotId()] = true;
        if (target == 255)
            return;
    }
}

// PE 100F4C90. A damaged vehicle that is standing still and has an engineer on the way waits for
// repairs, at 60 times the elapsed time; otherwise ride requests are answered. Either way the
// bot then answers the passengers' requests to stop (type 11) for its own vehicle, and the idle
// generator is told whom to serve. Without a reason to keep waiting the wait state is cleared.
float BBIdle::calculateUrgency(Bot* bot, bool, float elapsed)
{
    PlanGenerator* const generator = m_generator;
    const unsigned int equipmentHandle = bot->getCurrentEquipment();
    Information* const equipment = lookupInformation(equipmentHandle);
    equipment->getPrimaryObject(bot->getSide());   // the shipped body makes this call and discards the result
    IPIMobile* const mobile = equipment->getMobile();
    bool keep = false;
    float urgency = elapsed;
    int target = -1;
    bool wait = false;
    bool cleared = true;
    if (equipment->isPrimary() && !((equipment->getObjectTypeFlags() >> 21) & 1) && mobile)
    {
        cleared = false;
        bool moving;
        if (bot->isAirBorn())
            moving = true;
        else
            moving = ((equipment->getObjectTypeFlags() >> 4) & 1) && mobile->getSpeedMagnitude() > 3.0f;
        IPIPhysical* const physical = equipment->getPhysical();
        const bool damaged = physical && physical->getHealth() < 0.9f;
        wait = !moving && damaged
               && (static_cast<BotMain*>(bot)->getIsRepaired()
                   || static_cast<BotMain*>(bot)->getIsEngineerTarget());
        static_cast<BotMain*>(bot)->setShouldWaitForRepairs(wait);
        if (!wait && m_float2C[bot->getBotId()] > 0.0f && physical
            && physical->getHealth() - m_float2C[bot->getBotId()] > 0.25f)
        {
            bot->getCommunicationAgent()->enqueueRadioMessage(8, 6, 3, 1.0f, -1);
            m_float2C[bot->getBotId()] = 0.0f;
        }

        const std::list<AIRadioMessageInfo>& stops =
            bot->getCommunicationAgent()->getReceivedMessages(static_cast<AIRadioMessageType>(11));
        bool stopsPending = false;
        if (wait)
        {
            const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            if (m_bool28[bot->getBotId()] == true || m_double24[bot->getBotId()] == 0.0)
            {
                m_double24[bot->getBotId()] = now;
                m_float2C[bot->getBotId()] = physical->getHealth();
            }
            if (now - m_double24[bot->getBotId()] > 3.0)
                static_cast<BotMain*>(bot)->setIsRepaired(false);
            if (stops.size() != 0)
                stopsPending = true;
            m_bool28[bot->getBotId()] = false;
            keep = true;
            urgency = elapsed * 60.0f;
        }
        else
        {
            answerRideRequests(bot, equipmentHandle, equipment, generator, moving, elapsed, urgency,
                               target, keep);
        }

        if (stops.size() != 0
            && bot->getCommunicationAgent()->handleMessages(static_cast<AIRadioMessageType>(11))
            && target != 255)
        {
            for (std::list<AIRadioMessageInfo>::const_iterator it = stops.begin(); it != stops.end();
                 ++it)
            {
                IAIPlayer* const passenger =
                    static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                        ->getPlayerFromId(it->m_senderPlayerId);
                if (!passenger)
                    continue;
                IAIObject* const vehicle = passenger->getCurrentVehicle();
                if (!vehicle)
                    continue;
                Information* const passengerInformation =
                    informationManager->getInformation(vehicle->getHandle(bot->getSide()));
                if (!passengerInformation)
                    continue;
                Information* const ownPrimary =
                    informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
                if (informationManager->getInformation(passengerInformation->getPrimaryObject(bot->getSide()))
                    != ownPrimary)
                    continue;
                if (!stopsPending)
                {
                    bot->getCommunicationAgent()->enqueueRadioMessage(6, 7, 3, 1.0f, -1);
                    keep = false;
                }
                else
                {
                    bot->getCommunicationAgent()->enqueueRadioMessage(7, 7, 3, 1.0f, -1);
                }
            }
        }
    }
    if (cleared || !keep)
    {
        static_cast<BotMain*>(bot)->setIsRepaired(false);
        static_cast<BotMain*>(bot)->setShouldWaitForRepairs(false);
        m_double24[bot->getBotId()] = 0.0;
        m_bool28[bot->getBotId()] = false;
        m_float2C[bot->getBotId()] = 0.0f;
        urgency = elapsed;
    }
    m_urgency[bot->getBotId()] = urgency;
    IdlePlanGenerator* const idle = static_cast<IdlePlanGenerator*>(generator);
    if (idle->getIdleType() == 1)
    {
        static_cast<BBPIdle2d*>(idle)->setTarget(bot, target);
        return urgency;
    }
    if (idle->getIdleType() == 2)
    {
        static_cast<BBPIdle3d*>(idle)->setTarget(50.0f, bot, target);
        return urgency;
    }
    if (idle->getIdleType() == 3)
        static_cast<BBPIdleInfantery*>(idle)->offerPlan(bot);
    return urgency;
}

// The generator must be a fire generator; it learns its owner after the arrays are set.
BBFire::BBFire(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
               float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    m_int24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
    m_curve = new SampledFunction(fireCurve, 0.0f, 1.0f, 0.03f);
}

// PE 101029E0.
BBFire::~BBFire()
{
    delete[] m_targets;
    delete[] m_int24;
    if (m_curve)
        delete m_curve;
}

namespace {

// PE 10070AA0: the difference b - a.
Vec3 vectorFrom(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = b.x - a.x;
    out.y = b.y - a.y;
    out.z = b.z - a.z;
    return out;
}

// PE 1006EE90.
float vectorLength(const Vec3& v)
{
    return static_cast<float>(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

// PE 1006EE50.
float vectorDot(const Vec3& a, const Vec3& b)
{
    return b.z * a.z + b.y * a.y + b.x * a.x;
}

// PE 1006ED90: clamps to [low, high]; an unordered value yields low.
float clampValue(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// Seen at PE 102A9C88: a writable float that only BBFire reads, scaling the occupant strength of
// targets that are not soldiers. [INFERRED] name.
float s_nonSoldierOccupantFactor = 2.0f;
// PE 102C05C5: a zero-initialised byte that nothing in the image writes. When set, BBFire skips
// anti-aircraft type 2 shots at targets above the equipment. [UNRESOLVED]
bool s_unresolvedAntiAirCheck = false;

// PE 1009B6F0: the point transformed by the matrix, rows as basis vectors plus the translation.
void transformByMatrix(const Mat4& matrix, const Vec3& in, Vec3& out)
{
    const float* const m = matrix.m;
    const float z = in.x * m[2] + in.z * m[10] + in.y * m[6] + m[14];
    const float y = in.x * m[1] + in.z * m[9] + in.y * m[5] + m[13];
    const float x = in.z * m[8] + in.y * m[4] + in.x * m[0] + m[12];
    out.x = x;
    out.y = y;
    out.z = z;
}

// Whether the object, or the primary object it belongs to, or any of that primary's secondary
// objects, is occupied. A secondary object whose primary is unknown is dereferenced as shipped.
// PE 101989B0; [INFERRED] name.
bool isOccupiedObject(unsigned int handle, int side)
{
    Information* information = lookupInformation(handle);
    if (!information)
        return false;
    if (!information->isPrimary())
        information = lookupInformation(information->getPrimaryObject(side));
    if (!information->getUnit())
        return false;
    if (information->getUnit()->isOccupied())
        return true;
    int iterator;
    for (Information* secondary =
             lookupInformation(information->getFirstSecondaryObject(iterator, side));
         secondary;
         secondary = lookupInformation(information->getSecondaryObject(iterator, side)))
    {
        if (secondary->getUnit() && secondary->getUnit()->isOccupied())
            return true;
    }
    return false;
}

} // anonymous namespace

// PE 101028F0.
double* BBFire::getTargetTime(unsigned int handle)
{
    const std::map<unsigned int, double*>::iterator found = m_map30.find(handle);
    if (found != m_map30.end())
        return found->second;
    double* const time = checkPointer(new double);
    *time = -10000.0;
    m_map30.insert(std::make_pair(handle, time));
    return time;
}

namespace {

// PE 102C05C4: a zero-initialised byte nothing in the image writes. When clear, a bot with C4
// placed does not choose a type 4 weapon. [UNRESOLVED]
bool s_unresolvedAllowSecondC4 = false;

// PE 100C1A20: the smaller value; the second on equality or when unordered.
float minValue(float a, float b)
{
    return a < b ? a : b;
}

// PE 1006EE70.
float vectorLengthSquared(const Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// Whether the bot's controls currently move it: throttle, yaw or sprint mapped and non-zero.
// Retail reads the player input inline.
bool isMoving(io::PlayerInput* input)
{
    if ((input->mappingFlags & 8) && input->inputs[io::PIThrottle] != 0.0f)
        return true;
    if ((input->mappingFlags & 1) && input->inputs[io::PIYaw] != 0.0f)
        return true;
    return (input->mappingFlags & 0x2000) && input->inputs[io::PISprint] != 0.0f;
}

// The kit's preferred slot: weapon 0 for most kits, weapon 5 for kit 5. Retail inlines the test
// twice in BBFire::chooseWeapon, calling getSpawnKit again for the second clause.
bool isKitWeapon(Bot* bot, unsigned int index)
{
    return (bot->getSpawnKit() != 5 && index == 0) || (bot->getSpawnKit() == 5 && index == 5);
}

} // anonymous namespace

// PE 101015F0. Scores each usable weapon against the target's strength type and returns the best
// index. A knife at close range against a soldier returns at once when the bot's path to the target
// is clear. Splash weapons against soldiers may switch the bot to a static or splash attack, which
// raises their score a hundredfold. Weapons outside their range bands, empty, or waiting on a long
// reload score less.
int BBFire::chooseWeapon(Bot* bot, unsigned int target, float distance)
{
    Information* const equipment = lookupInformation(bot->getCurrentEquipment());
    const bool soldier = (equipment->getObjectTypeFlags() >> 21) & 1;
    IPIArmament* const armament = equipment->getArmament();
    static_cast<BotMain*>(bot)->setUseStaticAttack(false);
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    float bestScore = 0.0f;
    int bestIndex = -1;
    Information* const targetInformation = lookupInformation(target);
    if (!targetInformation)
        return -1;
    const bool targetSoldier = (targetInformation->getObjectTypeFlags() >> 21) & 1;
    IPIPhysical* const targetPhysical = targetInformation->getPhysical();
    IPIMobile* const targetMobile = targetInformation->getMobile();
    if (!targetPhysical)
        return -1;

    bool holdingKitWeapon = false;
    bool kitWeaponEmpty = false;
    IPIArmament* const ownArmament = equipment->getArmament();
    if (ownArmament && ownArmament->getActiveWeapon() != -1)
    {
        const int active = ownArmament->getActiveWeapon();
        WeaponInfo* const activeWeapon =
            WeaponInfoManager::m_instance->getInformation(ownArmament->getWeapon(active));
        if (!activeWeapon)
            return -1;
        IWeapon* const engineWeapon = activeWeapon->getWeapon();
        if (soldier && isKitWeapon(bot, static_cast<unsigned int>(active)))
            holdingKitWeapon = true;
        kitWeaponEmpty = engineWeapon->getAmmoInMag(0) == 0 && !holdingKitWeapon;
    }

    IPIArmament* const targetArmament = targetInformation->getArmament();
    WeaponInfo* targetWeapon = 0;
    bool targetWeaponEmpty = false;
    if (targetArmament && targetArmament->getActiveWeapon() != -1)
    {
        targetWeapon = WeaponInfoManager::m_instance->getInformation(
            targetArmament->getWeapon(targetArmament->getActiveWeapon()));
        if (targetWeapon)
            targetWeaponEmpty = targetWeapon->getWeapon()->getAmmoInMag(0) == 0;
    }

    // Whether the target is engaging this bot: another bot firing at it, or a human shooter
    // with a shot on record.
    bool targetEngaging = false;
    IBotManager* const botManager = IBotManager::instance;
    Bot* const targetBot = botManager->getBotFromPlayerId(targetInformation->getPlayerId());
    if (targetBot)
    {
        Information* const firingTarget = informationManager->getInformation(targetBot->getFiringTarget());
        if (firingTarget)
        {
            const int playerId = bot->getPlayerId();
            if (firingTarget->getPlayerId() == playerId)
                targetEngaging = true;
        }
    }
    else if (targetWeapon)
    {
        double shotTime;
        static_cast<BotMain*>(bot)->isAttacking(target, shotTime);
        targetEngaging = true;
        if (!(now - shotTime < 9995.0))
            targetEngaging = false;
    }
    bool soldierDuel = targetSoldier && soldier;
    const bool targetHuman = !IBotManager::instance->getBotFromPlayerId(targetInformation->getPlayerId());
    SensedObject* const sense = static_cast<SensedObject*>(bot->getSensedObjectFromHandle(target));

    std::vector<unsigned int>& weapons = armament->getWeapons();
    for (unsigned int index = 0; index < weapons.size(); ++index)
    {
        if (!WeaponInfoManager::m_instance->getInformation(weapons[index]))
            continue;
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[index]);
        const int type = weapon->getWeaponType();
        if (type != 0 && type != 4 && type != 3)
            continue;
        bool kitWeapon = false;
        bool knifeAttack = false;
        const bool splashAgainstSoldier = !soldier && targetSoldier
                                          && weapon->getWeaponGuidanceType() == 0
                                          && weapon->getExplosionRadius() != 0.0f;
        if (!weapon->getFiresThroughTransparent() && !(sense->m_senseFlags & 0x48))
            continue;
        if (soldier && isKitWeapon(bot, index))
            kitWeapon = true;
        if (targetInformation->getArmament() && targetInformation->getArmament()->getActiveWeapon() == 0)
            kitWeapon = false;
        if (type == 4)
        {
            if (static_cast<BotMain*>(bot)->isC4Placed() && !s_unresolvedAllowSecondC4)
                continue;
            if (targetMobile
                && std::fabs(vectorDot(equipment->getFaceing(), targetMobile->getSpeed())) > 4.0f)
                continue;
        }
        if (kitWeapon && soldier && targetInformation->getMobile() && equipment->getMobile())
        {
            const IPIMobile::ValidPos& valid = targetInformation->getMobile()->getLastValidPosition(
                equipment->getMobile()->getVehicleNumber());
            if (vectorLengthSquared(vectorFrom(valid.position, targetInformation->getPosition())) > 1.0f
                || bot->getBotNavigation()->isInsideObstacle(targetInformation->getPosition()))
            {
                soldierDuel = false;
                continue;
            }
        }

        bool close = holdingKitWeapon && distance < 10.0f;
        const float stamina = targetMobile ? targetMobile->getSprintStamina() : 0.0f;
        if (stamina > 0.5f || (close && stamina > 0.0f))
        {
            if (kitWeapon && (targetWeaponEmpty || close) && (kitWeaponEmpty || holdingKitWeapon)
                && soldierDuel && targetHuman && distance < 20.0f && (targetEngaging || holdingKitWeapon))
                knifeAttack = true;
        }

        int ammo = weapon->getAmmo();
        if (ammo == -1)
            ammo = 0x10000;
        bool boosted = false;
        if (!(ammo > 0) && type != 3 && !knifeAttack)
            continue;
        float score = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
        if (score == 0.0f)
            continue;
        if ((soldier && targetSoldier && distance < 3.0f && kitWeapon) || knifeAttack)
        {
            const IPIMobile::ValidPos& valid = targetInformation->getMobile()->getLastValidPosition(
                equipment->getMobile()->getVehicleNumber());
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            Vec3 hit;
            hit.x = hit.y = hit.z = 0.0f;
            unsigned int hitHandle;
            const int equipmentType = equipment->getUnit()->getEquipmentType();
            if (pathfinding->trace(bot->getBotNavigation(), false, valid.position, hit, hitHandle,
                                   equipmentType, -1, 0.0f, 0, 0))
                return static_cast<int>(index);
        }

        if ((soldier || splashAgainstSoldier) && ((targetInformation->getObjectTypeFlags() >> 21) & 1)
            && weapon->getWeaponGuidanceType() == 0 && weapon->getExplosionRadius() != 0.0f)
        {
            // A splash weapon against a soldier: hold off near friendly ground the bot is moving
            // on, then switch to a static attack when the target sits within range and has been
            // seen recently enough.
            bool moving = static_cast<BotMain*>(bot)->getIsGoingForFlag();
            bool nearObjective = false;
            if (!splashAgainstSoldier)
            {
                AIStrategicObject* const objective = AIStrategicObjectManager::getInstance()->getClosestObject(
                    equipment->getPosition(), bot->getSide(), false);
                if (objective)
                {
                    IPIMobile* const ownMobile = equipment->getMobile();
                    const Vec3 orderPosition =
                        objective->getOrderPos(ownMobile->getVehicleNumber(), bot->getSide());
                    const float radius = objective->m_size[bot->getSide()];
                    const float radiusSquared = radius * radius;
                    Vec3 centre;
                    centre.x = centre.y = centre.z = 0.0f;
                    vec2ToVec3XZ(objective->getPosition(bot->getSide()), centre);
                    centre.y = orderPosition.y;
                    Vec3 offset;
                    offset.x = centre.x - orderPosition.x;
                    offset.y = 0.0f;
                    offset.z = centre.z - orderPosition.z;
                    if (vectorLengthSquared(offset) < radiusSquared && objective->getStatus(bot->getSide()) != 0
                        && vectorLengthSquared(vectorFrom(orderPosition, equipment->getPosition())) < 1600.0f
                        && isMoving(bot->playerInput()))
                        moving = true;
                    nearObjective = vectorLengthSquared(vectorFrom(targetInformation->getPosition(), centre))
                                    < radiusSquared;
                }
            }
            if (!((moving || nearObjective) && !splashAgainstSoldier))
            {
                const float height = targetInformation->getPosition().y - equipment->getPosition().y;
                float minRange = weapon->getMinRange();
                if (!splashAgainstSoldier)
                {
                    minRange = 5.0f;
                    if (!(height > 3.0f))
                        minRange = 10.0f;
                }
                float maxRange = weapon->getMaxRange();
                if (height > 5.0f && !weapon->getIsThrown())
                    maxRange = height * 1.25f + maxRange;
                if (static_cast<BotMain*>(bot)->getBotWantsToSimulateArtillery())
                {
                    minRange = minRange - 1.0f;
                    maxRange = maxRange + 1.0f;
                }
                bool unseenOk;
                if (bot->getSpawnKit() == 1 || weapon->getIsThrown())
                    unseenOk = false;
                else
                    unseenOk = height > 30.0f;
                unseenOk = unseenOk || splashAgainstSoldier;
                const double window = splashAgainstSoldier ? 5.0 : 10.0;
                int grenade = 1;
                if (!weapon->getIsThrown() && weapon->getBurst())
                    grenade = 3;
                const double sinceSeen = now - sense->m_lastUpdate;
                const double sinceGrenade = now - bot->getTimeOfGrenadeThrown(grenade - 1);
                const bool staticFailed =
                    now - static_cast<BotMain*>(bot)->getTimeOfLastFailedStaticAttack() < 10.0;
                const bool splashFailed =
                    now - static_cast<BotMain*>(bot)->getTimeOfLastFailedSplashDamageAttack() < 5.0;
                const bool inRange = !(distance < minRange) && distance <= maxRange;
                const bool failed = splashAgainstSoldier ? splashFailed : staticFailed;
                if (inRange && !failed && (sense->m_senseFlags & 2)
                    && ((sense->m_senseFlags & 8) || unseenOk) && sinceSeen < window
                    && (sinceGrenade > 10.0 || splashAgainstSoldier))
                {
                    score = score * 100.0f;
                    boosted = true;
                    static_cast<BotMain*>(bot)->setUseStaticAttack(true);
                }
            }
        }

        IWeapon* const engineWeapon = weapon->getWeapon();
        if (engineWeapon->getAmmoInMag(0) == 0 && weapon->getReloadTime() > 1.9f && weapon->hasAutoReload())
        {
            static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();   // discarded
            if (engineWeapon->getWeaponGuidingSystem() == 0 || !engineWeapon->getProjectile())
                score = 0.1f;
        }
        const float weaponMaxRange = weapon->getMaxRange();
        const float weaponMinRange = weapon->getMinRange();
        if (!boosted)
        {
            if (distance < weaponMinRange)
                score = (0.100000024f - minValue(1.0f, (weaponMinRange - distance) * 0.2f) * 0.09f) * score;
            else if (distance > weaponMaxRange)
                score = (1.0f - minValue(1.0f, (distance - weaponMaxRange) * 0.2f) * 0.9f) * score;
        }
        if (engineWeapon->getAmmoInMag(0) == 0)
            score = score * 0.33f;
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = static_cast<int>(index);
        }
    }
    return bestIndex;
}

// PE 10102A60. The unflagged pass keeps the stored urgency while the target is still known.
// The flagged pass scores every sensed, living enemy the equipment can reach: the best weapon's
// strength against it, the occupants' battle strengths, distance, freshness, an interception
// estimate for targets out of range and the bot's standing orders, shaped by the fire curve. The
// best target and weapon are kept per bot; the previous target survives unless the best scores
// more than 1.2 times as high.
float BBFire::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    if (!flag)
    {
        if (m_generator->hasPlan(bot))
        {
            if (!lookupInformation(m_targets[bot->getBotId()]))
            {
                m_targetChanged[bot->getBotId()] = true;
                m_generator->reset(bot);
                m_urgency[bot->getBotId()] = 0.0f;
                return 0.0f;
            }
            m_targetChanged[bot->getBotId()] = false;
            bot->addToFireInclination(m_urgency[bot->getBotId()]);
            return m_urgency[bot->getBotId()];
        }
        m_targetChanged[bot->getBotId()] = false;
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    float areaFactor = 0.5f;
    const float* const armourClassValues =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getArmourClassValues();
    if (now - m_double28 > 60.0)
    {
        for (std::map<unsigned int, double*>::iterator it = m_map30.begin(); it != m_map30.end();)
        {
            if (!lookupInformation(it->first))
            {
                delete it->second;
                m_map30.erase(it++);
            }
            else
            {
                ++it;
            }
        }
        m_double28 = now;
    }

    const int side = bot->getSide();
    Information* const equipment = lookupInformation(bot->getCurrentEquipment());
    const Vec3& position = equipment->getPosition();
    const unsigned int equipmentFlags = equipment->getObjectTypeFlags();
    if (!((equipmentFlags >> 7) & 1) && !((equipmentFlags >> 4) & 1) && ((equipmentFlags >> 21) & 1)
        && equipment->isPrimary() && equipment->getSoldierPose() == 4)
    {
        m_targetChanged[bot->getBotId()] = true;
        m_generator->reset(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    IPIUnit* const unit = equipment->getUnit();
    const int equipmentType = unit->getEquipmentType();   // read before the null check, as shipped
    IPIPhysical* const physical = equipment->getPhysical();
    IPIArmament* const armament = equipment->getArmament();
    IPIControlInfo* const controlInfo = equipment->getControlInfo();
    if (!unit || !physical || !armament || !controlInfo)
    {
        m_targetChanged[bot->getBotId()] = true;
        m_generator->reset(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }
    IPIMobile* const mobile = equipment->getMobile();
    const bool exposedSoldier = unit->hasExposedSoldier();
    const int strengthType = physical->getStrengthType();
    int soldierStrengthType;
    if (!exposedSoldier)
    {
        soldierStrengthType = 0;
    }
    else
    {
        soldierStrengthType = lookupInformation(bot->getPlayer()->getSoldier()->getInformation(side))
                                  ->getPhysical()->getStrengthType();
    }

    unsigned int ordersMatched = 0;
    WeaponInfo* bestWeapon = 0;
    float bestUrgency = 0.0f;
    float previousTargetUrgency = 0.0f;
    const unsigned int previousTarget = m_targets[bot->getBotId()];
    bot->getBotId();   // the shipped body makes this call and discards the result
    bool keepPrevious = false;
    ISAI* const sai = IAIMain::m_instance->getSAI(side);
    AIStrategicObject* const orderedObject = AIStrategicObjectManager::getInstance()->getStrategicObject(
        sai->getBotOrderedPos(bot->getBotId()));
    if (orderedObject && !static_cast<AIStrategicArea*>(orderedObject)->isInsideArea(position))
        areaFactor = 0.85f;
    equipment->getTransformation();   // the shipped body makes this call and discards the result
    float bestDistance = 0.0f;
    m_int24[bot->getBotId()] = static_cast<unsigned int>(-1);

    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end();
         ++it)
    {
        SensedObject* const sense = it->second;
        if (!(sense->m_senseFlags & 2) || !(sense->m_senseFlags & 4))
            continue;
        Information* const target = lookupInformation(sense->m_handle);
        if (!target || target->getSide() == side || target->getSide() == 0)
            continue;
        IPIUnit* const targetUnit = target->getUnit();
        IPIPhysical* const targetPhysical = target->getPhysical();
        if (!targetPhysical)
            continue;
        const unsigned int targetFlags = target->getObjectTypeFlags();
        if (((targetFlags >> 17) & 1) || ((targetFlags >> 20) & 1) || ((targetFlags >> 23) & 1))
            continue;
        const float health = targetPhysical->getHealth();
        if (health < 0.0f || health == 0.0f)
            continue;
        if ((sense->m_senseFlags & 0x20) && sense->m_int4C < 0)
            continue;

        if (now - *getTargetTime(sense->m_handle) < 20.0)
        {
            float speeds = mobile ? mobile->getSpeedMagnitude() : 0.0f;
            if (target->getMobile())
                speeds = target->getMobile()->getSpeedMagnitude() + speeds;
            if (speeds < 2.0f)
                continue;
            static_cast<BotMain*>(bot)->setTime276(now);
        }

        double attackTime;
        float attackFactor;
        if (!static_cast<BotMain*>(bot)->isAttacking(sense->m_handle, attackTime))
        {
            attackFactor = 1.0f;
        }
        else
        {
            attackFactor = 1.5f - clampValue(static_cast<float>((now - attackTime) * 0.033333335f),
                                             0.0f, 1.0f) * 0.5f;
            IBotManager* const botManager = IBotManager::instance;
            if (!botManager->getBotFromPlayerId(target->getPlayerId()) && ((target->getObjectTypeFlags() >> 21) & 1))
                attackFactor = attackFactor * 3.0f;
        }

        IPIMobile* const targetMobile = target->getMobile();
        Vec3 targetPosition;
        targetPosition.x = targetPosition.y = targetPosition.z = 0.0f;
        if (targetMobile)
            targetMobile->getPositionEstimate(targetPosition);
        else
            targetPosition = target->getPosition();
        Vec3 direction = vectorFrom(position, targetPosition);
        float distance = vectorLength(direction);
        if (distance > 1e-4f)
        {
            const float inverse = 1.0f / distance;   // PE 1009B6B0
            direction.x *= inverse;
            direction.y *= inverse;
            direction.z *= inverse;
        }
        distance = std::max(distance, 1.0f);

        float freshness;
        if (sense->m_senseFlags & 8)
        {
            if (!mobile || !targetMobile)
                continue;
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            const IPIMobile::ValidPos& valid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
            if (valid.polygon == static_cast<unsigned int>(-1))
                continue;
            if (targetUnit->getEquipmentType() != equipmentType)
            {
                unsigned char material;
                pathfinding->getMaterial(mobile->getVehicleNumber(), valid.quadTree, valid.polygon,
                                         material);
                if (!static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                         ->isValidMaterial(equipmentType, material))
                    continue;
            }
            freshness = 1.0f / (static_cast<float>(now - sense->m_firstSensed) * 0.05f + 1.5f);
        }
        else
        {
            freshness = 1.0f;
            if (!mobile)
            {
                Vec3 cameraBase;
                cameraBase.x = cameraBase.y = cameraBase.z = 0.0f;
                controlInfo->getCameraBasePos(cameraBase);
                Vec3 sensedPoint;
                sensedPoint.x = sensedPoint.y = sensedPoint.z = 0.0f;
                transformByMatrix(target->getTransformation(), sense->m_senseNormal, sensedPoint);
                const unsigned int flags = equipment->getObjectTypeFlags();
                if ((!((flags >> 4) & 1) || ((flags >> 5) & 1))
                    && !controlInfo->validateCameraDirectionByPos(sensedPoint))
                    continue;
            }
        }

        // The best weapon against the target's strength type, preferring the active one.
        float maxRange = 0.0f;
        int bestWeaponIndex = -1;
        float bestWeaponScore = 0.0f;
        float scores[8];
        std::vector<unsigned int>& weapons = armament->getWeapons();
        unsigned int index = 0;
        for (; index < weapons.size(); ++index)
        {
            if (!WeaponInfoManager::m_instance->getInformation(weapons[index]))
            {
                scores[index] = 0.0f;
                continue;
            }
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[index]);
            const int type = weapon->getWeaponType();
            if (type != 0 && type != 4 && type != 3)
            {
                scores[index] = 0.0f;
                continue;
            }
            if (type == 4 && targetMobile
                && std::fabs(vectorDot(equipment->getFaceing(), targetMobile->getSpeed())) > 4.0f)
                continue;   // the score slot is left unwritten, as shipped
            const int ammo = weapon->getAmmo();
            float& score = scores[index];
            if (ammo == -1 || ammo > 0)
                score = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
            else
                score = 0.0f;
            if (index == static_cast<unsigned int>(armament->getActiveWeapon()))
                score = score * 1.03f;
            if (score > bestWeaponScore)
            {
                bestWeaponIndex = static_cast<int>(index);
                bestWeaponScore = score;
                bestWeapon = weapon;
            }
            if (weapon->getMaxRange() > maxRange)
                maxRange = weapon->getMaxRange();
        }
        if (bestWeaponScore == 0.0f)
            continue;
        for (; index < 8; ++index)
            scores[index] = 0.0f;

        Vec3 targetSpeed;
        targetSpeed.x = targetSpeed.y = targetSpeed.z = 0.0f;
        if (targetMobile)
            targetSpeed = targetMobile->getSpeed();
        const float targetSpeedMagnitude = vectorLength(targetSpeed);
        float placementFactor = areaFactor;
        if (mobile && !((equipment->getObjectTypeFlags() >> 4) & 1) && orderedObject
            && !static_cast<AIStrategicArea*>(orderedObject)->isInsideArea(targetPosition))
            placementFactor = areaFactor * 0.85f;

        float reachFactor;
        bool generic = true;
        if ((target->getObjectTypeFlags() >> 4) & 1)
        {
            const int antiAircraft = armament->isAntiAircraft();
            if (antiAircraft == 0)
            {
                if (target->getAltitude() > 5.0f)
                    continue;
                if (maxRange * 0.5f < distance || targetSpeedMagnitude > 5.0f)
                {
                    reachFactor = 0.0f;
                    generic = false;
                }
            }
            else
            {
                if (s_unresolvedAntiAirCheck && antiAircraft == 2)
                {
                    const float* const m = equipment->getTransformation().m;
                    if (direction.z * m[6] + direction.y * m[5] + direction.x * m[4] > 0.5f)
                        continue;
                }
                if (maxRange * 0.9f > distance)
                {
                    reachFactor = 1.0f;
                    generic = false;
                }
            }
        }
        if (generic)
        {
            if (!targetMobile || !(distance > maxRange))
            {
                placementFactor = 0.5f;
                reachFactor = 1.0f;
            }
            else
            {
                // An interception estimate: how fast the target opens the range sideways and
                // along the line of sight, compared with the equipment's own speed.
                float ownMaxSpeed;
                Vec3 ownSpeed;
                if (mobile)
                {
                    ownMaxSpeed = mobile->getMaxSpeed();
                    ownSpeed = mobile->getSpeed();
                }
                else
                {
                    ownMaxSpeed = 0.0f;
                    ownSpeed.x = ownSpeed.y = ownSpeed.z = 0.0f;
                }
                if (!(targetMobile->getMaxSpeed() - ownMaxSpeed > 5.0f))
                {
                    reachFactor = 0.25f;
                }
                else
                {
                    Vec3 lateral;
                    lateral.x = 0.0f;
                    lateral.y = 1.0f;
                    lateral.z = 0.0f;
                    const float lx = lateral.y * direction.z - lateral.z * direction.y;
                    const float ly = lateral.z * direction.x - lateral.x * direction.z;
                    const float lz = lateral.x * direction.y - lateral.y * direction.x;
                    const Vec3& speed = targetMobile->getSpeed();
                    const float ownLateral = lx * ownSpeed.x + lz * ownSpeed.z + ly * ownSpeed.y;
                    const float targetLateral = lx * speed.x + lz * speed.z + ly * speed.y;
                    const float targetClosing =
                        direction.x * speed.x + direction.z * speed.z + direction.y * speed.y;
                    const float ownClosing =
                        ownSpeed.x * direction.x + ownSpeed.z * direction.z + ownSpeed.y * direction.y;
                    float closing = targetClosing - ownClosing;
                    if (closing < 0.0f)
                        closing = 0.0f;
                    float sideways = targetLateral - ownLateral;
                    if (sideways < 0.0f)
                        sideways = 0.0f;
                    reachFactor = 0.25f / ((sideways + closing) * 2.0f + vectorLength(speed) * 0.5f + 1.0f);
                }
            }
        }

        if (distance > maxRange)
        {
            if (!targetMobile || !mobile)
                continue;
            const IPIMobile::ValidPos& valid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
            if (valid.polygon == static_cast<unsigned int>(-1))
                continue;
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            if (targetUnit->getEquipmentType() != equipmentType)
            {
                unsigned char material;
                pathfinding->getMaterial(mobile->getVehicleNumber(), valid.quadTree, valid.polygon,
                                         material);
                if (!static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                         ->isValidMaterial(equipmentType, material))
                    continue;
            }
            const float gap = distance - maxRange;
            if (mobile->getMaxSpeed() * 5.0f < gap)
                continue;
        }
        if (!target->getUnit())
            continue;
        if (!isOccupiedObject(sense->m_handle, side))
            continue;

        // The occupants' strength, summed over the target and its secondary objects.
        float occupants = 0.0f;
        float strength = 0.0f;
        int iterator = target->getFirstSecondaryObjectIterator();
        const int targetStrengthType = target->getPhysical()->getStrengthType();
        unsigned int handle = sense->m_handle;
        Information* current = target;
        do
        {
            IPIUnit* occupantUnit = current->getUnit();
            const unsigned int occupied = current->getUnit()->getOccupiedObject(side);
            if (occupied != static_cast<unsigned int>(-1))
            {
                Information* const occupiedInformation = lookupInformation(occupied);
                if (occupiedInformation)
                    occupantUnit = occupiedInformation->getUnit();
            }
            occupants = occupants + occupantUnit->getBattleStrengths()[strengthType];
            float orderFactor = 1.0f;
            if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
                && ordersMatched < bot->getOrderList(m_behaviourId)->size())
            {
                std::list<unsigned int>* const orders = bot->getOrderList(m_behaviourId);
                if (std::find(orders->begin(), orders->end(), handle) != orders->end())
                {
                    ++ordersMatched;
                    orderFactor = bot->getMemory()->m_float8;
                }
            }
            float occupantStrength;
            if (!exposedSoldier)
                occupantStrength = occupantUnit->getBattleStrengths()[strengthType];
            else
                occupantStrength = std::max(occupantUnit->getBattleStrengths()[strengthType],
                                            occupantUnit->getBattleStrengths()[soldierStrengthType]);
            if (!((target->getObjectTypeFlags() >> 21) & 1))
                occupantStrength = occupantStrength * s_nonSoldierOccupantFactor;
            strength = (unit->getBattleStrengths()[targetStrengthType] + current->getStrength()
                        + occupantStrength)
                           * orderFactor * armourClassValues[targetStrengthType] * placementFactor
                       + strength;
            handle = target->getSecondaryObject(iterator, side);
            current = lookupInformation(handle);
        } while (current);

        const float weaponMaxRange = bestWeapon->getMaxRange();
        const float weaponMinRange = bestWeapon->getMinRange();
        const float draw = AIRandom::m_instance->nextFloat();
        const float value = std::max(0.0f, 1.0f - distance / (maxRange * 1.2f)) * strength * reachFactor
                            * placementFactor * bestWeaponScore * freshness * attackFactor;
        float urgency = m_curve->sample(draw, true) * value
                        * (((weaponMaxRange - weaponMinRange) * 0.1f + weaponMinRange) / distance);
        if (static_cast<float>(static_cast<BotMain*>(bot)->getInt40()) >= occupants)
            urgency = urgency * 0.33f;
        if (!target->getUnit())
            continue;
        if (previousTarget == it->first)
        {
            keepPrevious = true;
            previousTargetUrgency = urgency;
        }
        if (urgency > bestUrgency)
        {
            bestUrgency = urgency;
            m_targets[bot->getBotId()] = it->first;
            m_int24[bot->getBotId()] = static_cast<unsigned int>(bestWeaponIndex);
            bestDistance = distance;
        }
    }

    if (!(bestUrgency > 0.0f))
    {
        m_targetChanged[bot->getBotId()] = !keepPrevious;
        m_generator->reset(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        return 0.0f;
    }

    bool chooseForBest = true;
    if (keepPrevious && previousTargetUrgency * 1.2f > bestUrgency)
    {
        if (m_targets[bot->getBotId()] != previousTarget)
        {
            m_targets[bot->getBotId()] = previousTarget;
            Information* const previous = informationManager->getInformation(previousTarget);
            const float distance = vectorLength(vectorFrom(equipment->getPosition(), previous->getPosition()));
            const int weapon = chooseWeapon(bot, previousTarget, distance);
            if (weapon != -1)
                m_int24[bot->getBotId()] = static_cast<unsigned int>(weapon);
            chooseForBest = false;
        }
    }
    else
    {
        bot->getBotId();   // the shipped body makes this call and discards the result
        keepPrevious = false;
    }
    if (chooseForBest)
    {
        const int weapon = chooseWeapon(bot, m_targets[bot->getBotId()], bestDistance);
        if (weapon != -1)
            m_int24[bot->getBotId()] = static_cast<unsigned int>(weapon);
    }

    fireAtTarget(bot, static_cast<int>(m_int24[bot->getBotId()]), false, false);
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
        && bot->getOrderList(m_behaviourId)->size() == 0)
        bestUrgency = bestUrgency * bot->getMemory()->m_float8;
    m_targetChanged[bot->getBotId()] = !keepPrevious;
    m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(bestUrgency + bestUrgency) * elapsed;
    bot->addToFireInclination(m_urgency[bot->getBotId()]);
    return m_urgency[bot->getBotId()];
}

// PE 10100C90. The bot's firing target becomes the designator.
void BBFire::fireAtPosition(int designator, const Vec3& position, Bot* bot, int weapon)
{
    bot->setFiringTarget(static_cast<unsigned int>(designator));
    static_cast<FirePlanGenerator*>(m_generator)->setPositionTarget(bot, designator, position,
                                                                     weapon);
}

// PE 10100C30.
void BBFire::fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB)
{
    bot->setFiringTarget(m_targets[bot->getBotId()]);
    static_cast<FirePlanGenerator*>(m_generator)->setObjectTarget(
        bot, m_targets[bot->getBotId()], weapon, flagA, flagB);
}

BBFireInfantery::BBFireInfantery(int behaviourId, int priority, PlanGenerator* generator,
                                 UrgeCurve* urgeCurve, float* weights)
    : BBFire(behaviourId, priority, generator, urgeCurve, weights)
{
    m_infanteryGenerator = generator;
}

BBFireInfantery::~BBFireInfantery()
{
}

// PE 10119BE0 jumps to the BBFire body.
void BBFireInfantery::fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB)
{
    BBFire::fireAtTarget(bot, weapon, flagA, flagB);
}

BBFire3d::BBFire3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
                   float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = new unsigned int[bots];
    m_int24 = new unsigned int[bots];
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

// PE 10115580.
BBFire3d::~BBFire3d()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFire3d::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFire3d::calculateUrgency");
}

// Built without weights, urge curve or generator.
BBFireUnarmed::BBFireUnarmed(int behaviourId, int priority)
    : BotBehaviour(0, 0, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    const int bots = configuredBotCount();
    m_int24 = checkPointer(new int[bots]);
    m_targets = checkPointer(new unsigned int[bots]);
    m_generator = 0;
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = 0;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBFireUnarmed::~BBFireUnarmed()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFireUnarmed::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireUnarmed::calculateUrgency");
}

BBFireLargeBore::BBFireLargeBore(int behaviourId, int priority, PlanGenerator* generator,
                                 UrgeCurve* urgeCurve, float* weights, bool flag)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_bool3C = flag;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    m_int24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

// PE 10110370.
BBFireLargeBore::~BBFireLargeBore()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFireLargeBore::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireLargeBore::calculateUrgency");
}

void BBFireLargeBore::fireAtTarget(Bot*, int, bool, bool, bool)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireLargeBore::fireAtTarget");
}

// The cover array is allocated twice and the first block is lost, as shipped.
BBTakeCover::BBTakeCover(int behaviourId, int priority, PlanGenerator* generator,
                         UrgeCurve* urgeCurve, float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_botCount = configuredBotCount();
    m_cover = checkPointer(new void*[m_botCount]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[m_botCount]);
    m_cover = checkPointer(new void*[m_botCount]);
    for (int i = 0; i < m_botCount; ++i)
    {
        m_targets[i] = static_cast<unsigned int>(-1);
        m_urgency[i] = 0.0f;
        m_cover[i] = 0;
    }
}

// PE 10119D50. The per-bot cover entries are released as plain blocks.
BBTakeCover::~BBTakeCover()
{
    for (int i = 0; i < m_botCount; ++i)
        ::operator delete(m_cover[i]);
    delete[] m_targets;
    delete[] m_cover;
}

void BBTakeCover::reset(Bot* bot)
{
    BotBehaviour::reset(bot);
    if (m_cover[bot->getBotId()])
        ::operator delete(m_cover[bot->getBotId()]);
    m_cover[bot->getBotId()] = 0;
}

float BBTakeCover::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTakeCover::calculateUrgency");
}

// Owns an unarmed and a large-bore behaviour with the same id and priority.
BBFireDriver::BBFireDriver(int behaviourId, int priority, PlanGenerator* generator,
                           UrgeCurve* urgeCurve, float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_unarmed = checkPointer(new BBFireUnarmed(behaviourId, priority));
    m_largeBore = checkPointer(
        new BBFireLargeBore(behaviourId, priority, generator, urgeCurve, weights, false));
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

BBFireDriver::~BBFireDriver()
{
    delete[] m_targets;
    if (m_unarmed)
        delete m_unarmed;
    if (m_largeBore)
        delete m_largeBore;
}

float BBFireDriver::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireDriver::calculateUrgency");
}

BBChange::BBChange(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_owned44 = 0;
    m_owned48 = 0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

// PE 100F7420.
BBChange::~BBChange()
{
    delete[] m_targets;
    if (m_owned44)
        delete m_owned44;
    if (m_owned48)
        delete m_owned48;
}

float BBChange::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChange::calculateUrgency");
}

BBChangeLandingCraft::BBChangeLandingCraft(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBChangeLandingCraft::~BBChangeLandingCraft()
{
    delete[] m_targets;
}

float BBChangeLandingCraft::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChangeLandingCraft::calculateUrgency");
}

BBChangeTeleport::BBChangeTeleport(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBChangeTeleport::~BBChangeTeleport()
{
    delete[] m_targets;
}

float BBChangeTeleport::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChangeTeleport::calculateUrgency");
}

BBSuicide::BBSuicide(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

BBSuicide::~BBSuicide()
{
}

float BBSuicide::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBSuicide::calculateUrgency");
}

BBCallForMedic::BBCallForMedic(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBCallForMedic::~BBCallForMedic()
{
    delete[] m_targets;
}

float BBCallForMedic::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBCallForMedic::calculateUrgency");
}

bool BBCallForMedic::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBCallForMedic::isUsable");
}

BBTakeCover3d::BBTakeCover3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBTakeCover3d::~BBTakeCover3d()
{
    delete[] m_targets;
}

float BBTakeCover3d::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTakeCover3d::calculateUrgency");
}

BBC4::BBC4(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBC4::~BBC4()
{
    delete[] m_targets;
}

float BBC4::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBC4::calculateUrgency");
}

// PE 101222E0. A living bot that is up and has C4 (weapon type 4) with ammunition left.
bool BBC4::isUsable(Information* equipment, Bot* bot)
{
    IPIArmament* const armament = equipment->getArmament();
    if (!bot->getPlayer()->getIsAlive() || bot->getManDown() || !armament)
        return false;
    const std::vector<unsigned int>& weapons = armament->getWeapons();
    for (size_t i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (weapon && weapon->getWeaponType() == 4 && weapon->getCurrentAmmo() > 0)
            return true;
    }
    return false;
}

BBRunOverSoldier::BBRunOverSoldier(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBRunOverSoldier::~BBRunOverSoldier()
{
    delete[] m_targets;
}

float BBRunOverSoldier::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRunOverSoldier::calculateUrgency");
}

// PE 10125740.
bool BBRunOverSoldier::isUsable(Information*, Bot* bot)
{
    return bot->getPlayer()->getIsAlive() && !bot->getManDown();
}

BBDefuseMines::BBDefuseMines(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_int38 = checkPointer(new int[configuredBotCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int38[i] = -1;
    }
}

BBDefuseMines::~BBDefuseMines()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_int38;
}

float BBDefuseMines::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBDefuseMines::calculateUrgency");
}

// PE 10124460. Only a living engineer (kit 4) that is up.
bool BBDefuseMines::isUsable(Information*, Bot* bot)
{
    if (!bot->getPlayer()->getIsAlive() || bot->getManDown())
        return false;
    return bot->getPlayer()->getSpawnKit() == 4;
}

BBRandom::BBRandom(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
    m_double28 = 0.0;
    m_bool24 = false;
    m_double30 = 0.0;
}

BBRandom::~BBRandom()
{
    delete[] m_targets;
}

float BBRandom::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRandom::calculateUrgency");
}

// PE 10125F90. Always false; the checks are made and discarded, as shipped.
bool BBRandom::isUsable(Information*, Bot* bot)
{
    if (bot->getPlayer()->getIsAlive())
        bot->getManDown();
    return false;
}

BBTriggerable::BBTriggerable(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
    m_int24 = 0;
}

BBTriggerable::~BBTriggerable()
{
    delete[] m_targets;
}

float BBTriggerable::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTriggerable::calculateUrgency");
}

// PE 101268F0. Always false; the checks are made and discarded, as shipped.
bool BBTriggerable::isUsable(Information*, Bot* bot)
{
    if (bot->getPlayer()->getIsAlive())
        bot->getManDown();
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
