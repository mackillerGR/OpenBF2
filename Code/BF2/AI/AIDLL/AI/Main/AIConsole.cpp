// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIConsole operations.

#include "AIConsole.h"

#include "../AIBot/SensingAgentPatterns.h"     // the 4 registered patterns
#include "../AIBot/SquadManager.h"               // init's second allocation
#include "../AIUtils/checkPointer.h"             // init's two dead null checks
#include "../AIPlanInterpreter/BAPEntries.h"   // the 42 registered primitives
#include "AIDllFrontier.h"
#include "AIMain.h"                                // IAIMain::m_instance
#include "StrategyManager.h"                       // addSAIStrategy lookup
#include "../AIStrategic/SAI.h"                    // ISAI::addStrategy
#include "../../../../Core/ConsoleObjectMacro.h"   // the ai.init binding at the tail
#include "CommonSingletonServer.h"
#include "AISettingsInterfaces.h"
#include "BotBehaviourManager.h"
#include "../AIPlanInterpreter/BAPStatementType.h"

namespace dice {
namespace hfe {
namespace ai {

AIConsole* AIConsole::m_instance = 0;

// Registers Bot Action Plan primitives.
// [FRONTIER] Missing primitive implementations stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
AIConsole::AIConsole()
{
    // Publishes this console as the active singleton.
    m_instance = this;
    // The shipped constructor zeroes the cursor; it is not the end of the modifier map.
    m_activeBehaviourModifiers = std::map<std::string, float*>::iterator();

    // the forty-two BAP primitives, in the binary's registration order
    m_bapEntries["TankMoveTo"] = new EntryTankMoveTo();
    m_bapEntries["TankMoveToObject"] = new EntryTankMoveToObject();
    m_bapEntries["TankMoveToDirection"] = new EntryTankMoveToDirection();
    m_bapEntries["TankResetControls"] = new EntryTankResetControls();
    m_bapEntries["TankTurnTo"] = new EntryTankTurnTo();
    m_bapEntries["CarMoveTo"] = new EntryCarMoveTo();
    m_bapEntries["CarMoveToDirection"] = new EntryCarMoveToDirection();
    m_bapEntries["CarMoveToObject"] = new EntryCarMoveToObject();
    m_bapEntries["CarResetControls"] = new EntryCarResetControls();
    m_bapEntries["PlaneMoveTo"] = new EntryPlaneMoveTo();
    m_bapEntries["PlaneMoveToDirection"] = new EntryPlaneMoveToDirection();
    m_bapEntries["PlaneMoveToObject"] = new EntryPlaneMoveToObject();
    m_bapEntries["PlaneAimAt"] = new EntryPlaneAimAt();
    m_bapEntries["PlaneResetControls"] = new EntryPlaneResetControls();
    m_bapEntries["BoatMoveTo"] = new EntryBoatMoveTo();
    m_bapEntries["BoatMoveToObject"] = new EntryBoatMoveToObject();
    m_bapEntries["BoatMoveToDirection"] = new EntryBoatMoveToDirection();
    m_bapEntries["BoatResetControls"] = new EntryBoatResetControls();
    m_bapEntries["InfanteryMoveTo"] = new EntryInfanteryMoveTo();
    m_bapEntries["InfanteryMoveToObject"] = new EntryInfanteryMoveToObject();
    m_bapEntries["InfanteryMoveToDirection"] = new EntryInfanteryMoveToDirection();
    m_bapEntries["InfanteryResetControls"] = new EntryInfanteryResetControls();
    m_bapEntries["Trigger"] = new EntryTrigger();
    m_bapEntries["TriggerContinously"] = new EntryTriggerContinously();
    m_bapEntries["KeyTurretAimAt"] = new EntryKeyTurretAimAt();
    m_bapEntries["MouseTurretAimAt"] = new EntryMouseTurretAimAt();
    m_bapEntries["MouseTurretLookAt"] = new EntryMouseTurretLookAt();
    m_bapEntries["Sense"] = new EntrySense();
    m_bapEntries["SoldierPose"] = new EntrySoldierPose();
    m_bapEntries["MoveToMedium"] = new EntryMoveToMedium();
    m_bapEntries["MoveToMediumBoat"] = new EntryMoveToMediumBoat();
    m_bapEntries["MoveToMediumSoldier"] = new EntryMoveToMediumSoldier();
    m_bapEntries["MoveToObjectMedium"] = new EntryMoveToObjectMedium();
    m_bapEntries["MoveToObjectMediumBoat"] = new EntryMoveToObjectMediumBoat();
    m_bapEntries["MoveToObjectMediumSoldier"] = new EntryMoveToObjectMediumSoldier();
    m_bapEntries["InfoWrapper"] = new EntryInfoWrapper();
    m_bapEntries["HeliMoveTo"] = new EntryHeliMoveTo();
    m_bapEntries["HeliMoveToDirection"] = new EntryHeliMoveToDirection();
    m_bapEntries["HeliMoveToObject"] = new EntryHeliMoveToObject();
    m_bapEntries["HeliAimAt"] = new EntryHeliAimAt();
    m_bapEntries["HeliResetControls"] = new EntryHeliResetControls();
    m_bapEntries["HeliMoveAwayFromObject"] = new EntryHeliMoveAwayFromObject();

    // the four sensing-agent patterns
    m_sensingAgentPatterns["Turret"] = new SensingAgentTurretPattern();
    m_sensingAgentPatterns["MouseFreeLook"] = new SensingAgentMouseFreeLookPattern();
    m_sensingAgentPatterns["Soldier"] = new SensingAgentSoldierPattern();
    m_sensingAgentPatterns["Null"] = new SensingAgentNullPattern();
}

// [FRONTIER] Destruction requires cleanup of owned plan entries, behavior entries and squad state.
AIConsole::~AIConsole()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::~AIConsole");
}

// [FRONTIER] Reset must delete behavior-modifier arrays and Strategy objects and return
// true, while retaining plan-entry and sensing-pattern registries. Ordered cleanup is unimplemented.
bool AIConsole::reset()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::reset");
}

// Dedicated-server console behavior: no single-player instance, no debug/camera-text
// action, and zero camera position and rotation.
AIConsole* AIConsole::getSinglePlayerInstance()
{
    return 0;
}

IAIMain* AIConsole::getActiveAIMain()
{
    return IAIMain::m_instance;
}

void AIConsole::debug()
{
}

void AIConsole::removeCameraText()
{
}

// A zeroed Vec3, by value. Three immediate zero stores into the return slot and nothing else.
Vec3 AIConsole::getRendCameraPosition()
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    return position;
}

