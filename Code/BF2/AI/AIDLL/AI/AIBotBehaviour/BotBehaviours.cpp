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
#include "../AIKnowledge/ObjectOccupancy.h"
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../AIStrategicMap/BFStrategicEnvironment.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
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
    m_battle38 = checkPointer(new float[configuredBattleStrengthCount()]);
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

BBRevive::BBRevive(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new float[configuredBattleStrengthCount()]);
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

namespace {

// PE 102ACFA8: a writable double that only the equipment evaluation reads. [INFERRED] name.
double s_possessionGrace = 15.0;

// PE 1006EEC0: normalises in place and returns 0, or zeroes the vector and returns -20001 when
// it has no length.
int normaliseVector(Vec3& v)
{
    const float length = static_cast<float>(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
    if (length < 1e-6f)
    {
        v.x = v.y = v.z = 0.0f;
        return -20001;
    }
    const float inverse = 1.0f / length;
    v.x *= inverse;
    v.y *= inverse;
    v.z *= inverse;
    return 0;
}

// Whether a human player, alive and standing, sent the first message in the list.
IAIPlayer* humanRequester(const std::list<AIRadioMessageInfo>& messages)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(messages.front().m_senderPlayerId);
    if (player && !player->getIsAIPlayer() && player->getIsAlive() && !player->isManDown())
        return player;
    return 0;
}

// Whether a vehicle is kept for the bot's squad: inside the squad's assigned area while the
// squad stands near its order position, or near the area's control point. Soldiers and aircraft
// are never kept. PE 101AA350; [INFERRED] name.
bool isHeldForSquadArea(Information* candidate, int side, Bot* bot)
{
    bool held = false;
    Information* const primary = informationManager->getInformation(candidate->getPrimaryObject(side));
    const unsigned int flags = primary->getObjectTypeFlags();
    if (((flags >> 21) & 1) || ((flags >> 4) & 1) || ((flags >> 5) & 1))
        return false;
    IPIMobile* const primaryMobile = primary->getMobile();
    AIStrategicArea* const area = bot->getSquadMember()->getSquad()->getSquadStrategicAreaAssignment();
    if (!area || !area->isAreaTakeable() || !primaryMobile
        || !area->isOrderPosOutsideControlPoint(primaryMobile->getVehicleNumber(), side))
        return false;
    Vec3 squadPosition;
    squadPosition.x = squadPosition.y = squadPosition.z = 0.0f;
    if (!bot->getSquadMember()->getPosition(squadPosition))
        return false;
    Vec3 target;
    target.x = target.y = target.z = 0.0f;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment->getPrimaryObject(side) == primary->getPrimaryObject(side))
    {
        target = area->getOrderPos(primaryMobile->getVehicleNumber(), side);
        if (vectorLengthSquared(vectorFrom(target, squadPosition)) < 625.0f)
        {
            held = true;
            if (equipment->isPrimary() && equipment->getArmament()
                && IAIMain::m_instance->getSAI(side)->haveAnyOwnedCps(side))
                held = false;
        }
        return held;
    }
    float radius = 1.0f;
    bool active;
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
        ->getControlPointInfo(area->m_controlPointId, target, radius, active);
    if (vectorLengthSquared(vectorFrom(target, primary->getPosition())) < 4900.0f)
        held = true;
    else if (vectorLengthSquared(vectorFrom(squadPosition, target)) < 4900.0f)
        held = true;
    return held;
}

// The worth of a seat for getting somewhere: the vehicle's top speed, scaled by who drives it.
// PE 101AABD0; [INFERRED] name.
float seatSpeedValue(Information* candidate, int side, Bot* bot, bool requireFree, Information* current)
{
    if (isHeldForSquadArea(candidate, side, bot))
        return 0.0f;
    const int botId = bot->getBotId();
    if (candidate->isPrimary())
        return candidate->getMobile() ? candidate->getMobile()->getMaxSpeed() * 5.0f : 0.0f;
    Information* const primary = informationManager->getInformation(candidate->getPrimaryObject(side));
    IPIMobile* const primaryMobile = primary->getMobile();
    if (!primaryMobile)
        return 0.0f;
    if (primary == current && requireFree)
        return 0.0f;
    IPIUnit* const unit = primary->getUnit();
    if (!unit->isOccupied())
    {
        if (primary->getSide() == side)
        {
            const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            const double since = now - unit->getLastPlayerPossesionTime();
            if (since < 15.0)
            {
                const float fraction = static_cast<float>(since * (1.0 / 15.0));
                return (1.0f - fraction) * primaryMobile->getMaxSpeed() * 4.0f;
            }
        }
        return 0.0f;
    }
    Bot* const driver = IBotManager::instance->getBot(primary->getAIObject());
    ISAI* const sai = IAIMain::m_instance->getSAI(side);
    if (driver && sai->getBotOrderedPos(driver->getBotId()) != sai->getBotOrderedPos(botId))
        return primaryMobile->getMaxSpeed() * 2.5f;
    return primaryMobile->getMaxSpeed() * 4.0f;
}

