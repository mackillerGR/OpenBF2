// Plan generators without an attested source of their own; see PlanGenerators.h.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../Main/AIDllFrontier.h"
#include "../AIBotActionPlan/BAPConditions.h"
#include "../AIBotActionPlan/BAPStatements.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIBot/WaypointList.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/checkPointer.h"
#include "../AIBot/BotNavigation.h"
#include "../Main/AIRandom.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../Game/Objects/IPlayer.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SensingAgent.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/IBotManager.h"
#include <cmath>
#include <vector>
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

static void releaseScriptState(Bot* bot);
namespace {
void dropPlan(Bot* bot);   // defined with the waypoint helpers

// Component-wise equality within FLT_EPSILON. PE 100D2910.
bool isSamePosition(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= 1.1920929e-07f && std::fabs(a.y - b.y) <= 1.1920929e-07f
        && std::fabs(a.z - b.z) <= 1.1920929e-07f;
}

// The shared zero vector. PE 101E5390.
const Vec3& zeroVector()
{
    static const Vec3 zero = {0.0f, 0.0f, 0.0f};
    return zero;
}

// The conditional Vec3 normaliser. PE 1006EEC0.
void normaliseVector(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f)) {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
}

// The conditional Vec2 normaliser. PE 100E9DE0.
void normaliseFlat(Vec2& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f)) {
        value.x = value.y = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
}
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-generators] Each generate
// body below stops by name until its retail body is recovered and a bot runs the plan it builds.

// ---- Avoid-collision family ----------------------------------------------------------------

AvoidCollisionPlanGenerator::AvoidCollisionPlanGenerator()
{
    const int bots = configuredBotCount();
    m_obstacles = checkPointer(new unsigned int[bots]);
    m_positions = checkPointer(newZeroedVectors(bots));
    m_distances = checkPointer(new float[bots]);
}

// PE 10198D50.
AvoidCollisionPlanGenerator::~AvoidCollisionPlanGenerator()
{
    delete[] m_obstacles;
    delete[] m_positions;
    delete[] m_distances;
}

BBPAvoidCollision2d::BBPAvoidCollision2d(int behaviourId)
{
    m_behaviourId = behaviourId;
    const int bots = configuredBotCount();
    Vec2* const directions = new Vec2[bots];
    for (int i = 0; i < bots; ++i)
    {
        directions[i].x = 0.0f;
        directions[i].y = 0.0f;
    }
    m_directions = checkPointer(directions);
}

BBPAvoidCollision2d::~BBPAvoidCollision2d()
{
    delete[] m_directions;
}

// The direction keeps its x and z components.
void BBPAvoidCollision2d::setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                                      const Vec3& position, float distance)
{
    m_hasPlan[bot->getBotId()] = true;
    Vec2& ground = m_directions[bot->getBotId()];
    ground.x = direction.x;
    ground.y = direction.z;
    m_obstacles[bot->getBotId()] = obstacle;
    m_positions[bot->getBotId()] = position;
    m_distances[bot->getBotId()] = distance;
}

BBPAvoidCollision3d::BBPAvoidCollision3d(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_directions = checkPointer(newZeroedVectors(configuredBotCount()));
}

BBPAvoidCollision3d::~BBPAvoidCollision3d()
{
    delete[] m_directions;
}

void BBPAvoidCollision3d::setObstacle(Bot* bot, const Vec3& direction, unsigned int obstacle,
                                      const Vec3& position, float distance)
{
    m_hasPlan[bot->getBotId()] = true;
    m_directions[bot->getBotId()] = direction;
    m_obstacles[bot->getBotId()] = obstacle;
    m_positions[bot->getBotId()] = position;
    m_distances[bot->getBotId()] = distance;
}

BBPAvoidCollisionInfantery::BBPAvoidCollisionInfantery(int behaviourId)
{
    m_behaviourId = behaviourId;
    const int bots = configuredBotCount();
    Vec2* const directions = new Vec2[bots];
    for (int i = 0; i < bots; ++i)
    {
        directions[i].x = 0.0f;
        directions[i].y = 0.0f;
    }
    m_directions = checkPointer(directions);
}

BBPAvoidCollisionInfantery::~BBPAvoidCollisionInfantery()
{
    delete[] m_directions;
}

void BBPAvoidCollisionInfantery::setObstacle(Bot* bot, const Vec3& direction,
                                             unsigned int obstacle, const Vec3& position,
                                             float distance)
{
    m_hasPlan[bot->getBotId()] = true;
    Vec2& ground = m_directions[bot->getBotId()];
    ground.x = direction.x;
    ground.y = direction.z;
    m_obstacles[bot->getBotId()] = obstacle;
    m_positions[bot->getBotId()] = position;
    m_distances[bot->getBotId()] = distance;
}

BotScript* BBPAvoidCollision2d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPAvoidCollision2d::generate");
}

BotScript* BBPAvoidCollision3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPAvoidCollision3d::generate");
}

// PE 100EF0C0. Steps a soldier aside: past a moving obstacle (perpendicular to its motion, or
// its facing when it stands still, on the bot's side, 10 away), or, with no obstacle (range 15)
// or one without a direction, back from the point to avoid or to one side or the other (a coin
// toss) wherever the pathfinding trace first comes back clear. The step runs straight (no path
// search) until it ends within 2 of the traced point or the time runs out: the obstacle's stored
// distance read as seconds, or with no obstacle the navigation's avoid time (5 s when gassed).
// The sensing agent is overridden and the soldier's inputs merged while it moves, between a
// turn lock and its release. Without an obstacle, a navigation avoid position or gas, no plan.
BotScript* BBPAvoidCollisionInfantery::generate(Bot* bot)
{
    Vec2 flatHit;
    flatHit.x = flatHit.y = 0.0f;
    Vec2 flatTarget;
    flatTarget.x = flatTarget.y = 0.0f;
    world::IPlayer* const player = bot->getPlayer()->getPlayer();
    if (!m_hasPlan[bot->getBotId()])
        return 0;
    bot->getSquadMember()->clearRequest();
    dropPlan(bot);
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPAvoidCollisionInfantery::generate (script debugger reset)");
    bot->getScriptContext()->clear();
    const bool noObstacle = m_obstacles[bot->getBotId()] == static_cast<unsigned int>(-1);
    Information* const obstacle = informationManager->getInformation(m_obstacles[bot->getBotId()]);
    Information* const own = informationManager->getInformation(bot->getCurrentEquipment());   // not checked
    IPIMobile* const ownMobile = own->getMobile();
    const Vec3& position = own->getPosition();
    const Vec2 flatOwn = { position.x, position.z };   // PE 1014ADC0
    double timeout = 0.0;   // PE 1026CB40
    Vec3 avoid = {0.0f, 0.0f, 0.0f};
    const Vec3* point;
    if (obstacle) {
        point = &obstacle->getPosition();
    } else if (player->getIsGassed()) {
        // Retail flees from the player's gas point (engine Player floats +0x330..+0x338, plus 1
        // on each axis) for 5 s. The field has no accessor in this tree.
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: player-gas-point]
        BF2_AI_FRONTIER("dice::hfe::ai::BBPAvoidCollisionInfantery::generate (gas point)");
    } else {
        if (!static_cast<BotMain*>(bot)->getBotNavigation()->getAvoidPosition(avoid, timeout))
            return 0;
        point = &avoid;
    }
    flatTarget.x = point->x;
    flatTarget.y = point->z;
    const float range = noObstacle ? 15.0f : 10.0f;
    own->getMobile()->getVehicleNumber();   // read and dropped, as shipped
    const unsigned long long driveControls = own->getControlInfo()->getDriveControls();
    Vec3 hit = {0.0f, 0.0f, 0.0f};
    const float maxSpeed = ownMobile->getMaxSpeed();
    const unsigned int draw = AIRandom::m_instance->next();
    float side = 1.0f;
    if (!(static_cast<float>(draw) * 2.3283064365386963e-10f > 0.5f))
        side = -1.0f;
    const int equipmentType = own->getUnit()->getEquipmentType();
    bool noDirection = false;
    if (!noObstacle) {
        const Vec2& direction = m_directions[bot->getBotId()];
        noDirection = !(std::fabs(direction.y) + std::fabs(direction.x) > 1.1920928955078125e-07f);
    }
    Vec3 step = {0.0f, 0.0f, 0.0f};
    unsigned int handle;
    BotNavigation* navigation;
    if (!noDirection && !noObstacle) {
        Vec3 motion = obstacle->getMobile()->getSpeed();
        if (isSamePosition(motion, zeroVector())) {
            const Vec3 facing = obstacle->getFaceing();
            motion.x = facing.x;
            motion.z = facing.z;
        }
        const Vec3& obstaclePosition = obstacle->getPosition();
        const Vec3 away = {position.x - obstaclePosition.x, position.y - obstaclePosition.y,
                           position.z - obstaclePosition.z};
        Vec3 across = {motion.z, 0.0f, -motion.x};
        normaliseVector(across);   // PE 1006EEC0
        across.x *= range;
        across.y *= range;
        across.z *= range;
        if (away.z * across.z + across.x * away.x + across.y * 0.0f > 0.0f) {
            step.x = across.x + position.x;
            step.y = across.y + position.y;
            step.z = across.z + position.z;
        } else {
            step.x = position.x - across.x;
            step.y = position.y - across.y;
            step.z = position.z - across.z;
        }
        navigation = static_cast<BotMain*>(bot)->getBotNavigation();
        IAIPathfinding::instance->trace(navigation, false, step, hit, handle, equipmentType, -1, 0.0f, 0, 0);
        timeout = m_distances[bot->getBotId()];
    } else {
        Vec2 toward = { flatTarget.x - flatOwn.x, flatTarget.y - flatOwn.y };
        normaliseFlat(toward);   // PE 100E9DE0
        const Vec2 across = { toward.y * range, -toward.x * range };
        const Vec2 back = { -toward.x * range, -toward.y * range };
        step = position;
        step.x += back.x;   // PE 1014AE80
        step.z += back.y;
        navigation = static_cast<BotMain*>(bot)->getBotNavigation();
        if (!IAIPathfinding::instance->trace(navigation, false, step, hit, handle, equipmentType, -1, 0.0f, 0, 0)) {
            step = position;
            step.x += across.x * side;
            step.z += across.y * side;
            navigation = static_cast<BotMain*>(bot)->getBotNavigation();
            if (!IAIPathfinding::instance->trace(navigation, false, step, hit, handle, equipmentType, -1, 0.0f, 0,
                                                 0)) {
                const float other = -side;
                step = position;
                step.x += across.x * other;
                step.z += across.y * other;
                navigation = static_cast<BotMain*>(bot)->getBotNavigation();
                IAIPathfinding::instance->trace(navigation, false, step, hit, handle, equipmentType, -1, 0.0f, 0, 0);
            }
        }
        if (!noObstacle)
            timeout = m_distances[bot->getBotId()];
    }
    BAPConTimer* const timer = checkPointer(new BAPConTimer(timeout, false, false, false));
    flatHit.x = hit.x;   // PE 1014ADC0
    flatHit.y = hit.z;
    BAPConPosition* const arrived = checkPointer(new BAPConPosition(flatHit, 2.0f));
    BAPConOr* const done = checkPointer(new BAPConOr);
    done->addCondition(timer);
    done->addCondition(arrived);
    BAPMoveToPoint* const move = checkPointer(new BAPMoveToPoint(bot, hit, maxSpeed, maxSpeed, done, driveControls,
                                                                 m_behaviourId));
    move->setFocus(bot, true, hit, static_cast<unsigned int>(-1));
    BAPSerial* const serial = checkPointer(new BAPSerial(m_behaviourId, -1));
    serial->addStatement(checkPointer(new BAPICTurnLock(true)));
    BAPParallel* const together = checkPointer(new BAPParallel(m_behaviourId, true, -1, false));
    together->addStatement(checkPointer(new BAPOverrideSensingAgent));
    together->addStatement(move);
    together->addStatement(checkPointer(new BAPMergeSoldierInputs(m_behaviourId)));
    serial->addStatement(together);
    serial->addStatement(checkPointer(new BAPICTurnLock(false)));
    serial->m_owner = this;
    return serial;
}