Vec3 AIConsole::getRendCameraRotation()
{
    Vec3 rotation;
    rotation.x = 0.0f;
    rotation.y = 0.0f;
    rotation.z = 0.0f;
    return rotation;
}

// [FRONTIER] Remaining bot, behavior and interpreter commands require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
BAPEntry* AIConsole::getBAPEntryFromName(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getBAPEntryFromName");
}

ISensingAgentPattern* AIConsole::getSensingAgentPatternFromName(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getSensingAgentPatternFromName");
}

// Initializes AI from a level script. Returns immediately when an active AI runtime already exists.
void AIConsole::init(int n)
{
    if (getActiveAIMain())
        return;

    AIMain* const aiMain = new AIMain();
    checkPointer(aiMain);

    m_aiMain = aiMain;

    // The second lookup is retail's - see above. The result is discarded.
    static_cast<AIMain*>(getActiveAIMain())->init(n);

    SquadManager* const squadManager = new SquadManager();
    checkPointer(squadManager);

    // SquadManager registers itself as a singleton; AIConsole owns it through that pointer.
    (void)squadManager;
}

void AIConsole::addBot(std::string, int, std::string, std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::addBot");
}

void AIConsole::addUnspawnedBot(std::string, int, std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::addUnspawnedBot");
}

void AIConsole::updateBot(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::updateBot");
}

void AIConsole::killBotName(const std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::killBotName");
}

void AIConsole::killBotId(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::killBotId");
}

void AIConsole::killAllBots()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::killAllBots");
}

void* AIConsole::getBotFromId(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getBotFromId");
}

void AIConsole::botMoveTo(float, Vec3, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::botMoveTo");
}

void AIConsole::botFlyTo(int, float, float, float, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::botFlyTo");
}

void AIConsole::botSetWaypoints_Patrol(int, float, int, Vec3, Vec3, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::botSetWaypoints_Patrol");
}

void AIConsole::botSetWaypoints_Patrol3d(int, float, int, Vec3, Vec3, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::botSetWaypoints_Patrol3d");
}

void AIConsole::botGiveOrder(int, std::string, std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::botGiveOrder");
}