// The fighting worth of a seat: the summed battle strengths of the seat, its vehicle and the
// occupied seats, against what the bot senses, its incoming fire, or the enemy picture of its
// side. PE 101AAE20; [INFERRED] name.
float combatValue(Bot* bot, Information* candidate, int side, const float* enemyStrengths,
                  const float* enemyCounts, bool requireFree, Information* current)
{
    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    const int kinds = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    float fireMax = 0.0f;
    bool engaged = false;
    const Vec3& position = candidate->getPosition();
    IPIArmament* armament = candidate->getArmament();
    IPIUnit* const unit = candidate->getUnit();
    IPIPhysical* const physical = candidate->getPhysical();
    IPIControlInfo* const controlInfo = candidate->getControlInfo();
    float maxRange = 50.0f;
    float minRange = 0.1f;
    bool antiAir = false;
    bool guided = false;
    if (armament)
    {
        if (armament->isAntiAircraft() == 1)
            antiAir = true;
        std::vector<unsigned int>& weapons = armament->getWeapons();
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (!weapon)
                continue;
            if (weapon->getWeaponGuidanceType())
                guided = true;
            const float range = weapon->getMaxRange();
            const float minimum = weapon->getMinRange();
            if (range > maxRange)
                maxRange = range;
            if (minimum < minRange)
                minRange = minimum;
        }
    }
    if (!(guided || antiAir) && candidate->isPrimary() && !candidate->getMobile())
    {
        // A fixed emplacement counts only within 12 m and 2 m of height of the current equipment.
        if (vectorLengthSquared(vectorFrom(current->getPosition(), position)) > 144.0f)
            return 0.0f;
        if (std::fabs(current->getPosition().y - position.y) > 2.0f)
            return 0.0f;
    }
    const bool countSeat = !(candidate == current && requireFree);
    IPIMobile* vehicleMobile;
    bool noVehicle;
    if (candidate->isPrimary())
    {
        vehicleMobile = candidate->getMobile();
        noVehicle = vehicleMobile == 0;
    }
    else
    {
        Information* const primary = informationManager->getInformation(candidate->getPrimaryObject(bot->getSide()));
        if (primary->getUnit() && current != primary && primary->getUnit()->isOccupied())
        {
            vehicleMobile = primary->getMobile();
            noVehicle = false;
        }
        else
        {
            vehicleMobile = 0;
            noVehicle = true;
        }
    }
    const int strengthType = physical->getStrengthType();
    const bool exposed = unit->hasExposedSoldier();
    int soldierStrengthType = 0;
    if (exposed)
        soldierStrengthType = informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(side))
                                  ->getPhysical()->getStrengthType();
    IAIObject* const object = candidate->getAIObject();
    IPIUnit* strengthSource = unit;
    if (countSeat && object->getUnit()->mustBorrowArmament(object))
    {
        Information* const soldier = informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(side));
        strengthSource = soldier->getUnit();
        armament = soldier->getArmament();
        maxRange = 50.0f;
        minRange = 0.1f;
        if (armament)
        {
            std::vector<unsigned int>& weapons = armament->getWeapons();
            for (unsigned int i = 0; i < weapons.size(); ++i)
            {
                WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
                if (!weapon)
                    continue;
                const float range = weapon->getMaxRange();
                const float minimum = weapon->getMinRange();
                if (range > maxRange)
                    maxRange = range;
                if (minimum < minRange)
                    minRange = minimum;
            }
        }
    }
    float strengths[32];
    for (int i = 0; i < kinds; ++i)
        strengths[i] = countSeat ? strengthSource->getBattleStrengths()[i] : 0.0f;

    Information* vehicle;
    float seatShare;
    if (candidate->isPrimary())
    {
        vehicle = candidate;
        seatShare = candidate->getArmament() ? 0.4f : 0.9f;
    }
    else
    {
        vehicle = informationManager->getInformation(candidate->getPrimaryObject(side));
        IPIUnit* const vehicleUnit = vehicle->getUnit();
        seatShare = 0.4f;
        if (!(vehicle == current && requireFree) && vehicleUnit->isOccupied())
            for (int i = 0; i < kinds; ++i)
                strengths[i] = vehicleUnit->getBattleStrengths()[i] * 0.4f + strengths[i];
    }
    int iterator;
    for (Information* seat = informationManager->getInformation(vehicle->getFirstSecondaryObject(iterator, side)); seat;
         seat = informationManager->getInformation(vehicle->getSecondaryObject(iterator, side)))
    {
        if (seat == candidate)
            continue;
        IPIUnit* const seatUnit = seat->getUnit();
        if (seat == current && requireFree)
            continue;
        if (!seatUnit->isOccupied())
            continue;
        for (int i = 0; i < kinds; ++i)
            strengths[i] = seatShare * seatUnit->getBattleStrengths()[i] + strengths[i];
    }

    if (sensed.size() != 0)
    {
        IPIUnit* const currentUnit = current->getUnit();
        bool canFire = true;
        float best = 0.0f;
        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
        {
            Information* const target = informationManager->getInformation(it->first);
            if (!target || !(it->second->m_senseFlags & 4))
                continue;
            if (!candidate->getMobile() && armament)
            {
                const Vec3& targetPosition = target->getPosition();
                const float dx = targetPosition.x - position.x;
                const float dy = targetPosition.y - position.y;
                const float dz = targetPosition.z - position.z;
                const float distance = static_cast<float>(std::sqrt(dz * dz + dy * dy + dx * dx));
                if (distance > maxRange || distance < minRange)
                    continue;
                if ((it->second->m_senseFlags & 8) && it->second->m_firstSensed + 20.0 > now)
                    continue;
            }
            IPIUnit* const targetUnit = target->getUnit();
            IPIPhysical* const targetPhysical = target->isPrimary()
                ? target->getPhysical()
                : informationManager->getInformation(target->getPrimaryObject(side))->getPhysical();
            if (!targetUnit || !targetPhysical)
                continue;
            if (!vehicleMobile)
            {
                canFire = false;
                Vec3 direction = vectorFrom(position, target->getPosition());
                if (normaliseVector(direction) == 0 && controlInfo->validateCameraDirection(direction))
                {
                    noVehicle = false;
                    canFire = true;
                }
            }
            const float own = strengths[targetPhysical->getStrengthType()];
            const float againstCurrent = currentUnit->getBattleStrengths()[targetPhysical->getStrengthType()];
            const int kind = targetPhysical->getStrengthType();
            float value = 0.0f;
            if (canFire && againstCurrent <= unit->getBattleStrengths()[kind])
            {
                engaged = true;
                if (!exposed)
                    value = own * own - targetUnit->getBattleStrengths()[strengthType];
                else
                    value = own * own - std::max(targetUnit->getBattleStrengths()[soldierStrengthType],
                                                 targetUnit->getBattleStrengths()[strengthType]);
            }
            if (value > best)
                best = value;
        }
        fireMax = 0.0f;
        std::list<std::pair<FireObject*, unsigned int> > fire =
            *static_cast<std::list<std::pair<FireObject*, unsigned int> >*>(static_cast<BotMain*>(bot)->getIncommingFire());
        for (std::list<std::pair<FireObject*, unsigned int> >::iterator it = fire.begin(); it != fire.end(); ++it)
        {
            const float strength = it->first->getStrength(strengthType);
            if (strength > fireMax)
                fireMax = strength;
            sensed.find(it->second);   // the shipped body makes this lookup and discards it
            Information* const shooter = informationManager->getInformation(it->second);
            if (shooter)
            {
                bot->getSide();         // both calls are made and discarded, as shipped
                shooter->getSide();
            }
        }
        if (!vehicleMobile && noVehicle)
            return 0.0f;
        if (engaged || !vehicleMobile)
        {
            if (isHeldForSquadArea(candidate, side, bot))
                return 0.0f;
            return best * 4.0f;
        }
    }
    else if (!vehicleMobile)
    {
        if (!armament && !object->getUnit()->mustBorrowArmament(object))
            return 0.0f;
        if (!armament)
            armament = informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(side))->getArmament();
        float reach = 0.0f;
        const std::vector<unsigned int> weapons = armament->getWeapons();
        for (std::vector<unsigned int>::const_iterator it = weapons.begin(); it != weapons.end(); ++it)
        {
            if (*it == static_cast<unsigned int>(-1))
                continue;
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(*it);
            if (weapon && weapon->getMaxRange() > reach)
                reach = weapon->getMaxRange();
        }
        std::vector<unsigned int> nearby;
        static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
            ->getEnemyObjects(nearby, bot, reach);
        float value = 0.0f;
        float best = 0.0f;
        if (!nearby.empty())
        {
            for (std::vector<unsigned int>::iterator it = nearby.begin(); it != nearby.end(); ++it)
            {
                Information* const target = informationManager->getInformation(*it);
                if (!target)
                    continue;
                const Vec3& targetPosition = target->getPosition();
                const float dx = targetPosition.x - position.x;
                const float dy = targetPosition.y - position.y;
                const float dz = targetPosition.z - position.z;
                const float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
                if (distance > maxRange || distance < minRange)
                    continue;
                IPIUnit* const targetUnit = target->getUnit();
                IPIPhysical* const targetPhysical = target->isPrimary()
                    ? target->getPhysical()
                    : informationManager->getInformation(target->getPrimaryObject(side))->getPhysical();
                if (!targetUnit || !targetPhysical)
                    continue;
                Vec3 direction = vectorFrom(position, target->getPosition());
                if (normaliseVector(direction) == 0)
                    controlInfo->validateCameraDirection(direction);
                const float own = strengths[targetPhysical->getStrengthType()];
                // Both shipped branches compute the same value; only a NaN strength skips it.
                if (own == strengths[targetPhysical->getStrengthType()])
                {
                    if (!exposed)
                        value = own * own - targetUnit->getBattleStrengths()[strengthType];
                    else
                        value = own * own - std::max(targetUnit->getBattleStrengths()[soldierStrengthType],
                                                     targetUnit->getBattleStrengths()[strengthType]);
                }
                if (value > best)
                    best = value;
            }
            return best;
        }
        ISAI* const sai = IAIMain::m_instance->getSAI(side);
        int area = sai->getBotOrderedPos(bot->getBotId());
        bool skip = false;
        if (area == -1 && sai->getBotCurrentPos(bot->getBotId()) != area)
            skip = true;
        if (!skip)
        {
            AIStrategicObject* const object2 = AIStrategicObjectManager::getInstance()->getStrategicObject(area);
            if (object2)
            {
                const Vec2 centre = object2->getPosition(side);
                const std::vector<AIStrategicObject*>& neighbours =
                    static_cast<AIStrategicArea*>(object2)->getNeighbours();
                for (std::vector<AIStrategicObject*>::const_iterator it = neighbours.begin(); it != neighbours.end(); ++it)
                {
                    if ((*it)->getStatus(side) == 0)
                        continue;
                    const Vec2& other = (*it)->getPosition(side);
                    Vec2 flat;
                    flat.x = other.x - centre.x;
                    flat.y = other.y - centre.y;
                    Vec3 direction;
                    direction.x = direction.y = direction.z = 0.0f;
                    vec2ToVec3XZ(flat, direction);
                    if (normaliseVector(direction) == 0 && controlInfo->validateCameraDirection(direction))
                        noVehicle = false;
                }
            }
        }
        if (!noVehicle)
            return 0.0f;
    }

    if (candidate->isPrimary() && !candidate->getMobile())
        return 0.0f;
    if (isHeldForSquadArea(candidate, side, bot))
        return 0.0f;
    float result = 0.0f;
    float running = 0.0f;
    for (int i = 0; i < kinds; ++i)
    {
        const float scaled = enemyCounts[i] > 0.5f ? strengths[i] * enemyCounts[i] * 3.0f : strengths[i] * 6.0f;
        if (running < scaled)
            running = scaled;
        if (enemyCounts[i] > 0.0f && result < running)
            result = running;
    }
    if (!(result > 0.0f))
        return result;
    if (!exposed)
        return result - (fireMax + enemyStrengths[strengthType]);
    return result - (std::max(enemyStrengths[soldierStrengthType], enemyStrengths[strengthType]) + fireMax);
}

