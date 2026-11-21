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

BotScript* BBPAvoidCollisionInfantery::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPAvoidCollisionInfantery::generate");
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

BotScript* BBPGotoWaypoint2d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypoint2d::generate");
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

BotScript* BBPChange::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPChange::generate");
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


BotScript* BBPRevive::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPRevive::generate");
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
    m_int0C = checkPointer(new int[configuredBotCount()]);
}

BBPRunOverSoldier::~BBPRunOverSoldier()
{
    delete[] m_int0C;
}

BotScript* BBPRunOverSoldier::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPRunOverSoldier::generate");
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