// ---- Waypoint family -----------------------------------------------------------------------

GotoWaypointPlanGenerator::~GotoWaypointPlanGenerator()
{
}

BBPGotoWaypoint2d::BBPGotoWaypoint2d(int behaviourId)
{
    m_behaviourId = behaviourId;
}

void BBPGotoWaypoint2d::setWaypoint(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

namespace {

BAPUseFlareOnMissileLock* newFlareStatement(Bot* bot, int id);   // defined with BBPChange
void dropPlan(Bot* bot);                                          // defined below
void clearPlan(Bot* bot);                                         // defined below

// The common tail of the land-vehicle waypoint plans (PE 100F3860): the serial gets the move,
// then a parallel runs a flare-on-lock statement beside it, wrapped as the MoveTo behaviour at
// the waypoint.
BotScript* finishVehicleWaypointPlan(Bot* bot, BAPSerial* serial, BotScript* move, const Vec3& position,
                                     int behaviourId, PlanGenerator* owner)
{
    serial->addStatement(move);
    BAPParallel* const parallel = checkPointer(new BAPParallel(behaviourId, true, -1, false));
    parallel->addStatement(newFlareStatement(bot, behaviourId));
    parallel->addStatement(serial);
    BAPMoveToWrapper* const wrapper = checkPointer(new BAPMoveToWrapper(position, parallel, behaviourId));
    wrapper->m_owner = owner;
    return wrapper;
}

// The serial that first turns the sensing agent towards the waypoint (PE 100F3860).
BAPSerial* newVehicleWaypointSerial(Information* equipment, const Vec3& position, int behaviourId)
{
    BAPSerial* const serial = checkPointer(new BAPSerial(behaviourId, -1));
    const Vec3& from = equipment->getPosition();
    Vec3 direction;
    direction.x = position.x - from.x;
    direction.y = position.y - from.y;
    direction.z = position.z - from.z;
    serial->addStatement(checkPointer(new BAPSetSensingAgentFocus(0.5f, direction, behaviourId, false)));
    return serial;
}

// Type-0 waypoint lists would hold an object handle (+0x10) and get the MoveToObject plan
// (PE 100F3CDA..100F3F7E and 100F428C..100F4410). No waypoint-list class in the image reports
// type 0, so the branch is unreachable in retail; it stops by name rather than invent the class.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-move-to-object] Recover the list
// class if one is found and build the plan as those ranges do.
BotScript* vehicleObjectWaypointPlan()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypoint2d::generate (object waypoint)");
}

// The 2-component flat part of a position, read and dropped by the waypoint generators.
Vec2 waypointFlat(const Vec3& position)   // PE 1014ADC0
{
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;
    return flat;
}

} // anonymous namespace

// PE 100F3860. The land-vehicle form of the waypoint plan. A current MoveTo plan of this
// behaviour is kept while its waypoint list does not ask for a new one, and for a type-2 list
// unless the waypoint came within four times the squad member's speed of the planned position
// along x (the planned x is compared with the waypoint's x twice, as shipped). Position
// waypoints (types 1 and 2) get a path-following plan within the list's tolerance, halved for
// type-1 lists; object waypoints (type 0) stop by name; other lists get none. Every plan drops flares on a missile lock. The equipment
// is not null-checked, as shipped.
BotScript* BBPGotoWaypoint2d::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        releaseScriptState(bot);
        dropPlan(bot);
        return 0;
    }
    const float speed = bot->getSquadMember()->getMaxSpeed();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const control = equipment->getControlInfo();

    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId)
    {
        if (!bot->getWaypoints()->isActive() || bot->getWaypoints()->isPaused())
            return bot->getPlan();
        bot->getWaypoints()->setActive(false);
        if (static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 2)
        {
            WaypointList* const waypoints = bot->getWaypoints();
            const unsigned long long inputMask = control->getDriveControls();
            // Only the object-waypoint branch uses the range.
            const float range = equipment->getPhysical()->getRadius() * 4.0f;
            (void)range;
            switch (waypoints->getType())
            {
            case 0:
                return vehicleObjectWaypointPlan();
            case 1:
            {
                Vec3 position;
                position.x = position.y = position.z = 0.0f;
                if (!waypoints->getCurrentWaypoint(position))
                    return 0;
                waypointFlat(position);
                clearPlan(bot);
                releaseScriptState(bot);
                BAPSerial* const serial = newVehicleWaypointSerial(equipment, position, m_behaviourId);
                BAPMoveToFinding* const move = checkPointer(new BAPMoveToFinding(
                    bot, position, speed, speed, 0, inputMask, m_behaviourId, waypoints->getTolerance() * 0.5f,
                    0.0f));
                return finishVehicleWaypointPlan(bot, serial, move, position, m_behaviourId, this);
            }
            case 2:
            {
                Vec3 position;
                position.x = position.y = position.z = 0.0f;
                if (!waypoints->getCurrentWaypoint(position))
                    return 0;
                const Vec2 flat = waypointFlat(position);
                const float first = static_cast<BAPMoveToWrapper*>(bot->getPlan())->m_position.x - flat.x;
                const float second = static_cast<BAPMoveToWrapper*>(bot->getPlan())->m_position.x - flat.x;
                const float reach = speed * 4.0f;
                if (reach * reach < second * second + first * first)
                    return bot->getPlan();
                clearPlan(bot);
                releaseScriptState(bot);
                BAPSerial* const serial = newVehicleWaypointSerial(equipment, position, m_behaviourId);
                BAPMoveToFinding* const move = checkPointer(new BAPMoveToFinding(
                    bot, position, speed, speed, 0, inputMask, m_behaviourId, waypoints->getTolerance(), 0.0f));
                return finishVehicleWaypointPlan(bot, serial, move, position, m_behaviourId, this);
            }
            default:
                dropPlan(bot);
                return 0;
            }
        }
        clearPlan(bot);
    }
    else
    {
        dropPlan(bot);
    }
    releaseScriptState(bot);
    if (bot->getWaypoints()->isPaused())
        return 0;
    WaypointList* const waypoints = bot->getWaypoints();
    const unsigned long long inputMask = control->getDriveControls();
    const float range = equipment->getPhysical()->getRadius() * 4.0f;
    (void)range;
    bot->getWaypoints()->setActive(false);
    bot->getSquadMember()->clearRequest();
    Vec3 position;
    position.x = position.y = position.z = 0.0f;
    switch (waypoints->getType())
    {
    case 0:
        return vehicleObjectWaypointPlan();
    case 1:
    {
        if (!waypoints->getCurrentWaypoint(position))
            return 0;
        if (!waypoints->getCurrentWaypoint(position))
            return 0;
        BAPSerial* const serial = newVehicleWaypointSerial(equipment, position, m_behaviourId);
        waypointFlat(position);
        BAPMoveToFinding* const move = checkPointer(new BAPMoveToFinding(
            bot, position, speed, speed, 0, inputMask, m_behaviourId, waypoints->getTolerance() * 0.5f, 0.0f));
        return finishVehicleWaypointPlan(bot, serial, move, position, m_behaviourId, this);
    }
    case 2:
    {
        if (!waypoints->getCurrentWaypoint(position))
            return 0;
        BAPSerial* const serial = newVehicleWaypointSerial(equipment, position, m_behaviourId);
        waypointFlat(position);
        BAPMoveToFinding* const move = checkPointer(new BAPMoveToFinding(
            bot, position, speed, speed, 0, inputMask, m_behaviourId, waypoints->getTolerance(), 0.0f));
        return finishVehicleWaypointPlan(bot, serial, move, position, m_behaviourId, this);
    }
    default:
        return 0;
    }
}

BBPGotoWaypointBoat::BBPGotoWaypointBoat(int behaviourId)
{
    m_behaviourId = behaviourId;
}

void BBPGotoWaypointBoat::setWaypoint(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

BotScript* BBPGotoWaypointBoat::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypointBoat::generate");
}

BBPGotoWaypoint3d::BBPGotoWaypoint3d(int behaviourId)
{
    m_behaviourId = behaviourId;
}

void BBPGotoWaypoint3d::setWaypoint(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

BotScript* BBPGotoWaypoint3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypoint3d::generate");
}

BBPGotoWaypointInfantery::BBPGotoWaypointInfantery(int behaviourId)
{
    m_behaviourId = behaviourId;
}

void BBPGotoWaypointInfantery::setWaypoint(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

namespace {

// Type-0 waypoint lists hold an object handle (+0x10) and get the MoveToObject plan (PE ctor
// 101A6130) towards it, rebuilt when the object moved by more than four times the speed. Neither
// that list class nor BAPMoveToObject is recovered yet, so this branch stops by name.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-move-to-object] Recover both and
// build the plan as PE 100F1AC3..100F1C4C and 100F1E37..100F1F1C do.
BotScript* objectWaypointPlan()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypointInfantery::generate (object waypoint)");
}

// The position-waypoint plan: a serial that first turns the sensing agent towards the waypoint
// and then follows the path there, wrapped as the MoveTo behaviour.
BotScript* buildPositionWaypointPlan(Bot* bot, Information* equipment, const Vec3& position, float speed,
                                     unsigned long long inputMask, float tolerance, int behaviourId,
                                     PlanGenerator* owner)
{
    BAPMoveToFinding* const move = checkPointer(new BAPMoveToFinding(
        bot, position, speed, speed, 0, inputMask, behaviourId, tolerance, 0.0f));
    BAPSerial* const serial = checkPointer(new BAPSerial(behaviourId, -1));
    const Vec3& from = equipment->getPosition();
    Vec3 direction;
    direction.x = position.x - from.x;
    direction.y = position.y - from.y;
    direction.z = position.z - from.z;
    BAPSetSensingAgentFocus* const focus =
        checkPointer(new BAPSetSensingAgentFocus(0.5f, direction, behaviourId, false));
    serial->addStatement(focus);
    serial->addStatement(move);
    BAPMoveToWrapper* const wrapper = checkPointer(new BAPMoveToWrapper(position, serial, behaviourId));
    wrapper->m_owner = owner;
    return wrapper;
}

// Deletes the plan and clears it when the bot has one.
void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        if (bot->getPlan())
            delete bot->getPlan();
        bot->setPlan(0);
    }
}