// The worth of the seat to the bot: zero when booked by another player, under deep water, taken
// when a free seat is required, or team-locked; otherwise combat and speed worth, weighted by
// health and by how recently a team-mate held it. PE 101AC1D0; [INFERRED] name.
float equipmentValue(Bot* bot, Information* candidate, const float* enemyStrengths,
                     const float* enemyCounts, bool requireFree, Information* current, float, float,
                     int side)
{
    IPIPhysical* const physical = candidate->getPhysical();
    IPIUnit* const unit = candidate->getUnit();
    if (candidate->isBooked() && !candidate->isBookedBy(bot->getPlayerId()))
        return 0.0f;
    if (candidate != current && !((candidate->getObjectTypeFlags() >> 4) & 1)
        && static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getAltitude(candidate->getPosition()) > 8.0f)
        return 0.0f;
    if (requireFree && unit->isOccupied())
        return 0.0f;
    if (!unit->isTeamLocked(side))
        return 0.0f;
    const float combat = combatValue(bot, candidate, side, enemyStrengths, enemyCounts, requireFree, current);
    float value = seatSpeedValue(candidate, side, bot, requireFree, current) + combat;
    if (value > 0.01f)
        value = candidate->getStrength() * 4.0f + value;
    const float health = physical->getHealth();
    float bail = IBotManager::instance->getVehicleBailValue();
    if (!((candidate->getObjectTypeFlags() >> 21) & 1) && candidate->isPrimary()
        && candidate->getHandle() == current->getHandle())
        bail = bail * 0.3f;
    if (health < bail && !((candidate->getObjectTypeFlags() >> 21) & 1))
        return 0.0f;
    value = health * value;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (candidate->getSide() == side)
    {
        const double since = now - unit->getLastPlayerPossesionTime();
        if (since < s_possessionGrace)
            return static_cast<float>(1.0 / s_possessionGrace * since) * value;
    }
    return value;
}

} // anonymous namespace

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

// PE 100F6D90.
float BBChange::acceptedUrgency(Bot* bot)
{
    const int id = bot->getBotId();
    const float urgency = m_urgency[id];
    Information* const target = informationManager->getInformation(m_targets[id]);
    if (target && target->isBooked() && !target->isBookedBy(bot->getPlayerId()) && m_urgency[id] > 0.0f)
        return 0.0f;
    return urgency;
}

// PE 100F6950. The possession time is read before the unit is checked, as shipped.
float BBChange::seatFactor(Information* vehicle, bool allowOccupied)
{
    if (!vehicle)
        return 1.0f;
    IPIUnit* const unit = vehicle->getUnit();
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double since = now - vehicle->getUnit()->getLastPlayerPossesionTime();
    if (!unit)
        return 1.0f;
    if (!allowOccupied && unit->isOccupied())
        return 1.0f;
    if (!vehicle->getMobile())
        return 1.0f;
    if (!(since < 15.0))
        return 1.0f;
    const unsigned int flags = vehicle->getObjectTypeFlags();
    if ((flags >> 7) & 1)
        return 1.0f;
    if ((flags >> 6) & 1)
        return 0.77f;
    if ((flags >> 4) & 1)
        return 0.5f;
    return 1.0f;
}

// PE 100F6E60.
bool BBChange::canLeaveEquipment(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const unsigned int soldier = bot->getPlayer()->getSoldier()->getInformation(bot->getSide());
    if (!informationManager->getInformation(soldier))
        return false;
    if (equipment->isPrimary())
    {
        const Vec3& position = equipment->getPosition();
        equipment->getUnit()->useNoPathfindingToGetToObject();   // discarded, as shipped
        equipment->isPrimary();                                  // discarded, as shipped
        if ((equipment->getObjectTypeFlags() >> 4) & 1)
        {
            const float altitude = AerialHeightMap::m_instance->getAltitude(position);
            if (altitude > 2.0f && altitude < 12.0f)
                return false;
        }
    }
    if (!equipment->getMobile())
        equipment->isPrimary();                                  // discarded, as shipped
    return true;
}

// PE 100F71B0.
bool BBChange::isRequestedByOccupant(unsigned int handle, const std::list<AIRadioMessageInfo>* requests,
                                     int side)
{
    Information* const information = informationManager->getInformation(handle);
    if (information->getSide() != side && isOccupiedObject(handle, side))
        return false;
    for (std::list<AIRadioMessageInfo>::const_iterator it = requests->begin(); it != requests->end(); ++it)
    {
        if (it->m_handled)
            continue;
        if (information->getPlayerId() == it->m_senderPlayerId)
            return true;
        int iterator;
        for (Information* seat = informationManager->getInformation(information->getFirstSecondaryObject(iterator, side));
             seat; seat = informationManager->getInformation(information->getSecondaryObject(iterator, side)))
        {
            if (seat->getPlayerId() == it->m_senderPlayerId)
                return true;
        }
    }
    return false;
}

// PE 100F6A10.
bool BBChange::isEnemyOccupied(const unsigned int* handle, Information* information, const int* side)
{
    return information->getSide() != *side && isOccupiedObject(*handle, *side);
}

// PE 100F6A50.
bool BBChange::isOverturned(Information* vehicle, IAITerrain*, float)
{
    return vehicle->getTransformation().m[5] < 0.01f;
}

bool BBChange::isLandingAreaClear(Information*, Information*, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChange::isLandingAreaClear");
}

// PE 100F6F60.
void BBChange::answerPickUp(Information* equipment, unsigned int requester, CommunicationAgent* agent)
{
    if (!agent->handleMessages(static_cast<AIRadioMessageType>(1)))
        return;
    Information* const other = informationManager->getInformation(requester);
    const Vec3& own = equipment->getPosition();
    const Vec3& theirs = other->getPosition();
    const float dx = own.x - theirs.x;
    const float dy = own.y - theirs.y;
    const float dz = own.z - theirs.z;
    if (dz * dz + dx * dx + dy * dy < 225.0f)
        agent->enqueueRadioMessage(7, 6, 3, 1.0f, -1);
}

// PE 100F7030.
bool BBChange::isRequesterAboard(unsigned int target, unsigned int requester, int side)
{
    Information* const requesterInformation = informationManager->getInformation(requester);
    Information* const primary =
        informationManager->getInformation(requesterInformation->getPrimaryObject(side));
    if (primary == informationManager->getInformation(target))
        return true;
    int iterator;
    unsigned int seat = primary->getFirstSecondaryObject(iterator, side);
    while (informationManager->getInformation(seat))
    {
        if (seat == target)
            return true;
        seat = primary->getSecondaryObject(iterator, side);
    }
    return false;
}