// PE 1006F220. Ignored until the settings object exists.
void AIConsole::setBehaviour(int id, std::string name)
{
    if (CommonSingletonServer::m_iaiSettings)
        BotBehaviourManager::m_instance->setBehaviourName(id, name);
}

std::string AIConsole::getBehaviour(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getBehaviour");
}

// PE 10071D70. Ignored while the bot count is unset, or for an unknown vehicle or behaviour.
void AIConsole::setVehicleDefaultBehaviour(std::string& vehicle, std::string& behaviour)
{
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (settings->getMaxNBots() == -1)
        return;
    const int vehicleId = settings->getVehicle(vehicle);
    if (vehicleId == -1)
        return;
    const int behaviourId = BotBehaviourManager::m_instance->getBehaviourId(behaviour);
    if (behaviourId == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setDefaultBehaviour(vehicleId, behaviourId);
}

// PE 10073750. Builds the behaviour named by `createFunction` with the plan generator named by
// `generator` and installs it for the vehicle. The create-function name "NULL" installs an empty
// slot. Unknown vehicles and modifier sets are ignored, as is a behaviour that fails to build.
void AIConsole::setVehicleBehaviour(std::string vehicle, std::string behaviour,
                                    std::string createFunction, std::string generator,
                                    int priority, std::string urgeCurve, std::string modifiers)
{
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (settings->getMaxNBots() == -1)
        return;
    const int vehicleId = settings->getVehicle(vehicle);
    if (vehicleId == -1)
        return;
    const std::map<std::string, float*>::iterator weights = m_behaviourModifiers.find(modifiers);
    if (weights == m_behaviourModifiers.end())
        return;

    std::string message;
    BotBehaviour* instance = 0;
    if (behaviour != "NULL")
    {
        instance = BotBehaviourManager::m_instance->create(behaviour, createFunction, generator,
                                                           priority, urgeCurve, weights->second,
                                                           message);
        if (!instance)
            return;
    }
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->addBehaviour(
        vehicleId, BotBehaviourManager::m_instance->getBehaviourId(behaviour), instance);
}

// PE 10075B20. Looking the name up creates an empty entry for an unknown name, as shipped.
void AIConsole::setBasicBotWeights(std::string name)
{
    float* const weights = m_behaviourModifiers[name];
    if (weights)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->setBasicBotWeights(weights);
}

// PE 10074880. Creates a zeroed weight per behaviour for a new name and makes the named set the
// target of setBehaviourModifier.
void AIConsole::createBehaviourModifiers(std::string name)
{
    if (m_behaviourModifiers.find(name) == m_behaviourModifiers.end())
    {
        const IAISettings* const settings =
            static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
        float* const weights = new float[settings->getNumberOfBehaviours()];
        for (int i = 0; i < settings->getNumberOfBehaviours(); ++i)
            weights[i] = 0.0f;
        m_behaviourModifiers.insert(std::make_pair(name, weights));
    }
    m_activeBehaviourModifiers = m_behaviourModifiers.find(name);
}

void AIConsole::setActiveBehaviourModifiers(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::setActiveBehaviourModifiers");
}

std::string AIConsole::getActiveBehaviourModifiers()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getActiveBehaviourModifiers");
}

float AIConsole::getBehaviourModifier(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getBehaviourModifier");
}

// PE 10072D00. Writes one weight of the active set; ignored without one or for an unknown name.
void AIConsole::setBehaviourModifier(std::string behaviour, float value)
{
    if (m_activeBehaviourModifiers == m_behaviourModifiers.end())
        return;
    const int id = BotBehaviourManager::m_instance->getBehaviourId(behaviour);
    if (id != -1)
        m_activeBehaviourModifiers->second[id] = value;
}

void AIConsole::listBehaviourModifiers()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::listBehaviourModifiers");
}

// PE 10076EE0. Looking the entry up creates an empty slot for an unknown name, as shipped.
void AIConsole::addInterpreterEntry(std::string vehicle, std::string entry)
{
    BAPEntry* const primitive = m_bapEntries[entry];
    if (!primitive)
        return;
    const int vehicleId =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    if (vehicleId != -1)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->addInterpreterEntry(vehicleId, primitive);
}

// PE 10076F50.
void AIConsole::addInterpreterEntry(std::string vehicle, std::string entry, BotLodLevel level)
{
    BAPEntry* const primitive = m_bapEntries[entry];
    if (!primitive)
        return;
    const int vehicleId =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    if (vehicleId != -1)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->addInterpreterEntry(vehicleId, level, primitive);
}