// Deletes any plan and clears it unconditionally.
void clearPlan(Bot* bot)
{
    if (BotScript* const plan = bot->getPlan())
        delete plan;
    bot->setPlan(0);
}

} // anonymous namespace

// PE 100F1770. A current MoveTo plan of this behaviour is kept until its waypoint list asks
// for a new one and, for type-2 lists, the waypoint moved by more than four times the squad
// member's speed (type-1 lists always rebuild). Position waypoints (types 1 and 2) get a path-following
// plan; object waypoints (type 0) the MoveToObject plan; other lists none. The equipment is not
// null-checked where its position is read.
BotScript* BBPGotoWaypointInfantery::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        releaseScriptState(bot);
        dropPlan(bot);
        return 0;
    }

    const float speed = bot->getSquadMember()->getMaxSpeed();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());

    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId)
    {
        if (!bot->getWaypoints()->isActive() || bot->getWaypoints()->isPaused())
            return bot->getPlan();
        bot->getWaypoints()->setActive(false);

        if (static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 2)
        {
            WaypointList* const waypoints = bot->getWaypoints();
            const unsigned long long inputMask = equipment->getControlInfo()->getDriveControls();
            // Only the object-waypoint branch uses the radius.
            const float radius = equipment->getPhysical()->getRadius() * 4.0f;
            (void)radius;
            Vec3 position;
            position.x = position.y = position.z = 0.0f;
            switch (waypoints->getType())
            {
            case 0:
                return objectWaypointPlan();
            case 1:
            {
                if (!waypoints->getCurrentWaypoint(position))
                    return 0;
                clearPlan(bot);
                releaseScriptState(bot);
                return buildPositionWaypointPlan(bot, equipment, position, speed, inputMask,
                                                 waypoints->getTolerance(), m_behaviourId, this);
            }
            case 2:
            {
                if (!waypoints->getCurrentWaypoint(position))
                    return 0;
                const Vec3& planned = static_cast<BAPMoveToWrapper*>(bot->getPlan())->m_position;
                const float dx = planned.x - position.x;
                const float dz = planned.z - position.z;
                const float reach = speed * 4.0f;
                if (!(reach * reach < dx * dx + dz * dz))
                    return bot->getPlan();
                clearPlan(bot);
                releaseScriptState(bot);
                return buildPositionWaypointPlan(bot, equipment, position, speed, inputMask,
                                                 waypoints->getTolerance(), m_behaviourId, this);
            }
            default:
                dropPlan(bot);
                return 0;
            }
        }
        clearPlan(bot);
    }
    else
    {
        dropPlan(bot);
    }
    releaseScriptState(bot);
    if (bot->getWaypoints()->isPaused())
        return 0;

    WaypointList* const waypoints = bot->getWaypoints();
    const unsigned long long inputMask = equipment->getControlInfo()->getDriveControls();
    const float radius = equipment->getPhysical()->getRadius() * 4.0f;
    (void)radius;
    bot->getSquadMember()->clearRequest();
    Vec3 position;
    position.x = position.y = position.z = 0.0f;
    switch (waypoints->getType())
    {
    case 0:
        return objectWaypointPlan();
    case 1:
    case 2:
        if (!waypoints->getCurrentWaypoint(position))
            return 0;
        return buildPositionWaypointPlan(bot, equipment, position, speed, inputMask, waypoints->getTolerance(),
                                         m_behaviourId, this);
    default:
        return 0;
    }
}

// ---- Assistance, change and the single-array generators -----------------------------------

BBPMedicAssist::BBPMedicAssist(int behaviourId)
{
    m_behaviourId = behaviourId;
    const int bots = configuredBotCount();
    m_int0C = checkPointer(new int[bots]);
    m_int10 = checkPointer(new int[bots]);
    m_int14 = checkPointer(new int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_int0C[i] = -1;
        m_int10[i] = -1;
        m_int14[i] = -1;
    }
}

BBPMedicAssist::~BBPMedicAssist()
{
    delete[] m_int0C;
    delete[] m_int10;
    delete[] m_int14;
}

// PE 100FEBF0.
void BBPMedicAssist::offerSupport(unsigned int target, int weapon, int playerId, Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
    m_int0C[bot->getBotId()] = static_cast<int>(target);
    m_int10[bot->getBotId()] = weapon;
    m_int14[bot->getBotId()] = playerId;
}

// PE 100FF370. The side is handed to every occupant lookup.
bool BBPMedicAssist::hasHumanOccupant(Information* object, int side)
{
    if (!object)
        return false;
    Information* const primary = informationManager->getInformation(object->getPrimaryObject(side));
    if (!primary)
        return false;
    IAIPlayerFactory* const players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    IPIUnit* unit = primary->getUnit();
    if (unit && unit->isOccupied())
    {
        IAIPlayer* const player = players->getPlayerFromId(unit->getOccupyingPlayerId(side));
        if (player && !player->getIsAIPlayer())
            return true;
    }
    int iterator;
    Information* part = informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
    while (part)
    {
        unit = part->getUnit();
        if (unit && unit->isOccupied())
        {
            IAIPlayer* const player = players->getPlayerFromId(unit->getOccupyingPlayerId(side));
            if (player && !player->getIsAIPlayer())
                return true;
        }
        part = informationManager->getInformation(primary->getSecondaryObject(iterator, side));
    }
    return false;
}

namespace {

// PE 102C05C0: a zero-initialised float that only BBPMedicAssist::generate reads, handed to its
// look statements as their height offset. Nothing in the image writes it. [UNRESOLVED]
float s_medicLookHeightOffset = 0.0f;

// PE 101ADCF0 (register-passed weapons and preferred index in the PE). Picks the weapon with
// the greatest strength against the strength type among those whose type flags match exactly;
// weapons with no ammunition qualify only when one type flag is set. Weapons in range (strictly
// between the minimum and maximum range) beat any out of range; the preferred index counts
// triple once chosen. -1 when none qualifies. [INFERRED] name.
int chooseSupportWeapon(const std::vector<unsigned int>& weapons, int preferred, float distance,
                        int strengthType, bool skipIndirect, bool wantType1, bool wantType3,
                        bool wantType2)
{
    int best = -1;
    int bestStrength = 0;
    bool inRangeFound = false;
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon)
            continue;
        if (wantType3 != (weapon->getWeaponType() == 3))
            continue;
        if (wantType1 != (weapon->getWeaponType() == 1))
            continue;
        if (wantType2 != (weapon->getWeaponType() == 2))
            continue;
        if (skipIndirect && weapon->getIndirect())
            continue;
        if (weapon->getAmmo() == 0 && !wantType2 && !wantType1 && !wantType3)
            continue;
        if (weapon->getStrength(strengthType) <= 0)
            continue;
        if (weapon->getMaxRange() > distance && weapon->getMinRange() < distance)
        {
            if (inRangeFound)
            {
                if (bestStrength > weapon->getStrength(strengthType))
                    continue;
            }
            else
            {
                inRangeFound = true;
            }
        }
        else
        {
            if (inRangeFound)
                continue;
            if (bestStrength >= weapon->getStrength(strengthType))
                continue;
        }
        best = static_cast<int>(i);
        bestStrength = weapon->getStrength(strengthType);
        if (static_cast<int>(i) == preferred)
            bestStrength *= 3;
    }
    return best;
}

unsigned long long singleInputMask(int input)
{
    return static_cast<unsigned long long>(static_cast<long long>(1 << input));
}

} // anonymous namespace