// PE 100F76B0. Weighs the bot's current seat against getting out, the other seats of its vehicle
// and the vehicles nearby, using the side's enemy picture. The unflagged pass keeps or drops the
// chosen target; the flagged pass picks the best seat and offers the change to it, with urgency
// from how much better it is than the current seat.
float BBChange::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    const int side = bot->getSide();
    const int id = bot->getBotId();
    bot->getSquadMember();   // the shipped body makes this call and discards the result
    const float fire = bot->getFireInclination();
    const float move = bot->getMoveInclination();
    float fireShare;
    float moveShare;
    if (move > 0.0f)
    {
        if (fire > 0.0f)
        {
            const float scale = 1.0f / (move + fire);
            fireShare = fire * scale;
            moveShare = scale * move;
        }
        else
        {
            fireShare = 0.0f;
            moveShare = 1.0f;
        }
    }
    else if (fire > 0.0f)
    {
        fireShare = 1.0f;
        moveShare = 0.0f;
    }
    else
    {
        fireShare = 0.5f;
        moveShare = 0.5f;
    }
    IAIStrategicEnvironment* const environment =
        static_cast<IAIStrategicEnvironment*>(AISingletonServer::m_iaiStrategicEnvironment);
    CommunicationAgent* const agent = bot->getCommunicationAgent();
    const float* const enemyCounts = environment->getEnemyTypes(bot);
    const float* const enemyStrengths = environment->getEnemyStrengths(bot);
    PlanGenerator* const generator = m_generator;

    if (!flag)
    {
        const unsigned int current = bot->getCurrentEquipment();
        const unsigned int soldier = bot->getPlayer()->getSoldier()->getInformation(side);
        const bool onFoot = soldier == current;
        if (!onFoot && canLeaveEquipment(bot))
        {
            Information* const currentInformation = informationManager->getInformation(current);
            float stay = equipmentValue(bot, currentInformation, enemyStrengths, enemyCounts, false,
                                        currentInformation, fireShare, moveShare, side) * 2.5f;
            if (!currentInformation->isPrimary())
                stay = seatFactor(informationManager->getInformation(currentInformation->getPrimaryObject(side)), false) * stay;
            const float leave = equipmentValue(bot, informationManager->getInformation(soldier), enemyStrengths,
                                               enemyCounts, false, currentInformation, fireShare, moveShare,
                                               side) * 0.2f;
            if (stay > leave)
            {
                Information* const target = informationManager->getInformation(m_targets[id]);
                if (target)
                {
                    if (!target->getUnit()->isOccupied() && generator->hasPlan(bot))
                    {
                        m_targetChanged[id] = false;
                        m_urgency[id] = acceptedUrgency(bot);
                        return m_urgency[id];
                    }
                    m_targetChanged[id] = true;
                }
                m_targetChanged[id] = generator->hasPlan(bot);
                m_targets[id] = static_cast<unsigned int>(-1);
                generator->reset(bot);
                m_urgency[id] = 0.0f;
                return 0.0f;
            }
            m_targetChanged[id] = m_targets[id] != soldier;
            m_targets[id] = soldier;
            static_cast<BBPChange*>(generator)->offerChange(soldier, false, false, bot);
            if (stay == 0.0f)
                m_urgency[id] = DecleiningSlopeCurve::calculate(leave * 0.5f) * elapsed * 4.0f;
            else
                m_urgency[id] = DecleiningSlopeCurve::calculate(leave / stay * 0.5f) * elapsed * 4.0f;
            m_urgency[id] = acceptedUrgency(bot);
            return m_urgency[id];
        }
        Information* const target = informationManager->getInformation(m_targets[id]);
        if (onFoot && target)
        {
            if (target->getUnit() && !target->getUnit()->isOccupied() && generator->hasPlan(bot))
            {
                m_targetChanged[id] = false;
                m_urgency[id] = acceptedUrgency(bot);
                if ((informationManager->getInformation(current)->getObjectTypeFlags() >> 21) & 1)
                {
                    const std::list<AIRadioMessageInfo>& pickUps =
                        agent->getReceivedMessages(static_cast<AIRadioMessageType>(1));
                    if (pickUps.size() != 0 && isRequestedByOccupant(m_targets[id], &pickUps, side))
                        static_cast<BBPChange*>(generator)->setViaRequest(true, bot);
                }
                m_urgency[id] = acceptedUrgency(bot);
                return m_urgency[id];
            }
            m_targetChanged[id] = true;
        }
        m_targetChanged[id] = generator->hasPlan(bot);
        m_targets[id] = static_cast<unsigned int>(-1);
        generator->reset(bot);
        m_urgency[id] = 0.0f;
        return 0.0f;
    }

    SquadManager* const squadManager = SquadManager::m_instance;
    float currentValue = 0.0f;
    bool viaRequest = false;
    bool leaveVehicle = false;
    Vec3 entry;
    entry.x = entry.y = entry.z = -666.0f;
    Vec3 bestEntry;
    bestEntry.x = bestEntry.y = bestEntry.z = -666.0f;
    const unsigned int currentHandle = bot->getCurrentEquipment();
    Information* const currentInformation = informationManager->getInformation(currentHandle);
    const Vec3& position = currentInformation->getPosition();
    double zoneTime = 0.0;
    int zoneSide = -1;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const bool soldier = (currentInformation->getObjectTypeFlags() >> 21) & 1;
    bool underArtillery = false;
    if (!((currentInformation->getObjectTypeFlags() >> 4) & 1))
    {
        if (squadManager->isInArtilleryDangerZone(position, bot->getSide(), zoneTime, zoneSide) && zoneTime > 4.0)
            underArtillery = true;
    }
    if (!soldier && !canLeaveEquipment(bot))
    {
        m_targetChanged[id] = generator->hasPlan(bot);
        m_targets[id] = static_cast<unsigned int>(-1);
        generator->reset(bot);
        m_urgency[id] = 0.0f;
        return 0.0f;
    }
    const unsigned int primaryHandle = currentInformation->getPrimaryObject(side);
    Information* const primary = informationManager->getInformation(primaryHandle);
    Information* const ownPrimary = informationManager->getInformation(currentInformation->getPrimaryObject(side));
    unsigned int bestHandle = primaryHandle;
    float seatValue;
    int vehicleNumber;
    if (currentHandle == primaryHandle)
    {
        seatValue = equipmentValue(bot, primary, enemyStrengths, enemyCounts, false, currentInformation,
                                   fireShare, moveShare, side) * 2.0f;
        currentValue = seatValue;
        if (seatValue < 0.0f)
            currentValue = 0.0f;
        if (primary->getMobile())
        {
            vehicleNumber = primary->getMobile()->getVehicleNumber();
            primary->getUnit()->getEquipmentType();   // discarded, as shipped
        }
        else
        {
            vehicleNumber = -1;
        }
    }
    else
    {
        seatValue = equipmentValue(bot, primary, enemyStrengths, enemyCounts, true, currentInformation,
                                   fireShare, moveShare, side) * 2.0f;
        vehicleNumber = -1;
    }

    // An overturned vehicle is worth nothing; otherwise compare its seats.
    bool overturned = false;
    float bestValue;
    if (!soldier && primary->getTransformation().m[5] < 0.01f)
    {
        overturned = true;
        seatValue = 0.0f;
        if (((primary->getObjectTypeFlags() >> 4) & 1) && !primary->getPhysical()->isInAir())
            overturned = false;
        bestValue = 0.0f;
    }
    else
    {
        bestValue = seatValue;
    }
    Information* bestInformation = primary;
    if (!overturned)
    {
        int iterator;
        unsigned int seatHandle = primary->getFirstSecondaryObject(iterator, side);
        Information* seat = informationManager->getInformation(seatHandle);
        const float factor = seatFactor(primary, primaryHandle == currentHandle);
        while (seat)
        {
            float value;
            if (seatHandle == currentHandle)
            {
                value = equipmentValue(bot, seat, enemyStrengths, enemyCounts, false, currentInformation,
                                       fireShare, moveShare, side) * 2.0f * factor;
                currentValue = value;
                if (value < 0.0f)
                    currentValue = 0.0f;
            }
            else
            {
                value = equipmentValue(bot, seat, enemyStrengths, enemyCounts, true, currentInformation,
                                       fireShare, moveShare, side) * factor;
            }
            if (!soldier && bot->getSquadMember()->isAtSquadArea() && bot->getSquadMember()->getFlag49()
                && !primary->getArmament())
                value = value * 5.0f;
            if (bestValue < value)
            {
                bestValue = value;
                bestHandle = seatHandle;
                bestInformation = seat;
                bestEntry.x = bestEntry.y = bestEntry.z = -666.0f;
            }
            seatHandle = primary->getSecondaryObject(iterator, side);
            seat = informationManager->getInformation(seatHandle);
        }
    }

    // Getting out on foot.
    const unsigned int soldierHandle = bot->getPlayer()->getSoldier()->getInformation(side);
    const std::list<AIRadioMessageInfo>& bailOrders = agent->getReceivedMessages(static_cast<AIRadioMessageType>(0));
    bool ordered = false;
    if (bailOrders.size() != 0 && humanRequester(bailOrders))
        ordered = true;
    if (soldierHandle != currentHandle)
    {
        Information* const soldierInformation = informationManager->getInformation(soldierHandle);
        float value = equipmentValue(bot, soldierInformation, enemyStrengths, enemyCounts, false,
                                     currentInformation, fireShare, moveShare, side) * 0.2f;
        if (bailOrders.size() != 0)
        {
            if (ordered && !((currentInformation->getObjectTypeFlags() >> 21) & 1))
                value = value * bailOrders.front().m_weight * 100.0f;
            if (ordered && ((currentInformation->getObjectTypeFlags() >> 4) & 1))
                value = 10000.0f;
        }
        if (underArtillery)
            value = 10000.0f;
        if (bot->isBotStuck() && soldierInformation)
        {
            bot->setBotStuck(false);
            value = 10000.0f;
        }
        AIStrategicArea* const area = bot->getSquadMember()->getSquad()->getSquadStrategicAreaAssignment();
        if (area)
        {
            const Vec3 first = area->getOrderPos(0, bot->getSide());
            const Vec3 second = area->getOrderPos(1, bot->getSide());
            if (std::fabs(first.y - second.y) > 3.0f && isHeldForSquadArea(ownPrimary, ownPrimary->getSide(), bot))
                value = 10000.0f;
        }
        if (currentValue < 0.001f)
            value = 10000.0f;
        soldierInformation->getMobile()->getVehicleNumber();      // discarded, as shipped
        soldierInformation->getUnit()->getEquipmentType();        // discarded, as shipped
        if (bestValue < value)
        {
            bestValue = value;
            bestHandle = soldierHandle;
            bestInformation = soldierInformation;
            leaveVehicle = true;
        }
    }

    float orderFactor;
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
        && bot->getOrderList(m_behaviourId)->size() == 0)
        orderFactor = bot->getMemory()->m_float8;
    else
        orderFactor = 1.0f;

    // A pick-up request from a human in the vehicle the bot is heading for.
    bool pickUp = false;
    unsigned int targetPrimary = static_cast<unsigned int>(-1);
    if (informationManager->getInformation(m_targets[id]))
    {
        targetPrimary = informationManager->getInformation(m_targets[id])->getPrimaryObject(side);
        const std::list<AIRadioMessageInfo>& pickUps = agent->getReceivedMessages(static_cast<AIRadioMessageType>(1));
        if (pickUps.size() != 0 && ((currentInformation->getObjectTypeFlags() >> 21) & 1))
        {
            IAIPlayer* const requester = humanRequester(pickUps);
            if (requester && targetPrimary == requester->getCurrentVehicle()->getPrimaryObject()->getInformation(side))
                pickUp = true;
        }
    }

    unsigned int requesting = static_cast<unsigned int>(-1);
    if (!leaveVehicle && !(bailOrders.size() != 0 && ordered)
        && !((currentInformation->getObjectTypeFlags() >> 4) & 1) && !underArtillery
        && !bot->getSquadMember()->isNearSquadGoal())
    {
        // Other vehicles nearby.
        m_vector24.resize(0, 0);
        float radius = static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
                           ->getHardware(m_vector24, position, bot);
        const std::list<AIRadioMessageInfo>& pickUps = agent->getReceivedMessages(static_cast<AIRadioMessageType>(1));
        const float radiusSquared = radius * radius;
        bool reachable = false;
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
        for (std::vector<unsigned int>::iterator it = m_vector24.begin(); it != m_vector24.end(); ++it)
        {
            requesting = static_cast<unsigned int>(-1);
            if (pickUps.size() != 0 && ((currentInformation->getObjectTypeFlags() >> 21) & 1)
                && isRequestedByOccupant(*it, &pickUps, side))
                requesting = *it;
            unsigned int handle = *it;
            Information* const candidate = informationManager->getInformation(handle);
            if (!candidate || candidate == ownPrimary || ((candidate->getObjectTypeFlags() >> 21) & 1))
                continue;
            if (pickUp && handle != targetPrimary)
                continue;
            if ((candidate->getObjectTypeFlags() >> 7) & 1)
            {
                if (pickUps.size() == 0)
                    continue;
                IAIPlayer* const requester = humanRequester(pickUps);
                if (!requester)
                    continue;
                const unsigned int vehicle = requester->getCurrentVehicle()->getPrimaryObject()->getInformation(side);
                Information* const vehicleInformation = informationManager->getInformation(vehicle);
                if (handle != vehicle || !vehicleInformation)
                    continue;
                IPIPhysical* const vehiclePhysical = vehicleInformation->getPhysical();
                if (!vehiclePhysical || !vehiclePhysical->isInAir() || !vehiclePhysical->isInWater())
                    continue;
            }
            if (!candidate->getAIObject()->getIsAllowedByBots())
                continue;
            if (isEnemyOccupied(&handle, candidate, &side))
                continue;
            const Vec3& candidatePosition = candidate->getPosition();
            IPIMobile* const candidateMobile = candidate->getMobile();
            if (candidateMobile)
            {
                if (!((candidate->getObjectTypeFlags() >> 4) & 1))
                {
                    const IPIMobile::ValidPos valid = candidateMobile->getLastValidPosition();
                    if (valid.quadTree == static_cast<unsigned int>(-1))
                    {
                        if (pickUps.size() == 0)
                            continue;
                        IAIPlayer* const requester = humanRequester(pickUps);
                        if (!requester)
                            continue;
                        const unsigned int vehicle =
                            requester->getCurrentVehicle()->getPrimaryObject()->getInformation(side);
                        const unsigned int seat = requester->getCurrentVehicle()->getInformation(side);
                        if (handle != vehicle || vehicle != seat)
                            continue;
                    }
                }
                if (!((candidate->getObjectTypeFlags() >> 4) & 1) && !((candidate->getObjectTypeFlags() >> 21) & 1)
                    && isOverturned(candidate, terrain, 0.70710677f))
                    continue;
            }
            IPIUnit* const unit = candidate->getUnit();
            const float distanceSquared = vectorLengthSquared(vectorFrom(position, candidatePosition));
            const float nearness =
                std::max(minValue((radiusSquared - distanceSquared * 0.8f) / radiusSquared, 0.5f) + 0.5f, 0.0f);
            if (vehicleNumber == -1)
            {
                reachable = true;
            }
            else if (unit)
            {
                if (unit->useNoPathfindingToGetToObject())
                {
                    reachable = true;
                    entry = unit->getEntryPositionOfNonMobile();
                }
                else if (!candidate->getMobile())
                {
                    reachable = true;
                    entry = unit->getEntryPositionOfNonMobile();
                }
                else
                {
                    const IPIMobile::ValidPos& valid = candidateMobile->getLastValidPosition(0);
                    if (candidate->getPhysical())
                    {
                        const float candidateRadius = candidate->getPhysical()->getRadius();
                        if (equipment->getPhysical())
                        {
                            const float reach = equipment->getPhysical()->getRadius() + candidateRadius;
                            reachable = reach * reach > vectorLengthSquared(vectorFrom(valid.position, candidatePosition));
                        }
                    }
                }
            }
            if (!reachable)
                continue;
            float value = equipmentValue(bot, candidate, enemyStrengths, enemyCounts, true, currentInformation,
                                         fireShare, moveShare, side) * nearness;
            if (pickUp)
                value = value * 5.0f;
            if (soldier && distanceSquared < 400.0f && currentInformation->getPhysical()->getHealth() < 0.25f)
                value = value * 50.0f;
            if (requesting != static_cast<unsigned int>(-1))
                value = value * pickUps.front().m_weight;
            if (bestValue < value)
            {
                if (!((candidate->getObjectTypeFlags() >> 4) & 1) || ((candidate->getObjectTypeFlags() >> 5) & 1)
                    || isLandingAreaClear(candidate, currentInformation, side))
                {
                    bestValue = value;
                    bestHandle = handle;
                    viaRequest = requesting != static_cast<unsigned int>(-1);
                    bestInformation = candidate;
                    bestEntry = entry;
                }
            }
            int iterator;
            unsigned int seatHandle = candidate->getFirstSecondaryObject(iterator, side);
            Information* seat = informationManager->getInformation(seatHandle);
            const float factor = seatFactor(candidate, false);
            while (seat)
            {
                float seatValueHere = equipmentValue(bot, seat, enemyStrengths, enemyCounts, true,
                                                     currentInformation, fireShare, moveShare, side)
                                      * nearness * factor;
                if (pickUp)
                    seatValueHere = seatValueHere * 5.0f;
                else if (soldier && distanceSquared < 400.0f && currentInformation->getPhysical()
                         && currentInformation->getPhysical()->getHealth() < 0.25f)
                    seatValueHere = seatValueHere * 50.0f;
                if (requesting != static_cast<unsigned int>(-1))
                    seatValueHere = seatValueHere * pickUps.front().m_weight;
                if (bestValue < seatValueHere)
                {
                    bestValue = seatValueHere;
                    bestHandle = seatHandle;
                    viaRequest = requesting != static_cast<unsigned int>(-1);
                    bestInformation = seat;
                    bestEntry = entry;
                }
                seatHandle = candidate->getSecondaryObject(iterator, side);
                seat = informationManager->getInformation(seatHandle);
            }
        }
    }

    if (bestHandle == currentHandle)
    {
        if (soldier && requesting != static_cast<unsigned int>(-1))
            answerPickUp(currentInformation, requesting, agent);
        m_targetChanged[id] = generator->hasPlan(bot);
        m_targets[id] = static_cast<unsigned int>(-1);
        generator->reset(bot);
        m_urgency[id] = 0.0f;
        return 0.0f;
    }
    const bool samePrimary = bestInformation->getPrimaryObject(side) == currentInformation->getPrimaryObject(side);
    m_targetChanged[id] = m_targets[id] != bestHandle;
    m_targets[id] = bestHandle;
    if (soldier && requesting != static_cast<unsigned int>(-1) && !isRequesterAboard(m_targets[id], requesting, side))
        answerPickUp(currentInformation, requesting, agent);
    static_cast<BBPChange*>(generator)->offerChange(bestHandle, &bestEntry, samePrimary, viaRequest, bot);
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (currentValue == 0.0f)
        m_urgency[id] = DecleiningSlopeCurve::calculate(bestValue * 0.5f) * orderFactor * elapsed * 4.0f;
    else
        m_urgency[id] = DecleiningSlopeCurve::calculate(bestValue / currentValue * 0.5f) * orderFactor * elapsed * 4.0f;
    if (now - currentInformation->getUnit()->getLastPlayerPossesionTime() < 15.0)
        m_urgency[id] = m_urgency[id] * 6.0f;
    if (leaveVehicle)
    {
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
        if (equipment)
        {
            if (equipment->getPhysical())
            {
                const float health = equipment->getPhysical()->getHealth();
                if (!(IBotManager::instance->getVehicleBailValue() + 0.1f < health))
                    agent->sendRadioMessageNow(0, 7, 10, 0);
            }
            m_urgency[id] = m_urgency[id] * 5.0f;
        }
    }
    if (soldier && vectorLengthSquared(vectorFrom(position, bestInformation->getPosition())) < 400.0f
        && currentInformation->getPhysical() && currentInformation->getPhysical()->getHealth() < 0.25f)
        m_urgency[id] = m_urgency[id] * 5.0f;
    bot->insertBehaviourInformation(0.5f, informationManager->getInformation(m_targets[bot->getBotId()])->getPosition(),
                                    m_behaviourId);
    m_urgency[id] = acceptedUrgency(bot);
    return m_urgency[id];
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