void AIConsole::removeInterpreterEntry(std::string, std::string, BotLodLevel)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::removeInterpreterEntry");
}

// PE 10071DD0.
void AIConsole::activateDefaultInterpreter(std::string vehicle, BAPStatementType type,
                                           BotLodLevel level)
{
    const int vehicleId =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    if (vehicleId == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->activateDefaultInterpreter(vehicleId, type, level);
}

// PE 10071E20. Unknown sides and strategy names are ignored.
void AIConsole::addSAIStrategy(int side, std::string name)
{
    if (!IAIMain::m_instance)
        return;
    SAI* const sai = IAIMain::m_instance->getSAI(side);
    if (!sai)
        return;
    Strategy* const strategy = StrategyManager::getInstance()->getStrategy(name);
    if (strategy)
        sai->addStrategy(strategy);
}

// PE 10076BF0. Looking the pattern up creates an empty slot for an unknown name, as shipped.
void AIConsole::addSensingAgentPattern(const std::string& vehicle, const std::string& pattern)
{
    ISensingAgentPattern* const instance = m_sensingAgentPatterns[pattern];
    if (!instance)
        return;
    const int vehicleId =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    if (vehicleId != -1)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->addSensingAgentPattern(vehicleId, instance);
}

// PE 1006F4F0.
void AIConsole::setSensingAgentPatternName(const std::string& name)
{
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setSensingAgentPatternName(name);
}

// PE 10076C50. An unknown name installs a null pattern.
void AIConsole::setSensingAgentNullPattern(const std::string& name)
{
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setSensingAgentNullPattern(m_sensingAgentPatterns[name]);
}

void AIConsole::sensingAgentDebug()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::sensingAgentDebug");
}

void AIConsole::addAllowedUnit(const std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::addAllowedUnit");
}

void AIConsole::addExpelledUnit(const std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::addExpelledUnit");
}

void AIConsole::addLandingZoneUnit(const std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::addLandingZoneUnit");
}

// PE 10073910. The vehicle id is not checked.
void AIConsole::setVehicleMods(const std::string& vehicle, const std::string& modifiers)
{
    const int vehicleId =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    const std::map<std::string, float*>::iterator weights = m_behaviourModifiers.find(modifiers);
    if (weights != m_behaviourModifiers.end())
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->setVehicleMods(vehicleId, weights->second);
}

// PE 10073960.
void AIConsole::setStandardPersonality(const std::string& modifiers, int side)
{
    const std::map<std::string, float*>::iterator weights = m_behaviourModifiers.find(modifiers);
    if (weights == m_behaviourModifiers.end())
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setStandardPersonality(weights->second, side);
}

float AIConsole::getFastThreshold()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getFastThreshold");
}

void AIConsole::setFastThreshold(float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::setFastThreshold");
}

void AIConsole::freeCameraFire()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::freeCameraFire");
}

void AIConsole::showAISettings()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::showAISettings");
}

void AIConsole::createLODManager(float, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::createLODManager");
}

void AIConsole::saveAll(const std::string&, const std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::saveAll");
}

float AIConsole::getCurrentPitchAngle()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getCurrentPitchAngle");
}

float AIConsole::getCurrentYawAngle()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIConsole::getCurrentYawAngle");
}

// Console binding for ai.init. Calls may reach named stops in dependent subsystems.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

namespace {

const char* const kAiConsoleTag = "ai";

void consoleAiInit(int n)
{
    // Requires AIConsole initialization before level scripts invoke this command.
    AIConsole::m_instance->init(n);
}

void consoleAiAddSAIStrategy(int side, const std::string& name)
{
    AIConsole::m_instance->addSAIStrategy(side, name);
}

} // anonymous namespace

BF2_CONSOLE_OBJECT("ai", 2, 0)
BF2_CONSOLE_METHOD_1(init, consoleAiInit, int, kAiConsoleTag)
BF2_CONSOLE_METHOD_2_ACTIVE(addSAIStrategy, consoleAiAddSAIStrategy, AIConsole::m_instance != 0,
                            int, std::string, kAiConsoleTag)

// Handed to the console registry by AIModule::init.
ConsoleObjectList& aiConsoleObjects()
{
    return consoleBindingList(kAiConsoleTag);
}

} // namespace ai
} // namespace hfe
} // namespace dice
