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
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/checkPointer.h"

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

BotScript* BBPGotoWaypointInfantery::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPGotoWaypointInfantery::generate");
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

BotScript* BBPMedicAssist::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPMedicAssist::generate");
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

BotScript* BBPC4::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPC4::generate");
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

BotScript* BBPIdle2d::generate(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPIdle2d::generate");
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
                // Retail adds a stance-2 SoldierPose statement (PE vtable 102835AC, constructor
                // 101A9130) whose second base is not yet recovered.
                // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-soldier-pose]
                BF2_AI_FRONTIER("dice::hfe::ai::BBPIdleInfantery::generate (SoldierPose statement)");
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