// PE 10122060. With a charge placed, detonate it when the enemy is taking over the area it was
// placed for and no friendly vehicle is within 15 m (urgency 100); otherwise, with C4 left, place
// it on an owned control point within 10 m (urgency 500). The scan for the C4 weapon keeps the
// last weapon looked at when none is type 4, as shipped.
float BBC4::calculateUrgency(Bot* bot, bool flag, float)
{
    if (!flag)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        return m_urgency[bot->getBotId()];
    }
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    AIStrategicObject* target = 0;
    float urgency = 0.0f;
    ISAI* const sai = IAIMain::m_instance->getSAI(bot->getSide());
    bool offer = false;
    bool detonate = false;
    if (equipment && ((equipment->getObjectTypeFlags() >> 21) & 1) && isUsable(equipment, bot))
    {
        WeaponInfo* c4 = 0;
        std::vector<unsigned int>& weapons = equipment->getArmament()->getWeapons();
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            c4 = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (c4 && c4->getWeaponType() == 4)
                break;
        }
        const C4Info charge = bot->getC4();
        const Vec3 position = equipment->getPosition();
        if (charge.placed)
        {
            target = charge.object;
            if (charge.placementType == 0)
            {
                AIStrategicObject* contested = 0;
                Vec2 where;
                where.x = 0.0f;
                where.y = 0.0f;
                if (sai->hostileTakeoveInProgress(contested, where) && charge.object
                    && contested->m_index == charge.object->m_index
                    && !SquadManager::m_instance->isFriendlyNearPosition(charge.position, 15.0f, bot->getSide()))
                {
                    detonate = true;
                    urgency = 100.0f;
                    offer = true;
                }
            }
        }
        else if (c4->getCurrentAmmo() > 0
                 && sai->isPosInsideOwnedControlPoint(target, position, 10.0f))
        {
            urgency = 500.0f;
            offer = true;
        }
    }
    if (offer)
        static_cast<BBPC4*>(m_generator)->offerPlan(bot, detonate, target);
    else
        m_generator->reset(bot);
    m_urgency[bot->getBotId()] = urgency;
    return m_urgency[bot->getBotId()];
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
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
    m_startTime = 0.0;
    m_active = false;
    m_duration = 0.0;
}