// PE 100FF770. Builds the support plan of a medic (kit 5), engineer (kit 4) or support soldier
// (kit 3) for the offered target: approach it, keep the chosen weapon on it while it is in
// reach and look at it, until it is gone or healed, repaired or rearmed. A current fire plan of
// this behaviour on the same target is kept and only answers the target's radio request. When
// the bot's equipment is the target itself the plan only triggers the weapon. The static-target
// branch moves to the entry position of the bot's own equipment, and the look-and-fire statements
// built for targets without a strength type and the alternative-weapon trigger are never added
// (and leak), as shipped.
BotScript* BBPMedicAssist::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPMedicAssist::generate (script debugger reset)");
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }

    const int kit = bot->getPlayer()->getSpawnKit();
    CommunicationAgent* const communication = bot->getCommunicationAgent();
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId
        && bot->getPlan()->getType() == static_cast<BAPStatementType>(15)
        && static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 0
        && static_cast<BAPFireWrapper*>(bot->getPlan())->m_target
               == static_cast<unsigned int>(m_int0C[bot->getBotId()]))
    {
        const int player = m_int14[bot->getBotId()];
        if (player >= 0)
        {
            int request;
            if (kit == 5)
                request = 3;
            else if (kit == 4)
                request = 4;
            else if (kit == 3)
                request = 10;
            else
                return bot->getPlan();
            if (communication->handleMessages(static_cast<AIRadioMessageType>(request), player))
                bot->getCommunicationAgent()->enqueueRadioMessage(6, 6, 3, 1.0f, -1);
        }
        return bot->getPlan();
    }

    bot->getBotId();   // read and dropped, as shipped
    bot->getSquadMember()->clearRequest();
    bot->getSensingAgent()->clearPlanTargetInformation();
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPMedicAssist::generate (script debugger reset)");
    dropPlan(bot);
    bot->getScriptContext()->clear();

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_int0C[bot->getBotId()]);
    if (!equipment || !equipment->getUnit())
        return 0;
    IPIMobile* const mobile = equipment->getMobile();
    if (!mobile || !target)
        return 0;
    IPIArmament* const armament = equipment->getArmament();
    IPIControlInfo* const control = equipment->getControlInfo();
    IPIPhysical* const physical = equipment->getPhysical();
    IPIPhysical* const targetPhysical = target->getPhysical();

    const bool supportKit = bot->getPlayer()->getSpawnKit() == 3;
    const int strengthType = targetPhysical->getStrengthType();
    const Vec3& targetPosition = target->getPosition();
    const Vec3& own = equipment->getPosition();
    Vec3 toTarget;
    toTarget.x = targetPosition.x - own.x;
    toTarget.y = targetPosition.y - own.y;
    toTarget.z = targetPosition.z - own.z;
    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    const int weaponIndex = chooseSupportWeapon(armament->getWeapons(), m_int10[bot->getBotId()],
                                                distance, strengthType, false, !supportKit, false,
                                                supportKit);
    if (weaponIndex == -1)
    {
        equipment->getAIObject()->update();
        return 0;
    }
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));

    BAPSerial* const serial = checkPointer(new BAPSerial(m_behaviourId, -1));
    const float reach = targetPhysical->getSmallestRadius() + physical->getSmallestRadius();
    bool targetIsFree;

    if (equipment != target)
    {
        const bool noStrengthType = targetPhysical->getStrengthType() == 0;
        const float range = weapon->getReplenishRadius() * 0.5f + reach;
        IPIUnit* const targetUnit = target->getUnit();

        // Whether the target stays put, so the plan heads for a position instead of the object.
        bool targetIsStatic = true;
        bool decided = false;
        if (targetUnit)
        {
            if (!targetUnit->useNoPathfindingToGetToObject() && target->getMobile())
            {
                targetIsStatic = false;
                decided = true;
            }
        }
        if (!decided && !target->isPrimary())
        {
            // The primary object is not null-checked, as shipped.
            Information* const primary =
                informationManager->getInformation(target->getPrimaryObject(bot->getSide()));
            targetIsStatic = primary->getMobile() == 0;
        }

        BotScriptValue<WeaponInfo*>* const weaponValue = bot->getScriptContext()->createVariable(weapon, 0);
        bot->getScriptContext()->createVariable(0, 0);

        BAPConAnd* const stop = checkPointer(new BAPConAnd);
        stop->addCondition(checkPointer(new BAPConObjectExists(m_int0C[bot->getBotId()])));
        if (kit == 3)
        {
            if (hasHumanOccupant(target, bot->getSide()))
                stop->addCondition(checkPointer(new BAPConTotalWeaponAmmo(m_int0C[bot->getBotId()], 0.95f)));
            else
                stop->addCondition(checkPointer(new BAPConTotalWeaponAmmo(m_int0C[bot->getBotId()], 0.75f)));
        }
        else
        {
            if (hasHumanOccupant(target, bot->getSide()))
                stop->addCondition(checkPointer(new BAPConObjectHealthBelow(m_int0C[bot->getBotId()], 0.99f)));
            else
                stop->addCondition(checkPointer(new BAPConObjectHealthBelow(m_int0C[bot->getBotId()], 0.75f)));
        }

        BAPParallel* const root = checkPointer(new BAPParallel(m_behaviourId, -1, false));
        root->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));
        root->addStatement(checkPointer(new BAPOverrideSensingAgent));
        if (!targetIsStatic)
        {
            root->addStatement(checkPointer(new BAPMoveToObjectFinding(
                bot, m_int0C[bot->getBotId()], reach, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
                control->getDriveControls(), m_behaviourId, 0, 0.0f, -1)));
        }
        else
        {
            const Vec3& entry = equipment->getUnit()->getEntryPositionOfNonMobile();
            Vec3 position;
            position.x = entry.x;
            position.y = entry.y;
            position.z = entry.z;
            root->addStatement(checkPointer(new BAPMoveToFinding(
                bot, position, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
                control->getDriveControls(), m_behaviourId, 0.5f, 0.0f)));
        }
        root->addStatement(checkPointer(new BAPALookAtObject(
            bot, m_int0C[bot->getBotId()], 0.0f, checkPointer(new BAPConFalse),
            equipment->getControlInfo()->getLookControls(), m_behaviourId, 0, s_medicLookHeightOffset)));
        if ((target->getObjectTypeFlags() >> 21 & 1) && (kit == 5 || kit == 3))
        {
            root->addStatement(checkPointer(new BAPSoldierPose(
                1, checkPointer(new BAPConTrue), m_behaviourId,
                static_cast<unsigned int>(m_int0C[bot->getBotId()]), true)));
        }
        else
        {
            root->addStatement(checkPointer(
                new BAPSoldierPose(1, checkPointer(new BAPConTrue), m_behaviourId, true)));
        }

        BAPConAnd* const inReach = checkPointer(new BAPConAnd);
        inReach->addCondition(checkPointer(
            new BAPConObjectDistance(m_int0C[bot->getBotId()], range, false, false, false)));

        BAPParallel* const fire = checkPointer(new BAPParallel(m_behaviourId, -1, false));
        fire->addStatement(checkPointer(new BAPTrigger(
            weapon->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(weapon->getWeaponActivate()), m_behaviourId, 0.0, false)));
        BAPOverrideSensingAgent* const override = checkPointer(new BAPOverrideSensingAgent);
        BAPParallel* const aim = checkPointer(new BAPParallel(m_behaviourId, -1, false));
        if (!noStrengthType)
        {
            aim->addStatement(override);
            aim->addStatement(checkPointer(new BAPALookAtObject(
                bot, m_int0C[bot->getBotId()], 0.0f, checkPointer(new BAPConFalse),
                equipment->getControlInfo()->getLookControls(), m_behaviourId, 0, s_medicLookHeightOffset)));
            BAPTriggerContinously* const trigger = checkPointer(new BAPTriggerContinously(
                weapon->getWeaponFire(), checkPointer(new BAPConWeaponActive(weaponValue)),
                checkPointer(new BAPConFalse), checkPointer(new BAPConFalse),
                singleInputMask(weapon->getWeaponFire()), m_behaviourId, 0.0));
            BAPConObjectInFront* const inFront =
                checkPointer(new BAPConObjectInFront(m_int0C[bot->getBotId()], 0.0f, 0.1f));
            aim->addStatement(checkPointer(new BAPIf(inFront, trigger, 0, m_behaviourId, true, false)));
        }
        fire->addStatement(checkPointer(new BAPWhile(checkPointer(new BAPConTrue), aim, true, m_behaviourId)));

        WeaponInfo* const alternative = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(2));
        if (alternative)
        {
            checkPointer(new BAPTrigger(
                alternative->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
                singleInputMask(alternative->getWeaponActivate()), m_behaviourId, 0.0, false));
        }

        const int player = m_int14[bot->getBotId()];
        if (player >= 0)
        {
            bool answered = false;
            bool known = true;
            if (kit == 5)
            {
                answered = communication->handleMessages(static_cast<AIRadioMessageType>(3), player);
            }
            else if (kit == 4)
            {
                answered = communication->handleMessages(static_cast<AIRadioMessageType>(4), player);
                if (Bot* const targetBot = IBotManager::instance->getBotFromPlayerId(player))
                    static_cast<BotMain*>(targetBot)->setIsEngineerTarget(true);
            }
            else if (kit == 3)
            {
                answered = communication->handleMessages(static_cast<AIRadioMessageType>(10), player);
            }
            else
            {
                known = false;
            }
            if (known && answered)
                serial->addStatement(checkPointer(new BAPSendRadioMessage(6, 6, 2, 1.0f, m_behaviourId, -1)));
        }

        BotScript* reached;
        if (!noStrengthType && !targetIsStatic)
        {
            BAPSerial* const closeIn = checkPointer(new BAPSerial(m_behaviourId, 1));
            BAPMoveToObjectFinding* const move = checkPointer(new BAPMoveToObjectFinding(
                bot, m_int0C[bot->getBotId()], 0.0f, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
                control->getDriveControls(), m_behaviourId, 0, 0.0f, -1));
            BAPConTimer* const timer = checkPointer(new BAPConTimer(0.5, false, false, false));
            closeIn->addStatement(checkPointer(
                new BAPWhile(checkPointer(new BAPConNot(timer)), move, false, m_behaviourId)));
            closeIn->addStatement(fire);
            reached = checkPointer(new BAPIf(inReach, closeIn, 0, m_behaviourId, true, false));
        }
        else
        {
            reached = checkPointer(new BAPIf(inReach, fire, 0, m_behaviourId, true, false));
        }
        root->addStatement(reached);
        root->addStatement(checkPointer(new BAPMergeSoldierInputs(m_behaviourId)));
        BAPWhile* const loop = checkPointer(new BAPWhile(stop, root, false, m_behaviourId));
        serial->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));
        serial->addStatement(loop);
        targetIsFree = targetUnit ? !targetUnit->isOccupied() : false;
    }
    else
    {
        BAPParallel* const self = checkPointer(new BAPParallel(m_behaviourId, -1, false));
        self->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));
        self->addStatement(checkPointer(new BAPSoldierPose(
            1, 3, 3, 2, 0.0f, checkPointer(new BAPConTrue), checkPointer(new BAPConFalse), m_behaviourId, true)));
        self->addStatement(checkPointer(new BAPTrigger(
            weapon->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(weapon->getWeaponActivate()), m_behaviourId, 0.0, true)));
        serial->addStatement(self);
        targetIsFree = false;
    }

    if (bot->getPlayer()->getSpawnKit() == 5)
    {
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(1, handle);
    }
    else if (bot->getPlayer()->getSpawnKit() == 4)
    {
        bot->setRepairTarget(static_cast<unsigned int>(m_int0C[bot->getBotId()]));
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(2, handle);
    }
    else if (bot->getPlayer()->getSpawnKit() == 3)
    {
        const unsigned int handle = target->getHandle();
        bot->getSquadMember()->setCurrentRequestAction(4, handle);
    }

    BAPFireWrapper* const script = checkPointer(new BAPFireWrapper(
        bot, static_cast<unsigned int>(m_int0C[bot->getBotId()]), targetIsFree, false, weaponIndex, -1,
        serial, m_behaviourId));
    script->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return script;
}

// The cover generators release the fifth array twice and never the fourth, as shipped.
BBPTakeCover::BBPTakeCover(int behaviourId)
{
    const int bots = configuredBotCount();
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[bots]);
    m_vec10 = checkPointer(newZeroedVectors(bots));
    m_vec14 = checkPointer(newZeroedVectors(bots));
    m_int18 = checkPointer(new int[bots]);
    m_int1C = checkPointer(new int[bots]);
}

BBPTakeCover::~BBPTakeCover()
{
    delete[] m_int0C;
    delete[] m_vec10;
    delete[] m_vec14;
    delete[] m_int1C;
    delete[] m_int1C;
}

BotScript* BBPTakeCover::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPTakeCover::generate");
}

BBPTakeCoverInfantry::BBPTakeCoverInfantry(int behaviourId)
{
    const int bots = configuredBotCount();
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[bots]);
    m_vec10 = checkPointer(newZeroedVectors(bots));
    m_vec14 = checkPointer(newZeroedVectors(bots));
    m_int18 = checkPointer(new int[bots]);
    m_int1C = checkPointer(new int[bots]);
}

