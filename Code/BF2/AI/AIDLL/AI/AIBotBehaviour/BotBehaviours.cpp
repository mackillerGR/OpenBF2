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
    m_int2C = new int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_double24[i] = 0.0;
        m_bool28[i] = false;
        m_int2C[i] = 0;
    }
    m_targets = 0;
}

BBIdle::~BBIdle()
{
    delete[] m_double24;
    delete[] m_bool28;
    delete[] m_int2C;
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
        m_int2C[i] = 0;
    }
    m_targets = 0;
}

float BBIdle::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBIdle::calculateUrgency");
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

float BBFire::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFire::calculateUrgency");
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