BBRandom::~BBRandom()
{
    delete[] m_targets;
}

// PE 101260E0. When the navigation reports a position to avoid, one episode starts for its time
// left; once that time has run out the behaviour takes over for five more seconds at urgency
// 1000. The line test against the position is made and its result discarded, as shipped.
float BBRandom::calculateUrgency(Bot* bot, bool flag, float)
{
    if (!flag)
    {
        if (!m_generator->hasPlan(bot))
        {
            m_generator->reset(bot);
            m_urgency[bot->getBotId()] = 0.0f;
            return 0.0f;
        }
        return m_urgency[bot->getBotId()];
    }

    Vec3 position;
    position.x = position.y = position.z = 0.0f;
    if (!m_active)
    {
        double timeLeft;
        if (bot->getBotNavigation()->getAvoidPosition(position, timeLeft)
            && bot->getBotNavigation()->getFlag276())
        {
            m_active = true;
            m_startTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            m_duration = timeLeft;
            m_position = position;
        }
    }
    else
    {
        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        const double end = m_duration + m_startTime;
        if (now > end)
        {
            if (now > end + 5.0)
                m_active = false;
            Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
            std::vector<IAIObject*> ignore;
            ignore.push_back(equipment->getAIObject());
            IAIObjectEnvironment* const environment =
                static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
            const Vec3& origin = equipment->getPosition();
            Vec3 from;
            from.x = origin.x;
            from.y = origin.y + 1.0f;
            from.z = origin.z;
            bool hit;
            environment->collideLineWithWorld(from, m_position, ignore, false, 0.7f, false, hit);
            static_cast<BBPRandom*>(m_generator)->offerPlan(bot);
            m_urgency[bot->getBotId()] = 1000.0f;
            return 1000.0f;
        }
    }
    m_generator->reset(bot);
    m_urgency[bot->getBotId()] = 0.0f;
    return m_urgency[bot->getBotId()];
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

// PE 10127BD0. Never urgent.
float BBTriggerable::calculateUrgency(Bot* bot, bool, float)
{
    m_urgency[bot->getBotId()] = 0.0f;
    return m_urgency[bot->getBotId()];
}

// PE 101268F0. Always false; the checks are made and discarded, as shipped.
bool BBTriggerable::isUsable(Information*, Bot* bot)
{
    if (bot->getPlayer()->getIsAlive())
        bot->getManDown();
    return false;
}

float BBMedicAssist::calculateMedicUrgency(Bot*, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMedicAssist::calculateMedicUrgency");
}

// PE 100FCE70.
bool BBMedicAssist::hasHumanOccupant(Information* information, int side)
{
    if (!information)
        return false;
    Information* const primary = informationManager->getInformation(information->getPrimaryObject(side));
    if (!primary)
        return false;
    IAIPlayerFactory* const players = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    IPIUnit* unit = primary->getUnit();
    if (unit && unit->isOccupied())
    {
        IAIPlayer* const player = players->getPlayerFromId(unit->getOccupyingPlayerId(side));
        if (player && !player->getIsAIPlayer())
            return true;
    }
    int iterator;
    for (Information* seat = informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
         seat; seat = informationManager->getInformation(primary->getSecondaryObject(iterator, side)))
    {
        unit = seat->getUnit();
        if (unit && unit->isOccupied())
        {
            IAIPlayer* const player = players->getPlayerFromId(unit->getOccupyingPlayerId(side));
            if (player && !player->getIsAIPlayer())
                return true;
        }
    }
    return false;
}

// PE 100FCFD0. The seat walk runs on the object even when it is null, as shipped.
bool BBMedicAssist::hasSupportRequest(Information* information, int side, CommunicationAgent* agent)
{
    if (information && agent->hasMessage(static_cast<AIRadioMessageType>(4), information->getPlayerId()))
        return true;
    int iterator;
    for (Information* seat = informationManager->getInformation(information->getFirstSecondaryObject(iterator, side));
         seat; seat = informationManager->getInformation(information->getSecondaryObject(iterator, side)))
    {
        if (agent->hasMessage(static_cast<AIRadioMessageType>(4), seat->getPlayerId()))
            return true;
    }
    return false;
}

// PE 100FE080. Kit 3 takes its own pass; the other kits look for friendly units in need of
// support (health below the threshold, or a support request over the radio) that one of the
// equipment's support weapons can reach.
float BBMedicAssist::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    if (!flag)
    {
        Information* const target = informationManager->getInformation(m_targets[bot->getBotId()]);
        if (target && target->getSide() == bot->getSide())
        {
            m_targetChanged[bot->getBotId()] = false;
            if (!m_generator->hasPlan(bot))
                m_urgency[bot->getBotId()] = 0.0f;
        }
        else
        {
            m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
            m_urgency[bot->getBotId()] = 0.0f;
            m_generator->reset(bot);
        }
        return m_urgency[bot->getBotId()];
    }

    if (bot->getPlayer()->getSpawnKit() == 3)
        return calculateMedicUrgency(bot, elapsed);

    const int strengthCount = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    int bestWeapon = -1;
    int bestPlayerId = -1;
    Vec3 normal;
    normal.x = normal.y = normal.z = 0.0f;
    const int kit = bot->getPlayer()->getSpawnKit();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& position = equipment->getPosition();
    for (int i = 0; i < strengthCount; ++i)
    {
        m_battle34[i] = -1;
        m_battle3C[i] = 0;
    }
    IPIArmament* const armament = equipment->getArmament();
    IPIMobile* const mobile = equipment->getMobile();
    int vehicleNumber;
    if (mobile)
    {
        vehicleNumber = mobile->getVehicleNumber();
        equipment->getUnit()->getEquipmentType();   // result unused, as shipped
    }
    else
    {
        vehicleNumber = -2;
    }

    // The strongest support weapon (type 1) per strength kind, with its range.
    std::vector<unsigned int> weapons(armament->getWeapons());
    for (unsigned int index = 0; weapons.begin() != weapons.end() && index < weapons.size(); ++index)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[index]);
        if (!weapon || weapon->getWeaponType() != 1)
            continue;
        for (int kind = 0; kind < strengthCount; ++kind)
        {
            if (weapon->getStrength(kind) > m_battle3C[kind])
            {
                m_battle34[kind] = static_cast<int>(index);
                m_battle3C[kind] = weapon->getStrength(kind);
                m_battle38[kind] = weapon->getMaxRange();
            }
        }
    }

    unsigned int ordersMatched = 0;
    float bestValue = 0.0f;
    m_vector24.resize(0, 0);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getFriendlyAndNeutralUnits(m_vector24, position, bot, 75.0f);
    unsigned int best = static_cast<unsigned int>(-1);
    CommunicationAgent* const agent = bot->getCommunicationAgent();
    const std::list<AIRadioMessageInfo>& reports = agent->getReceivedMessages(static_cast<AIRadioMessageType>(3));
    const std::list<AIRadioMessageInfo>& requests = agent->getReceivedMessages(static_cast<AIRadioMessageType>(4));
    bool haveReports = reports.size() != 0;
    bool haveRequests = requests.size() != 0;
    if (bot->isAirBorn())
    {
        haveReports = false;
        haveRequests = false;
    }
    // The shipped body also reads three squad accessors here and discards them.

    bool bestNeedsHuman = false;
    bool humanAsked = false;
    float bestDistance = 100.0f;
    for (std::vector<unsigned int>::iterator it = m_vector24.begin(); it != m_vector24.end(); ++it)
    {
        float threshold = 0.75f;
        bool needsHuman = false;
        Information* const target = informationManager->getInformation(*it);
        if (!target || !target->getPhysical())
            continue;
        if (target == equipment && bot->getPlayer()->getSpawnKit() == 5 && bot->isFiring())
            continue;
        if (((target->getObjectTypeFlags() >> 7) & 1) && !hasHumanOccupant(target, bot->getSide()))
            continue;
        if (target == equipment && bot->getPlayer()->getSpawnKit() == 5 && armament->getActiveWeapon() != -1)
        {
            WeaponInfo* const active =
                WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            if (active && active->getWeaponType() == 1)
                continue;
        }
        IPIPhysical* const physical = target->getPhysical();
        const int strengthType = physical->getStrengthType();
        if (physical->getManDown())
            continue;
        if (hasHumanOccupant(target, bot->getSide()))
        {
            needsHuman = true;
            threshold = 0.99f;
        }
        if (m_battle34[strengthType] == -1)
            continue;
        const float health = physical->getHealth();
        int occupantId = -1;
        IPIUnit* unit = target->getUnit();
        if (unit)
            occupantId = unit->getOccupyingPlayerId(target->getSide());
        AIRadioMessageInfo report;
        bool reported = false;
        if (haveReports)
            reported = agent->findMessage(report, static_cast<AIRadioMessageType>(3), occupantId);
        AIRadioMessageInfo request;
        bool requested = false;
        if (haveRequests)
            requested = hasSupportRequest(target, target->getSide(), agent);
        if (!requested && bot->getPlayer()->getSpawnKit() == 4 && bot->getSquadMember()->isNearSquadGoal())
            continue;
        if (!((health < threshold && health > 0.0f) || reported || requested))
            continue;
        if (health > threshold)
            continue;
        const Vec3& targetPosition = target->getPosition();
        static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
            ->getNormal(targetPosition.x, targetPosition.z, normal);
        const float* const m = target->getTransformation().m;
        if (normal.z * m[6] + normal.y * m[5] + normal.x * m[4] < 0.7071f)
            continue;
        unit = target->getUnit();
        if (unit->getAlwaysTargetSoldier())
            continue;
        if (vectorLength(vectorFrom(targetPosition, position)) > m_battle38[strengthType])
        {
            if (!mobile)
                continue;
            if (!unit->useNoPathfindingToGetToObject() && target->getMobile())
            {
                const IPIMobile::ValidPos& valid = mobile->getLastValidPosition(vehicleNumber);
                const float range = m_battle38[strengthType];
                // Shipped comparison: an object closer to the last valid position than the
                // range is skipped.
                if (range * range > vectorLengthSquared(vectorFrom(targetPosition, valid.position)))
                    continue;
            }
        }
        float distance;
        if (target == equipment)
            distance = 1.0f;
        else
            distance = vectorLength(vectorFrom(targetPosition, position));

        float value;
        bool ordered = false;
        if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
            && ordersMatched < bot->getOrderList(m_behaviourId)->size())
        {
            std::list<unsigned int>* const orders = bot->getOrderList(m_behaviourId);
            if (std::find(orders->begin(), orders->end(), *it) != orders->end())
            {
                ++ordersMatched;
                ordered = true;
            }
        }
        if (ordered)
            value = target->getStrength() * bot->getMemory()->m_float8
                    / (DecleiningSlopeCurve::calculate(1.0f - health) * distance);
        else
            value = target->getStrength() / (DecleiningSlopeCurve::calculate(1.0f - health) * distance);

        // A request is never copied out, so its sender only matches a target without occupant.
        int sender = -1;
        if (reported && report.m_senderPlayerId == occupantId && kit == 5)
            sender = report.m_senderPlayerId;
        else if (requested && request.m_senderPlayerId == occupantId && kit == 4)
            sender = request.m_senderPlayerId;
        if ((reported && report.m_senderPlayerId == occupantId && kit == 5)
            || (requested && request.m_senderPlayerId == occupantId && kit == 4))
        {
            if (!IBotManager::instance->getBotFromPlayerId(sender))
            {
                value = value * 3.0f;
                humanAsked = true;
            }
            else
            {
                value = value * 1.5f;
            }
        }
        if (value > bestValue)
        {
            bestValue = value;
            bestDistance = distance;
            bestNeedsHuman = needsHuman;
            bestWeapon = m_battle34[strengthType];
            best = *it;
            bestPlayerId = occupantId;
        }
    }

    const unsigned int id = bot->getBotId();
    if (best == static_cast<unsigned int>(-1))
    {
        m_urgency[id] = 0.0f;
        m_targetChanged[id] = m_targets[id] != static_cast<unsigned int>(-1);
        m_targets[id] = static_cast<unsigned int>(-1);
        m_generator->reset(bot);
        return 0.0f;
    }
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
        && bot->getOrderList(m_behaviourId)->size() == 0)
        bestValue = bestValue * bot->getMemory()->m_float8;
    if (bestNeedsHuman)
    {
        if (humanAsked && bestDistance < 50.0f)
            m_urgency[id] = 100.0f;
        else
            m_urgency[id] = DecleiningSlopeCurve::calculate(bestValue) * elapsed * 10.0f;
    }
    else
    {
        const float urgency = DecleiningSlopeCurve::calculate(bestValue) * elapsed;
        m_urgency[id] = urgency + urgency;
    }
    m_targetChanged[id] = m_targets[id] != best;
    m_targets[id] = best;
    static_cast<BBPMedicAssist*>(m_generator)->offerSupport(m_targets[id], bestWeapon, bestPlayerId, bot);
    if (kit == 5)
    {
        SquadManager* const squads = SquadManager::m_instance;
        m_urgency[id] = squads->getSupportDemand(bot->getSquadMember(), best, 1, bot->getSide()) * m_urgency[id];
    }
    else if (kit == 4)
    {
        SquadManager* const squads = SquadManager::m_instance;
        m_urgency[id] = squads->getSupportDemand(bot->getSquadMember(), best, 2, bot->getSide()) * m_urgency[id];
        if (m_urgency[id] == 0.0f)
            bot->setWantedRepairTarget(static_cast<unsigned int>(-1));
        else
            bot->setWantedRepairTarget(best);
    }
    return m_urgency[bot->getBotId()];
}