BBPTakeCoverInfantry::~BBPTakeCoverInfantry()
{
    delete[] m_int0C;
    delete[] m_vec10;
    delete[] m_vec14;
    delete[] m_int18;
    delete[] m_int1C;
}

BotScript* BBPTakeCoverInfantry::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPTakeCoverInfantry::generate");
}

BBPChange::BBPChange(int behaviourId)
{
    const int bots = configuredBotCount();
    m_vec10 = checkPointer(newZeroedVectors(bots));
    m_bool14 = checkPointer(new bool[configuredBotCount()]);
    m_bool18 = checkPointer(new bool[configuredBotCount()]);
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPChange::~BBPChange()
{
    delete[] m_int0C;
    delete[] m_bool14;
    delete[] m_vec10;
    delete[] m_bool18;
}

// PE 100F8CE0.
void BBPChange::offerChange(unsigned int target, bool samePrimary, bool viaRequest, Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
    m_bool14[bot->getBotId()] = samePrimary;
    m_int0C[bot->getBotId()] = static_cast<int>(target);
    m_bool18[bot->getBotId()] = viaRequest;
}

// PE 100F8D40.
void BBPChange::setViaRequest(bool viaRequest, Bot* bot)
{
    m_bool18[bot->getBotId()] = viaRequest;
}

// PE 100F9280.
void BBPChange::offerChange(unsigned int target, const Vec3* entry, bool samePrimary,
                            bool viaRequest, Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
    m_bool14[bot->getBotId()] = samePrimary;
    m_int0C[bot->getBotId()] = static_cast<int>(target);
    m_vec10[bot->getBotId()] = *entry;
    m_bool18[bot->getBotId()] = viaRequest;
}

namespace {

// The radius of the object's primary object (the object itself when it is primary).
float primaryRadius(Information* object, Bot* bot)
{
    IPIPhysical* physical;
    if (object->isPrimary())
        physical = object->getPhysical();
    else
        physical = informationManager->getInformation(object->getPrimaryObject(bot->getSide()))->getPhysical();
    return physical->getRadius();
}

// Whether the target should be approached as a moving object rather than at a position.
bool approachAsMobile(Information* target, IPIUnit* targetUnit, Bot* bot)
{
    if (!targetUnit->useNoPathfindingToGetToObject() && target->getMobile())
        return true;
    if (target->isPrimary())
        return false;
    return informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->getMobile() != 0;
}

const unsigned long long kExitVehicleMask = 1ULL << 10;
const unsigned long long kFlareMask = 0x800000000000ULL;

BAPUseFlareOnMissileLock* newFlareStatement(Bot* bot, int id)
{
    // The first argument is the bot pointer in retail, stored and never read. [PLATFORM]
    return new BAPUseFlareOnMissileLock(static_cast<int>(reinterpret_cast<std::size_t>(bot)), kFlareMask, id);
}

} // anonymous namespace

// PE 100F95E0. Builds the plan that takes the bot into the offered vehicle or seat (or out of its
// vehicle into its soldier): a seat in the same vehicle is selected with its key; a stationary
// vehicle is left with the exit input; otherwise the bot drives or walks up to the target and
// changes into it, guarded by occupancy conditions. The plan is wrapped so the target stays booked
// for the bot, and a vehicle crew adds missile-lock flares. A current plan of this behaviour on
// the same target is kept and only answers the ride request.
BotScript* BBPChange::generate(Bot* bot)
{
    const int botId = bot->getBotId();
    CommunicationAgent* const communication = bot->getCommunicationAgent();
    if (!m_hasPlan[bot->getBotId()])
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPChange::generate (script debugger reset)");
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }
    if (bot->getPlan() && bot->getPlan()->getId() == m_behaviourId
        && static_cast<BAPChangeWrapper*>(bot->getPlan())->m_target == static_cast<unsigned int>(m_int0C[botId]))
    {
        if (m_bool18[botId] && communication->handleMessages(static_cast<AIRadioMessageType>(1)))
            communication->enqueueRadioMessage(6, 6, 3, 1.0f, -1);
        return bot->getPlan();
    }

    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    // The equipment is not null-checked, as shipped.
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment->getMobile();
    IPIPhysical* const physical = equipment->getPhysical();
    const unsigned long long drive = equipment->getControlInfo()->getDriveControls();
    Information* const target = informationManager->getInformation(m_int0C[bot->getBotId()]);
    if (!target)
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPChange::generate (script debugger reset)");
        dropPlan(bot);
        bot->getScriptContext()->clear();
        return 0;
    }
    IPIUnit* const targetUnit = target->getUnit();
    const float targetRadius = primaryRadius(target, bot);
    const float equipmentRadius = primaryRadius(equipment, bot);
    const float reach = equipmentRadius + targetRadius;
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPChange::generate (script debugger reset)");
    dropPlan(bot);
    bot->getScriptContext()->clear();
    bot->getSquadMember()->clearRequest();

    const bool inSoldier = (equipment->getObjectTypeFlags() >> 21 & 1) != 0;
    BotScript* body;

    if (!inSoldier)
    {
        BAPSerial* const serial = checkPointer(new BAPSerial(m_behaviourId, -1));
        IAIObject* const soldier = bot->getPlayer()->getSoldier();
        const unsigned int soldierHandle = soldier->getInformation(bot->getSide());
        if (static_cast<unsigned int>(m_int0C[bot->getBotId()]) == soldierHandle
            && communication->handleMessages(static_cast<AIRadioMessageType>(0)))
            serial->addStatement(checkPointer(new BAPSendRadioMessage(6, 7, 3, 1.0f, m_behaviourId, -1)));

        if (m_bool14[bot->getBotId()] || !equipment->getMobile())
        {
            BAPSerial* const change = checkPointer(new BAPSerial(m_behaviourId, -1));
            if (m_bool14[bot->getBotId()])
            {
                // Another seat of the same vehicle: press its select key.
                change->addStatement(checkPointer(new BAPTrigger(
                    targetUnit->getSelectKey(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
                    1ULL << targetUnit->getSelectKey(), m_behaviourId, 0.0, false)));
            }
            else
            {
                // A stationary vehicle: press exit.
                change->addStatement(checkPointer(new BAPTrigger(
                    10, checkPointer(new BAPConTrue), checkPointer(new BAPConFalse), kExitVehicleMask,
                    m_behaviourId, 0.0, false)));
            }
            change->addStatement(checkPointer(new BAPICChangeVehicle(m_int0C[bot->getBotId()])));
            serial->addStatement(change);
            body = serial;
        }
        else
        {
            Vec2 centre;
            const Vec3& first = target->getPosition();
            const Vec3& second = target->getPosition();
            centre.x = second.x;
            centre.y = first.z;
            BAPConPosition* const near = checkPointer(new BAPConPosition(centre, reach * 1.2f + 50.0f));
            const bool aircraft = (equipment->getObjectTypeFlags() >> 4 & 1) != 0;
            BotScript* approach;
            if (approachAsMobile(target, targetUnit, bot))
            {
                if (aircraft)
                {
                    approach = checkPointer(new BAPAirMoveToObject(
                        m_int0C[bot->getBotId()], 0.5f, mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.5f,
                        25.0f, near, drive, m_behaviourId));
                }
                else
                {
                    bool beside = false;
                    Vec3 position;
                    if ((target->getObjectTypeFlags() >> 4 & 1) && !(target->getObjectTypeFlags() >> 5 & 1))
                    {
                        position = target->getPosition();
                        // [FRONTIER] PE 100F9360, which looks for a free position beside the
                        // aircraft, is not recovered.
                        BF2_AI_FRONTIER("dice::hfe::ai::BBPChange::findPositionBeside (PE 100F9360)");
                    }
                    if (beside)
                    {
                        Vec2 point;
                        point.x = position.x;
                        point.y = position.z;
                        BAPConPosition* const there = checkPointer(new BAPConPosition(point, equipmentRadius));
                        approach = checkPointer(new BAPMoveToFinding(
                            bot, position, mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.5f, there, drive,
                            m_behaviourId, 0.0f, 0.0f));
                    }
                    else
                    {
                        BAPConAnd* const stop = checkPointer(new BAPConAnd);
                        stop->addCondition(checkPointer(
                            new BAPConObjectDistance(m_int0C[bot->getBotId()], 20.0f, true, false, false)));
                        stop->addCondition(checkPointer(new BAPConObjectMoved(m_int0C[bot->getBotId()], 10.0f)));
                        approach = checkPointer(new BAPMoveToObjectFinding(
                            bot, m_int0C[bot->getBotId()], 0.5f, mobile->getMaxSpeed(),
                            mobile->getMaxSpeed() * 0.5f, near, drive, m_behaviourId, stop, 0.0f, -1));
                    }
                }
            }
            else if (aircraft)
            {
                approach = checkPointer(new BAPMoveTo(
                    bot, target->getPosition(), mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.75f, near,
                    drive, 25.0f, m_behaviourId));
            }
            else
            {
                BAPConAnd* const arrived = checkPointer(new BAPConAnd);
                arrived->addCondition(near);
                arrived->addCondition(checkPointer(new BAPConObjectBehind(m_int0C[bot->getBotId()], -0.95f, false)));
                approach = checkPointer(new BAPMoveToFinding(
                    bot, m_vec10[bot->getBotId()], mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.75f, arrived,
                    drive, m_behaviourId, 0.0f, 0.0f));
            }
            serial->addStatement(approach);

            BAPUpdateVehicle* const update = checkPointer(new BAPUpdateVehicle(m_behaviourId));
            BAPConObjectEnemyOccupied* const enemy =
                checkPointer(new BAPConObjectEnemyOccupied(m_int0C[bot->getBotId()]));
            BAPTrigger* const exit = checkPointer(new BAPTrigger(
                10, checkPointer(new BAPConTrue), enemy, kExitVehicleMask, m_behaviourId, 0.0, false));
            if ((target->getObjectTypeFlags() >> 21 & 1) && equipment->isPrimary()
                && !(equipment->getObjectTypeFlags() >> 4 & 1) && !(equipment->getObjectTypeFlags() >> 5 & 1))
            {
                // Bailing out of a damaged vehicle into the soldier: press input 47 first.
                const float health = physical->getHealth();
                if (IBotManager::instance->getVehicleBailValue() > health)
                {
                    serial->addStatement(checkPointer(new BAPTrigger(
                        47, checkPointer(new BAPConTrue), checkPointer(new BAPConFalse), 1ULL << 47,
                        m_behaviourId, 0.0, false)));
                }
            }
            serial->addStatement(checkPointer(new BAPICTurnLock(true)));
            serial->addStatement(exit);
            serial->addStatement(update);
            serial->addStatement(checkPointer(new BAPICTurnLock(false)));
            BAPParallel* const both = checkPointer(new BAPParallel(m_behaviourId, -1, false));
            both->addStatement(checkPointer(new BAPICChangeVehicle(m_int0C[bot->getBotId()])));
            both->addStatement(serial);
            body = both;
        }
    }
    else
    {
        BAPParallel* const walk = checkPointer(new BAPParallel(m_behaviourId, false, -1, false));
        BAPConObjectOccupied* const occupied = checkPointer(new BAPConObjectOccupied(m_int0C[bot->getBotId()]));
        BAPConObjectEnemyOccupied* const enemy =
            checkPointer(new BAPConObjectEnemyOccupied(m_int0C[bot->getBotId()]));
        BAPConOr* const taken = checkPointer(new BAPConOr);
        taken->addCondition(occupied);
        taken->addCondition(enemy);
        BotScript* approach;
        if (approachAsMobile(target, targetUnit, bot))
        {
            BAPConAnd* const stop = checkPointer(new BAPConAnd);
            stop->addCondition(checkPointer(new BAPConObjectDistance(m_int0C[bot->getBotId()], 10.0f, true, false, false)));
            stop->addCondition(checkPointer(new BAPConObjectMoved(m_int0C[bot->getBotId()], 5.0f)));
            approach = checkPointer(new BAPMoveToObjectFinding(
                bot, m_int0C[bot->getBotId()], 0.0f, mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.5f, taken,
                drive, m_behaviourId, stop, 0.0f, -1));
        }
        else
        {
            approach = checkPointer(new BAPMoveToFinding(
                bot, m_vec10[bot->getBotId()], mobile->getMaxSpeed(), mobile->getMaxSpeed() * 0.75f, taken, drive,
                m_behaviourId, 0.5f, 0.0f));
        }
        walk->addStatement(approach);

        BAPCondition* close = checkPointer(new BAPConObjectDistance(m_int0C[bot->getBotId()], reach + 1.0f, false, false, false));
        if (targetUnit->useNoPathfindingToGetToObject())
        {
            BAPConAnd* const behind = checkPointer(new BAPConAnd);
            behind->addCondition(close);
            behind->addCondition(checkPointer(new BAPConObjectBehind(m_int0C[bot->getBotId()], -0.8f, false)));
            close = behind;
        }
        BAPTrigger* const enter =
            checkPointer(new BAPTrigger(10, close, 0, kExitVehicleMask, m_behaviourId, 0.0, false));
        BAPUpdateVehicle* const update = checkPointer(new BAPUpdateVehicle(m_behaviourId));
        walk->addStatement(enter);

        BAPSerial* const sequence = checkPointer(new BAPSerial(m_behaviourId, -1));
        const bool askedForRide = communication->getReceivedMessages(static_cast<AIRadioMessageType>(1)).size() != 0;
        const bool askedForExit = communication->getReceivedMessages(static_cast<AIRadioMessageType>(0)).size() != 0;
        Information* const primary = informationManager->getInformation(target->getPrimaryObject(bot->getSide()));
        IAIObject* const primaryObject = primary->getAIObject();
        Bot* const occupantBot = IBotManager::instance->getBot(primaryObject);
        if (!m_bool14[bot->getBotId()] && inSoldier && primaryObject->getPlayerId() >= 0 && primary->getMobile()
            && !askedForRide && !askedForExit)
        {
            // Ask the driver for a ride while the vehicle is occupied.
            BAPConObjectOccupied* const driven = checkPointer(new BAPConObjectOccupied(primary->getHandle()));
            BAPSendRadioMessage* const ask = checkPointer(
                new BAPSendRadioMessage(2, 6, 3, 0.0f, m_behaviourId, primaryObject->getPlayerId()));
            sequence->addStatement(checkPointer(new BAPIf(driven, ask, 0, m_behaviourId, false, false)));
            if (occupantBot)
                communication->setInt92(primaryObject->getPlayerId());
            else
                communication->setInt92(0xFF);
        }
        sequence->addStatement(checkPointer(new BAPWhile(
            checkPointer(new BAPConNot(checkPointer(new BAPConObjectOccupied(m_int0C[bot->getBotId()])))), walk,
            false, m_behaviourId)));
        sequence->addStatement(update);
        BAPParallel* const both = checkPointer(new BAPParallel(m_behaviourId, -1, false));
        both->addStatement(checkPointer(new BAPICChangeVehicle(m_int0C[bot->getBotId()])));
        both->addStatement(sequence);
        BAPSerial* const outer = checkPointer(new BAPSerial(m_behaviourId, -1));
        if (m_bool18[bot->getBotId()] && communication->handleMessages(static_cast<AIRadioMessageType>(1)))
            outer->addStatement(checkPointer(new BAPSendRadioMessage(6, 6, 2, 1.0f, m_behaviourId, -1)));
        outer->addStatement(both);
        body = outer;
    }

    if (!inSoldier)
    {
        BAPParallel* const crew = checkPointer(new BAPParallel(m_behaviourId, true, -1, false));
        crew->addStatement(newFlareStatement(bot, m_behaviourId));
        crew->addStatement(body);
        body = crew;
    }
    BAPChangeWrapper* const script = checkPointer(new BAPChangeWrapper(
        static_cast<unsigned int>(m_int0C[bot->getBotId()]), body, m_behaviourId, bot->getPlayerId()));
    script->m_owner = this;
    return script;
}

BBPChangeLandingCraft::BBPChangeLandingCraft(int behaviourId)
{
    m_bool10 = checkPointer(new bool[configuredBotCount()]);
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPChangeLandingCraft::~BBPChangeLandingCraft()
{
    delete[] m_int0C;
    delete[] m_bool10;
}

BotScript* BBPChangeLandingCraft::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPChangeLandingCraft::generate");
}

BBPChangeTeleport::BBPChangeTeleport(int behaviourId)
{
    m_bool10 = checkPointer(new bool[configuredBotCount()]);
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPChangeTeleport::~BBPChangeTeleport()
{
    delete[] m_int0C;
    delete[] m_bool10;
}

BotScript* BBPChangeTeleport::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPChangeTeleport::generate");
}

BBPSuicide::BBPSuicide(int behaviourId)
{
    m_behaviourId = behaviourId;
}

BotScript* BBPSuicide::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPSuicide::generate");
}

BBPCallForMedic::BBPCallForMedic(int behaviourId)
{
    m_behaviourId = behaviourId;
}

BotScript* BBPCallForMedic::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPCallForMedic::generate");
}

BBPTakeCover3d::BBPTakeCover3d(int behaviourId)
{
    m_behaviourId = behaviourId;
}

BotScript* BBPTakeCover3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPTakeCover3d::generate");
}

BBPRevive::BBPRevive(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
    m_int10 = checkPointer(new int[configuredBotCount()]);
}

BBPRevive::~BBPRevive()
{
    delete[] m_int0C;
    delete[] m_int10;
}
// PE 10120EB0.
void BBPRevive::offerRevive(unsigned int target, int weapon, Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
    m_int0C[bot->getBotId()] = static_cast<int>(target);
    m_int10[bot->getBotId()] = weapon;
}


// PE 101210A0. Builds the medic's revive plan for the offered man-down soldier: approach until
// within 1.6 (flat), then crouch and fire the chosen weapon (the defibrillator), while the target
// exists and stays man down. A current fire plan of this behaviour on the same target is kept.
BotScript* BBPRevive::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPRevive::generate (script debugger reset)");
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId
        && bot->getPlan()->getType() == static_cast<BAPStatementType>(15)
        && static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 0
        && static_cast<BAPFireWrapper*>(bot->getPlan())->m_target
               == static_cast<unsigned int>(m_int0C[bot->getBotId()]))
        return bot->getPlan();

    bot->getSquadMember()->clearRequest();
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPRevive::generate (script debugger reset)");
    dropPlan(bot);
    bot->getScriptContext()->clear();

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_int0C[bot->getBotId()]);
    if (!equipment || !equipment->getUnit())
        return 0;
    IPIMobile* const mobile = equipment->getMobile();
    if (!mobile || !target)
        return 0;
    IPIControlInfo* const control = equipment->getControlInfo();
    IPIArmament* const armament = equipment->getArmament();
    const int strengthType = target->getPhysical()->getStrengthType();
    const Vec3& targetPosition = target->getPosition();
    const Vec3& own = equipment->getPosition();
    Vec3 toTarget;
    toTarget.x = targetPosition.x - own.x;
    toTarget.y = targetPosition.y - own.y;
    toTarget.z = targetPosition.z - own.z;
    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
    const int weaponIndex = chooseSupportWeapon(armament->getWeapons(), m_int10[bot->getBotId()],
                                                distance, strengthType, false, false, true, false);
    if (weaponIndex == -1)
    {
        equipment->getAIObject()->update();
        return 0;
    }
    bot->setReviveTarget(static_cast<unsigned int>(m_int0C[bot->getBotId()]));
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));

    BAPSerial* const serial = checkPointer(new BAPSerial(m_behaviourId, -1));
    // A weapon whose activate input is 64 gets a reset of the movement and action inputs instead.
    if (weapon->getWeaponActivate() == 64)
    {
        serial->addStatement(checkPointer(new BAPResetControls(true, false, true, m_behaviourId)));
    }
    else
    {
        serial->addStatement(checkPointer(new BAPTrigger(
            weapon->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(weapon->getWeaponActivate()), m_behaviourId, 0.0, false)));
    }

    BAPConObjectDistance* const near =
        checkPointer(new BAPConObjectDistance(m_int0C[bot->getBotId()], 1.6f, false, true, false));
    BAPConAnd* const close = checkPointer(new BAPConAnd);
    close->addCondition(checkPointer(new BAPConObjectDistance(m_int0C[bot->getBotId()], 10.0f, true, true, false)));
    close->addCondition(checkPointer(new BAPConObjectMoved(m_int0C[bot->getBotId()], 0.2f)));
    BAPMoveToObjectFinding* const move = checkPointer(new BAPMoveToObjectFinding(
        bot, m_int0C[bot->getBotId()], 0.0f, mobile->getMaxSpeed(), mobile->getMaxSpeed(), 0,
        control->getDriveControls(), m_behaviourId, close, 0.0f, -1));
    BAPIf* const revive =
        checkPointer(new BAPIf(near, checkPointer(new BAPRevive(m_behaviourId)), move, m_behaviourId, true, false));

    BAPConAnd* const valid = checkPointer(new BAPConAnd);
    BAPConObjectExists* const exists = checkPointer(new BAPConObjectExists(m_int0C[bot->getBotId()]));
    BAPConObjectManDown* const manDown = checkPointer(new BAPConObjectManDown(m_int0C[bot->getBotId()]));
    valid->addCondition(exists);
    valid->addCondition(manDown);

    BAPParallel* const root = checkPointer(new BAPParallel(m_behaviourId, -1, false));
    root->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));
    root->addStatement(checkPointer(new BAPOverrideSensingAgent));
    root->addStatement(checkPointer(new BAPALookAtObject(
        bot, m_int0C[bot->getBotId()], 0.0f, checkPointer(new BAPConFalse),
        equipment->getControlInfo()->getLookControls(), m_behaviourId, 0, 0.0f)));
    root->addStatement(revive);
    root->addStatement(checkPointer(new BAPMergeSoldierInputs(m_behaviourId)));
    serial->addStatement(checkPointer(new BAPWhile(valid, root, false, m_behaviourId)));

    if (bot->getPlayer()->getSpawnKit() == 5)
    {
        const unsigned int handle = static_cast<unsigned int>(m_int0C[bot->getBotId()]);
        bot->getSquadMember()->setCurrentRequestAction(1, handle);
    }

    BAPFireWrapper* const script = checkPointer(new BAPFireWrapper(
        bot, static_cast<unsigned int>(m_int0C[bot->getBotId()]), true, false, weaponIndex, -1, serial,
        m_behaviourId));
    script->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return script;
}