// Whether a human player occupies the object. Inlined twice into BBRevive's pass;
// [CHOSEN, not recovered] helper.
bool isHumanOccupied(Information* information)
{
    IPIUnit* const unit = information->getUnit();
    if (!unit || !unit->isOccupied())
        return false;
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(unit->getOccupyingPlayerId(information->getSide()));
    return player && !player->getIsAIPlayer();
}

// PE 10120650. Looks for downed friendly soldiers that the equipment's revive weapons (type 3)
// can reach before their man-down time runs out; humans are preferred tenfold.
float BBRevive::calculateUrgency(Bot* bot, bool flag, float elapsed)
{
    if (!flag)
    {
        Information* const target = informationManager->getInformation(m_targets[bot->getBotId()]);
        if (target && target->getSide() == bot->getSide())
        {
            m_targetChanged[bot->getBotId()] = false;
            if (!m_generator->hasPlan(bot))
                m_urgency[bot->getBotId()] = 0.0f;
        }
        else
        {
            m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
            m_urgency[bot->getBotId()] = 0.0f;
            m_generator->reset(bot);
        }
        return m_urgency[bot->getBotId()];
    }

    const int strengthCount = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNBattleStrengths();
    for (int i = 0; i < strengthCount; ++i)
    {
        m_battle34[i] = -1;
        m_battle3C[i] = 0;
    }
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = equipment->getArmament();
    const Vec3& position = equipment->getPosition();
    m_vector24.resize(0, 0);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getFriendlyUnits(m_vector24, position, bot, 40.0f);
    if (m_vector24.size() == 0)
    {
        m_targetChanged[bot->getBotId()] = m_urgency[bot->getBotId()] != 0.0f;
        m_urgency[bot->getBotId()] = 0.0f;
        m_generator->reset(bot);
        return 0.0f;
    }

    std::vector<unsigned int> weapons(armament->getWeapons());
    const int kit = bot->getPlayer()->getSpawnKit();
    for (unsigned int index = 0; weapons.begin() != weapons.end() && index < weapons.size(); ++index)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[index]);
        if (!weapon || weapon->getWeaponType() != 3)
            continue;
        for (int kind = 0; kind < strengthCount; ++kind)
        {
            if (weapon->getStrength(kind) > m_battle3C[kind])
            {
                m_battle34[kind] = static_cast<int>(index);
                m_battle3C[kind] = weapon->getStrength(kind);
                const float range = weapon->getMaxRange();
                m_battle38[kind] = range * range;
            }
        }
    }

    int bestWeapon = -1;
    unsigned int best = static_cast<unsigned int>(-1);
    float bestValue = 0.0f;
    unsigned int ordersMatched = 0;
    const float speed = equipment->getMobile()->getMaxSpeed();
    float bestDistance = 100.0f;
    for (std::vector<unsigned int>::iterator it = m_vector24.begin(); it != m_vector24.end(); ++it)
    {
        Information* const target = informationManager->getInformation(*it);
        if (!target)
            continue;
        IPIPhysical* const physical = target->getPhysical();
        if (!physical || target == equipment || !target->getMobile()
            || !((target->getObjectTypeFlags() >> 21) & 1) || !physical->getManDown()
            || target->getSoldierPose() == static_cast<SoldierPose>(4))
            continue;
        const Vec3& targetPosition = target->getPosition();
        IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
        const float water = terrain->getWaterLevel(targetPosition.x, targetPosition.z);
        const float depth = water - terrain->getHeight(targetPosition.x, targetPosition.z);
        // Under the surface in water deeper than 0.75 is out of reach.
        if (water + 0.5f > targetPosition.y && depth > 0.75f)
            continue;
        const int strengthType = physical->getStrengthType();
        if (m_battle34[strengthType] == -1)
            continue;
        const float timeLeft = physical->getManDownTimeLeft();
        if (timeLeft < 0.001f)
            continue;
        const float distance = vectorLength(vectorFrom(targetPosition, position));
        float value = 1.0f - std::min(1.0f, distance / speed / timeLeft);
        if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
            && ordersMatched < bot->getOrderList(m_behaviourId)->size())
        {
            std::list<unsigned int>* const orders = bot->getOrderList(m_behaviourId);
            if (std::find(orders->begin(), orders->end(), *it) != orders->end())
            {
                ++ordersMatched;
                value = value * bot->getMemory()->m_float8;
            }
        }
        if (isHumanOccupied(target))
            value = value * 10.0f;
        if (value > bestValue)
        {
            bestDistance = distance;
            bestWeapon = m_battle34[strengthType];
            bestValue = value;
            best = *it;
        }
    }

    const unsigned int id = bot->getBotId();
    if (best == static_cast<unsigned int>(-1))
    {
        m_urgency[id] = 0.0f;
        m_targetChanged[id] = m_targets[id] != static_cast<unsigned int>(-1);
        m_targets[id] = static_cast<unsigned int>(-1);
        m_generator->reset(bot);
        return 0.0f;
    }
    Information* const bestInformation = informationManager->getInformation(best);
    const bool human = bestInformation && isHumanOccupied(bestInformation);
    if (bot->isOrderedBehaviour(m_behaviourId) && bot->isTakingOrders()
        && bot->getOrderList(m_behaviourId)->size() == 0)
        bestValue = bestValue * bot->getMemory()->m_float8;
    float urgency;
    if (human)
    {
        if (bestDistance < 50.0f)
            urgency = 100.0f;
        else
            urgency = DecleiningSlopeCurve::calculate(bestValue) * elapsed * 5.0f;
    }
    else
    {
        urgency = DecleiningSlopeCurve::calculate(bestValue) * elapsed;
        urgency = urgency + urgency;
    }
    m_urgency[id] = urgency;
    m_targetChanged[id] = m_targets[id] != best;
    m_targets[id] = best;
    static_cast<BBPRevive*>(m_generator)->offerRevive(m_targets[id], bestWeapon, bot);
    if (kit == 5)
        m_urgency[id] = SquadManager::m_instance->getSupportDemand(bot->getSquadMember(), best, 1,
                                                                  bot->getSide())
                        * m_urgency[id];
    return m_urgency[id];
}

} // namespace ai
} // namespace hfe
} // namespace dice