BBPC4::BBPC4(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPC4::~BBPC4()
{
    delete[] m_int0C;
}

// PE 101223B0.
void BBPC4::offerPlan(Bot* bot, bool detonate, AIStrategicObject* object)
{
    m_hasPlan[bot->getBotId()] = true;
    m_detonate = detonate;
    m_object = object;
}

// PE 101224E0. Switches to the C4 slot (4), arms it with the alternate fire unless the weapon is
// already in state 1 (when detonating: only while it is not), waits 0.7 seconds, fires, then places
// or removes the charge, and finally switches back to slot 2 when there is one. Any current plan
// of this behaviour is kept.
BotScript* BBPC4::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPC4::generate (script debugger reset)");
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPC4::generate (script debugger reset)");
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    bot->getCurrentEquipment();   // read and dropped, as shipped
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId)
        return bot->getPlan();
    dropPlan(bot);

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getUnit() || !equipment->getMobile())
        return 0;
    IPIArmament* const armament = equipment->getArmament();
    WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(4));
    if (!weapon)
        return 0;

    BAPTrigger* const select = checkPointer(new BAPTrigger(
        weapon->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
        singleInputMask(weapon->getWeaponActivate()), m_behaviourId, 0.0, false));
    BAPSerial* const serial = checkPointer(new BAPSerial(m_behaviourId, -1));
    serial->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));
    serial->addStatement(checkPointer(select));
    BotScriptValue<WeaponInfo*>* const weaponValue = context->createVariable(weapon, 0);

    BAPIf* arm;
    BAPPlaceC4* charge;
    if (m_detonate)
    {
        BAPTrigger* const altFire = checkPointer(new BAPTrigger(
            weapon->getWeaponAltFire(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(weapon->getWeaponAltFire()), m_behaviourId, 0.0, true));
        BAPConNot* const notArmed =
            checkPointer(new BAPConNot(checkPointer(new BAPConWeaponStateOne(weaponValue))));
        arm = checkPointer(new BAPIf(notArmed, altFire, 0, m_behaviourId, true, false));
        charge = checkPointer(new BAPPlaceC4(0, m_object, false));
    }
    else
    {
        BAPTrigger* const altFire = checkPointer(new BAPTrigger(
            weapon->getWeaponAltFire(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(weapon->getWeaponAltFire()), m_behaviourId, 0.0, true));
        BAPConWeaponStateOne* const armed = checkPointer(new BAPConWeaponStateOne(weaponValue));
        arm = checkPointer(new BAPIf(armed, altFire, 0, m_behaviourId, true, false));
        charge = checkPointer(new BAPPlaceC4(0, m_object, true));
    }
    serial->addStatement(arm);
    serial->addStatement(checkPointer(new BAPIdle(
        checkPointer(new BAPConTimer(static_cast<double>(0.7f), false, false, false)), 0, m_behaviourId)));
    serial->addStatement(checkPointer(new BAPTrigger(
        weapon->getWeaponFire(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
        singleInputMask(weapon->getWeaponFire()), m_behaviourId, 0.0, true)));
    serial->addStatement(charge);

    WeaponInfo* const previous = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(2));
    if (previous)
    {
        serial->addStatement(checkPointer(new BAPTrigger(
            previous->getWeaponActivate(), checkPointer(new BAPConTrue), checkPointer(new BAPConFalse),
            singleInputMask(previous->getWeaponActivate()), m_behaviourId, 0.0, false)));
    }

    BAPFireWrapper* const script = checkPointer(new BAPFireWrapper(
        bot, static_cast<unsigned int>(-1), true, false, 4, -1, serial, m_behaviourId));
    script->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return script;
}

BBPRunOverSoldier::BBPRunOverSoldier(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
}

BBPRunOverSoldier::~BBPRunOverSoldier()
{
    delete[] m_targets;
}

// PE 10125BE0.
void BBPRunOverSoldier::offerTarget(Bot* bot, unsigned int target)
{
    const unsigned int id = bot->getBotId();
    m_hasPlan[id] = true;
    m_targets[id] = target;
}

// PE 10125D10. Keeps a running run-over plan against the same target; otherwise drives at the
// target under a fire wrapper (weapon 4).
BotScript* BBPRunOverSoldier::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        if (bot->getDebugger())
            BF2_AI_FRONTIER("dice::hfe::ai::BBPRunOverSoldier::generate (script debugger reset)");
        bot->getScriptContext()->clear();
        dropPlan(bot);
        return 0;
    }
    if (bot->getDebugger())
        BF2_AI_FRONTIER("dice::hfe::ai::BBPRunOverSoldier::generate (script debugger reset)");
    bot->getScriptContext()->clear();
    bot->getCurrentEquipment();   // read and dropped, as shipped
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId
        && bot->getPlan()->getType() == static_cast<BAPStatementType>(15)
        && static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 0
        && static_cast<BAPFireWrapper*>(bot->getPlan())->m_target == m_targets[bot->getBotId()])
        return bot->getPlan();
    dropPlan(bot);

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getUnit() || !equipment->getMobile())
        return 0;
    IPIControlInfo* const control = equipment->getControlInfo();
    BAPRunOverObject* const runOver = checkPointer(
        new BAPRunOverObject(m_targets[bot->getBotId()], control->getDriveControls(), m_behaviourId));
    BAPFireWrapper* const script = checkPointer(new BAPFireWrapper(
        bot, m_targets[bot->getBotId()], true, false, 4, -1, runOver, m_behaviourId));
    script->m_owner = this;
    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    return script;
}

BBPRandom::BBPRandom(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPRandom::~BBPRandom()
{
    delete[] m_int0C;
}

// PE 10126340.
void BBPRandom::offerPlan(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
    m_offerTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

BotScript* BBPRandom::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPRandom::generate");
}

BBPTriggerable::BBPTriggerable(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
    m_int14 = 0;
}

BBPTriggerable::~BBPTriggerable()
{
    delete[] m_int0C;
}

BotScript* BBPTriggerable::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPTriggerable::generate");
}

BBPBridgeRepair::BBPBridgeRepair(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
    m_int10 = checkPointer(new int[configuredBotCount()]);
    m_vec14 = checkPointer(newZeroedVectors(configuredBotCount()));
    m_bool18 = checkPointer(new bool[configuredBotCount()]);
    m_vec1C = checkPointer(newZeroedVectors(configuredBotCount()));
    m_vec20 = checkPointer(newZeroedVectors(configuredBotCount()));
    for (int i = 0; i < configuredBotCount(); ++i)
    {
        m_int0C[i] = -1;
        m_int10[i] = -1;
        m_bool18[i] = false;
    }
}

BBPBridgeRepair::~BBPBridgeRepair()
{
    delete[] m_int0C;
    delete[] m_int10;
    delete[] m_vec14;
    delete[] m_bool18;
    delete[] m_vec1C;
    delete[] m_vec20;
}

// PE 101239A0.
void BBPBridgeRepair::offerRepair(unsigned int bridge, int weapon, Vec3 vector, Bot* bot, bool requested,
                                  Vec3 position, Vec3 start)
{
    m_hasPlan[bot->getBotId()] = true;
    m_int0C[bot->getBotId()] = static_cast<int>(bridge);
    m_int10[bot->getBotId()] = weapon;
    m_vec14[bot->getBotId()] = vector;
    m_bool18[bot->getBotId()] = requested;
    m_vec1C[bot->getBotId()] = position;
    m_vec20[bot->getBotId()] = start;
}

BotScript* BBPBridgeRepair::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPBridgeRepair::generate");
}

BBPDefuseMines::BBPDefuseMines(int behaviourId)
{
    m_behaviourId = behaviourId;
    m_int0C = checkPointer(new int[configuredBotCount()]);
    m_int10 = checkPointer(new int[configuredBotCount()]);
    m_vec14 = checkPointer(newZeroedVectors(configuredBotCount()));
    m_vec18 = checkPointer(newZeroedVectors(configuredBotCount()));
    for (int i = 0; i < configuredBotCount(); ++i)
    {
        m_int0C[i] = -1;
        m_int10[i] = -1;
    }
}

BBPDefuseMines::~BBPDefuseMines()
{
    delete[] m_int0C;
    delete[] m_int10;
    delete[] m_vec14;
    delete[] m_vec18;
}

// PE 10124E90.
void BBPDefuseMines::offerDefuse(unsigned int mine, int weapon, Bot* bot, Vec3 minePosition,
                                 Vec3 standPosition)
{
    const unsigned int id = bot->getBotId();
    m_hasPlan[id] = true;
    m_int0C[id] = static_cast<int>(mine);
    m_int10[id] = weapon;
    m_vec14[id] = minePosition;
    m_vec18[id] = standPosition;
}

BotScript* BBPDefuseMines::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPDefuseMines::generate");
}

// ---- Idle family ---------------------------------------------------------------------------

IdlePlanGenerator::IdlePlanGenerator(int behaviourId)
{
    m_behaviourId = behaviourId;
}

IdlePlanGenerator::~IdlePlanGenerator()
{
}

// PE 101A8DF0.
bool IdlePlanGenerator::hasFreeSeat(unsigned int handle, int side)
{
    Information* information = informationManager->getInformation(handle);
    if (!information)
        return false;
    if (!information->isPrimary())
        information = informationManager->getInformation(information->getPrimaryObject(side));
    if (!information->getUnit())
        return false;
    if (!information->getUnit()->isOccupied())
        return true;
    int iterator;
    for (Information* secondary =
             informationManager->getInformation(information->getFirstSecondaryObject(iterator, side));
         secondary;
         secondary = informationManager->getInformation(information->getSecondaryObject(iterator, side)))
    {
        if (secondary->getUnit() && !secondary->getUnit()->isOccupied())
            return true;
    }
    return false;
}

int IdlePlanGenerator::getIdleType()
{
    return 0;
}

BBPIdle2d::BBPIdle2d(int behaviourId)
    : IdlePlanGenerator(behaviourId)
{
    m_int0C = checkPointer(new int[configuredBotCount()]);
    for (int i = 0; i < configuredBotCount(); ++i)
        m_int0C[i] = -1;
}

BBPIdle2d::~BBPIdle2d()
{
    delete[] m_int0C;
}

int BBPIdle2d::getIdleType()
{
    return 1;
}

// PE 100F6390.
void BBPIdle2d::setTarget(Bot* bot, int target)
{
    m_hasPlan[bot->getBotId()] = true;
    m_int0C[bot->getBotId()] = target;
}

// PE 100F6490. Keeps its own plan; a vehicle bot asked by a player (m_int0C) answers a waiting
// request with radio message 6. A new plan answers once more when the player's vehicle is within
// 40 and, unless the player is 255, the bot's equipment has a free seat; it then holds the controls
// released forever while dropping flares on a missile lock.
BotScript* BBPIdle2d::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        releaseScriptState(bot);
        if (bot->getPlan())
        {
            if (bot->getPlan())
                delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan())
    {
        if (bot->getPlan()->getId() == m_behaviourId)
        {
            // The equipment is not null-checked, as shipped.
            const int player = m_int0C[bot->getBotId()];
            if (player >= 0 && !((equipment->getObjectTypeFlags() >> 21) & 1)
                && bot->getCommunicationAgent()->handleMessages(static_cast<AIRadioMessageType>(2), player))
                bot->getCommunicationAgent()->enqueueRadioMessage(6, 6, 3, 1.0f, -1);
            return bot->getPlan();
        }
    }
    else if (bot->getPlan())
    {
        if (bot->getPlan())
            delete bot->getPlan();
        bot->setPlan(0);
    }

    releaseScriptState(bot);
    bot->getSquadMember()->clearRequest();
    BAPSerial* const root = checkPointer(new BAPSerial(m_behaviourId, -1));
    // Neither lookup is checked, as shipped.
    Information* const primary = informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
    const int player = m_int0C[bot->getBotId()];
    if (player >= 0 && !((primary->getObjectTypeFlags() >> 21) & 1))
    {
        IAIPlayer* const requester =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(player);
        IAIObject* const vehicle = requester ? requester->getCurrentVehicle() : 0;
        if (vehicle)
        {
            const unsigned int equipmentHandle = bot->getCurrentEquipment();
            Information* const requesterVehicle =
                informationManager->getInformation(vehicle->getHandle(bot->getSide()));
            if (requesterVehicle)
            {
                const float* const to = requesterVehicle->getTransformation().m + 12;
                const float* const from = primary->getTransformation().m + 12;
                const float dx = from[0] - to[0];
                const float dy = from[1] - to[1];
                const float dz = from[2] - to[2];
                const float distanceSq = dx * dx + dy * dy + dz * dz;
                if ((player == 0xFF || hasFreeSeat(equipmentHandle, bot->getSide()))
                    && distanceSq < 1600.0f && equipment->isPrimary()
                    && bot->getCommunicationAgent()->handleMessages(static_cast<AIRadioMessageType>(2), player))
                    root->addStatement(checkPointer(new BAPSendRadioMessage(6, 6, 2, 1.0f, m_behaviourId, -1)));
            }
        }
    }

    BAPResetControls* const release = new BAPResetControls(true, true, true, m_behaviourId);
    BAPConTrue* const always = new BAPConTrue;
    root->addStatement(checkPointer(new BAPWhile(checkPointer(always), checkPointer(release), m_behaviourId)));

    BAPParallel* const body = checkPointer(new BAPParallel(m_behaviourId, true, -1, false));
    // The first argument is the bot pointer in retail, stored and never read; on a 64-bit port
    // only its low half fits. [PLATFORM]
    body->addStatement(new BAPUseFlareOnMissileLock(
        static_cast<int>(reinterpret_cast<std::size_t>(bot)), 0x800000000000ULL, m_behaviourId));
    body->addStatement(root);
    BAPInfoWrapper* const script =
        checkPointer(new BAPInfoWrapper(equipment->getPosition(), body, m_behaviourId, bot));
    script->m_owner = this;
    bot->getBotNavigation()->clearProgress();
    return script;
}

BBPIdle3d::BBPIdle3d(int behaviourId)
    : IdlePlanGenerator(behaviourId)
{
    m_int10 = checkPointer(new int[configuredBotCount()]);
    const int bots = configuredBotCount();
    m_float0C = checkPointer(new float[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_float0C[i] = 100.0f;
        m_int10[i] = -1;
    }
}

BBPIdle3d::~BBPIdle3d()
{
    delete[] m_int10;
}

int BBPIdle3d::getIdleType()
{
    return 2;
}

// PE 100F5A70.
void BBPIdle3d::setTarget(float distance, Bot* bot, int target)
{
    m_float0C[bot->getBotId()] = distance;
    m_hasPlan[bot->getBotId()] = true;
    m_int10[bot->getBotId()] = target;
}

BotScript* BBPIdle3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPIdle3d::generate");
}

BBPIdleInfantery::BBPIdleInfantery(int behaviourId)
    : IdlePlanGenerator(behaviourId)
{
}

BBPIdleInfantery::~BBPIdleInfantery()
{
}

int BBPIdleInfantery::getIdleType()
{
    return 3;
}

// PE 100F51C0.
void BBPIdleInfantery::offerPlan(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

// PE 100F5510. A bot standing at its strategic object zooms its active weapon (when it has a
// zoom input) and, with equipment type flag 21, takes stance 2; it then idles with its controls
// released, forever. The plan is reused while it is this behaviour's.
BotScript* BBPIdleInfantery::generate(Bot* bot)
{
    if (!m_hasPlan[bot->getBotId()])
    {
        releaseScriptState(bot);
        if (bot->getPlan())
        {
            if (bot->getPlan())
                delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }
    releaseScriptState(bot);
    Information* const equipment =
        informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId)
        return bot->getPlan();
    if (bot->getPlan())
    {
        if (bot->getPlan())
            delete bot->getPlan();
        bot->setPlan(0);
    }
    bot->getSquadMember()->clearRequest();

    BAPParallel* const root = checkPointer(new BAPParallel(m_behaviourId, false, 99, false));
    root->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));

    const Vec3& position = equipment->getPosition();
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getClosestObject(position, bot->getSide(), false);
    if (object)
    {
        const float radius = object->m_size[bot->getSide()];
        Vec3 centre = {};
        vec2ToVec3XZ(object->getPosition(bot->getSide()), centre);
        centre.y = position.y;
        const float dx = centre.x - position.x;
        const float dy = centre.y - position.y;
        const float dz = centre.z - position.z;
        const float reach = radius + 2.0f;
        if (dz * dz + dy * dy + dx * dx < reach * reach)
        {
            IPIArmament* const armament = equipment->getArmament();
            if (armament && armament->getActiveWeapon() != -1)
            {
                WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
                    armament->getWeapon(armament->getActiveWeapon()));
                if (weapon && weapon->getWeaponZoom() != 64)
                {
                    BAPConFalse* const release = new BAPConFalse;
                    BAPConTrue* const until = new BAPConTrue;
                    const unsigned long long mask = 1ULL << (weapon->getWeaponZoom() & 63);
                    BAPTrigger* const zoom = new BAPTrigger(
                        weapon->getWeaponZoom(), checkPointer(until), checkPointer(release), mask,
                        m_behaviourId, 0.0, false);
                    BAPConNot* const notZoomed = new BAPConNot(checkPointer(new BAPConZoomed));
                    root->addStatement(checkPointer(new BAPIf(
                        checkPointer(notZoomed), checkPointer(zoom), 0, m_behaviourId, true, false)));
                }
            }
            if ((equipment->getObjectTypeFlags() >> 21) & 1)
            {
                // PE 100F58B4.
                root->addStatement(checkPointer(
                    new BAPSoldierPose(2, checkPointer(new BAPConTrue), m_behaviourId, true)));
            }
        }
    }

    BAPWhile* const loop =
        checkPointer(new BAPWhile(checkPointer(new BAPConTrue), root, m_behaviourId));
    BAPInfoWrapper* const script = checkPointer(
        new BAPInfoWrapper(equipment->getPosition(), loop, m_behaviourId, bot));
    script->m_owner = this;
    return script;
}

// Shared opening of the generators: resets the bot's script debugger and clears the script
// variables. [CHOSEN, not recovered] Retail inlines this into each generate.
static void releaseScriptState(Bot* bot)
{
    if (bot->getDebugger())
    {
        // Retail calls the debugger's third slot; the debugger type is not recovered.
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-debugger]
        BF2_AI_FRONTIER("dice::hfe::ai::PlanGenerator (script debugger reset)");
    }
    bot->getScriptContext()->clear();
}

// ---- Fire family base ----------------------------------------------------------------------

FirePlanGenerator::FirePlanGenerator()
{
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    m_targetChanged = checkPointer(new bool[configuredBotCount()]);
    m_weapons = checkPointer(new int[configuredBotCount()]);
    m_designators = checkPointer(new int[configuredBotCount()]);
    m_positions = checkPointer(newZeroedVectors(configuredBotCount()));
}

FirePlanGenerator::~FirePlanGenerator()
{
    delete[] m_targets;
    delete[] m_targetChanged;
    delete[] m_weapons;
    delete[] m_designators;
    delete[] m_positions;
}

int FirePlanGenerator::getFireType()
{
    return 0;
}

// PE 10117890.
void BBPFire3d::setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon)
{
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    m_weapons[bot->getBotId()] = weapon;
    m_designators[bot->getBotId()] = designator;
    m_positions[bot->getBotId()] = position;
}

// PE 101176F0.
void BBPFire3d::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool flagA, bool flagB)
{
    m_hasPlan[bot->getBotId()] = true;
    m_targets[bot->getBotId()] = target;
    m_designators[bot->getBotId()] = -1;
    m_weapons[bot->getBotId()] = weapon;
    m_flagA = flagA;
    m_flagB = flagB;
}

BBPFire3d::BBPFire3d(int behaviourId)
{
    m_behaviourId = behaviourId;
}

BBPFire3d::~BBPFire3d()
{
}

int BBPFire3d::getFireType()
{
    return 4;
}

BotScript* BBPFire3d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPFire3d::generate");
}

} // namespace ai
} // namespace hfe
} // namespace dice
