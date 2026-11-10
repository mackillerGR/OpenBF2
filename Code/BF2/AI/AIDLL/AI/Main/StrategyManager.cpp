// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining StrategyManager operations.

#include "StrategyManager.h"

#include "AIDllFrontier.h"
#include "../AIStrategic/StatusMap.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../../../../Core/ConsoleObjectMacro.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

StrategyManager* StrategyManager::m_instance = 0;

// Initializes strategy collections and installs the singleton when none exists.
StrategyManager::StrategyManager()
    : m_activeStrategy(0)
    , m_activePrerequisite(0)
    , m_enableDebug(false)
{
    m_forcedStrategy[0] = 0;
    m_forcedStrategy[1] = 0;

    m_activeCondition = m_conditions.end();
    m_activeStrategyName.assign("No Strategies");

    if (!m_instance)
        m_instance = this;
}

// Resets strategy state and releases the owned collections.
StrategyManager::~StrategyManager()
{
    reset();

    if (m_instance == this)
        m_instance = 0;
}

// Static access to the concrete strategy manager, whose destructor is nonvirtual.
StrategyManager* StrategyManager::getInstance()
{
    return m_instance;
}

void StrategyManager::setInstance(StrategyManager* instance)
{
    m_instance = instance;
}

bool StrategyManager::getEnableDebug()
{
    return m_enableDebug;
}

void StrategyManager::setEnableDebug(bool on)
{
    m_enableDebug = on;
}

Strategy* StrategyManager::getActiveStrategy()
{
    return m_activeStrategy;
}

// by value - the shipped function is `__stdcall` with a hidden return slot, i.e. it copies the
// member rather than handing back a reference to it.
std::string StrategyManager::getActiveStrategyName()
{
    return m_activeStrategyName;
}

StrategyPrerequisite* StrategyManager::getActivePrerequisite()
{
    return m_activePrerequisite;
}

// the cursor is checked before it is dereferenced, and a miss returns null rather than a sentinel.
StrategyCondition* StrategyManager::getActiveCondition()
{
    if (m_activeCondition == m_conditions.end())
        return 0;
    return m_activeCondition->second;
}

// 1-based, and unclamped - see the member note in the header. `team == 0` reads the dword that
// holds `m_enableDebug`.
Strategy* StrategyManager::getForcedStrategy(int team)
{
    return m_forcedStrategy[team - 1];
}

// [FRONTIER] Remaining strategy-management operations require strategy and condition behavior.
// reset is also reached by destruction.
// [RETIRE-WHEN: aidll-02-skeleton] Replace each stop when its corresponding behavior is recovered.
void StrategyManager::reset()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::reset");
}

// PE 10095A00. Callers in the strategy evaluators discard the result.
bool StrategyManager::isDebugEnabled()
{
    return m_enableDebug;
}

std::string StrategyManager::getActivePrerequisiteName()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::getActivePrerequisiteName");
}

std::string StrategyManager::getActiveConditionName()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::getActiveConditionName");
}

// PE 10098400.
Strategy* StrategyManager::getStrategy(std::string name)
{
    const std::map<std::string, Strategy*>::iterator it = m_strategies.find(name);
    return it == m_strategies.end() ? 0 : it->second;
}

// PE 10098870.
StrategyCondition* StrategyManager::getCondition(std::string name)
{
    const std::map<std::string, StrategyCondition*>::iterator it = m_conditions.find(name);
    return it == m_conditions.end() ? 0 : it->second;
}

// PE 10098780.
StrategyPrerequisite* StrategyManager::getPrerequisite(std::string name)
{
    const std::map<std::string, StrategyPrerequisite*>::iterator it = m_prerequisites.find(name);
    return it == m_prerequisites.end() ? 0 : it->second;
}

void StrategyManager::setActiveStrategy(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::setActiveStrategy");
}

void StrategyManager::setActivePrerequisite(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::setActivePrerequisite");
}

void StrategyManager::setActiveCondition(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::setActiveCondition");
}

// PE 10099360. Without an active strategy this reports an error (fatal in this tree).
void StrategyManager::setPrerequisite(std::string name)
{
    if (!m_activeStrategy)
    {
        Debug(DebugTypeError, __FILE__, 182, "")
            << "Cannot set prerequisite " << name << " - there is no active strategy.";
        return;
    }
    m_activeStrategy->setPrerequisite(getPrerequisite(name));
}

void StrategyManager::forceStrategy(std::string, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::forceStrategy");
}

// PE 1009A250. A duplicate name clears the active strategy.
bool StrategyManager::createStrategy(std::string name)
{
    if (m_strategies.find(name) == m_strategies.end())
    {
        Strategy* const strategy = checkPointer(new Strategy(name));
        m_strategies[name] = strategy;
        m_activeStrategyName = name;
        m_activeStrategy = strategy;
        return true;
    }

    m_activeStrategyName = "NULL";
    m_activeStrategy = 0;
    return false;
}

// PE 1009A320. A duplicate name leaves the active prerequisite unchanged.
bool StrategyManager::createPrerequisite(std::string name)
{
    if (m_prerequisites.find(name) != m_prerequisites.end())
        return false;

    StrategyPrerequisite* const prerequisite = checkPointer(new StrategyPrerequisite(name));
    m_prerequisites[name] = prerequisite;
    m_activePrerequisiteName = name;
    m_activePrerequisite = prerequisite;
    return true;
}

// PE 10099550. A duplicate name leaves no condition selected.
bool StrategyManager::createConstantCondition(std::string name,
                                              StrategyCondition::CompareType compareType,
                                              StrategyCondition::Compare compare,
                                              StrategyCondition::ConditionSide side,
                                              StrategyCondition::Conditional conditional,
                                              float value)
{
    if (m_conditions.find(name) != m_conditions.end())
    {
        m_activeCondition = m_conditions.end();
        return false;
    }

    StrategyCondition* const condition = checkPointer(
        new ConstantCondition(name, compareType, compare, side, conditional, value, 0.0f));
    m_activeCondition = m_conditions.insert(std::make_pair(name, condition)).first;
    return true;
}

// PE 10099510. The active prerequisite is not checked.
void StrategyManager::addCondition(std::string name)
{
    m_activePrerequisite->addCondition(getCondition(name), 1.0f);
}

// PE 100994D0. The active prerequisite is not checked.
void StrategyManager::addCondition(std::string name, float weight)
{
    m_activePrerequisite->addCondition(getCondition(name), weight);
}

void StrategyManager::addSpecificObjectModifier(std::string, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::addSpecificObjectModifier");
}

void StrategyManager::addRequiredPrecedingStrategy(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::addRequiredPrecedingStrategy");
}

void StrategyManager::addProhibitedPrecedingStrategy(std::string)
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::addProhibitedPrecedingStrategy");
}

void StrategyManager::completeStrategies()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::completeStrategies");
}

void StrategyManager::completeConditions()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::completeConditions");
}

void StrategyManager::listStrategies()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::listStrategies");
}

void StrategyManager::listPrerequisites()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::listPrerequisites");
}

void StrategyManager::listConditions()
{
    BF2_AI_FRONTIER("dice::hfe::ai::StrategyManager::listConditions");
}

// Console bindings for aiStrategy. Registration order is [CHOSEN, not recovered]; only the
// commands the shipped default strategy scripts use are bound.
// [RETIRE-WHEN: aistrategy-console] Bind the remaining aiStrategy commands as their bodies land.

namespace {

const char* const kAiStrategyConsoleTag = "aiStrategy";

StrategyManager* consoleStrategyManager()
{
    return StrategyManager::getInstance();
}

bool consoleStrategyHasActiveStrategy()
{
    return StrategyManager::getInstance() != 0
        && StrategyManager::getInstance()->getActiveStrategy() != 0;
}

bool consoleStrategyHasActiveCondition()
{
    return StrategyManager::getInstance() != 0
        && StrategyManager::getInstance()->getActiveCondition() != 0;
}

Strategy* consoleActiveStrategy()
{
    return StrategyManager::getInstance()->getActiveStrategy();
}

bool consoleCreateStrategy(const std::string& name)
{
    return consoleStrategyManager()->createStrategy(name);
}

bool consoleCreatePrerequisite(const std::string& name)
{
    return consoleStrategyManager()->createPrerequisite(name);
}

bool consoleCreateConstantCondition(const std::string& name,
                                    StrategyCondition::CompareType compareType,
                                    StrategyCondition::Compare compare,
                                    StrategyCondition::ConditionSide side,
                                    StrategyCondition::Conditional conditional, float value)
{
    return consoleStrategyManager()->createConstantCondition(name, compareType, compare, side,
                                                             conditional, value);
}

void consoleSetConditionStrength(StrategyCondition::ConditionStrength strength)
{
    consoleStrategyManager()->getActiveCondition()->setStrength(strength);
}

void consoleAddCondition(const std::string& name)
{
    consoleStrategyManager()->addCondition(name);
}

void consoleAddCondition(const std::string& name, float weight)
{
    consoleStrategyManager()->addCondition(name, weight);
}

void consoleSetPrerequisite(const std::string& name)
{
    consoleStrategyManager()->setPrerequisite(name);
}

void consoleSetAggression(float value)    { consoleActiveStrategy()->setAggression(value); }
void consoleSetNumberOfAttacks(int value) { consoleActiveStrategy()->setNumberOfAttacks(value); }
void consoleSetNumberOfDefences(int value){ consoleActiveStrategy()->setNumberOfDefences(value); }

void consoleSetStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value)
{
    consoleActiveStrategy()->setStrategicObjectsModifier(type, value);
}

void consoleSetStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value,
                                        StatusMap::Status status)
{
    consoleActiveStrategy()->setStrategicObjectsModifier(type, value, status);
}

} // anonymous namespace

BF2_CONSOLE_OBJECT("aiStrategy", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(createStrategy, consoleCreateStrategy, bool,
                                StrategyManager::getInstance() != 0,
                                std::string, kAiStrategyConsoleTag)
BF2_CONSOLE_PROPERTY_FN(aggression, float, consoleStrategyHasActiveStrategy(),
                        consoleActiveStrategy()->getAggression(), consoleSetAggression,
                        kAiStrategyConsoleTag)
BF2_CONSOLE_PROPERTY_FN(numberOfAttacks, int, consoleStrategyHasActiveStrategy(),
                        consoleActiveStrategy()->getNumberOfAttacks(), consoleSetNumberOfAttacks,
                        kAiStrategyConsoleTag)
BF2_CONSOLE_PROPERTY_FN(numberOfDefences, int, consoleStrategyHasActiveStrategy(),
                        consoleActiveStrategy()->getNumberOfDefences(), consoleSetNumberOfDefences,
                        kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_2_3_ACTIVE(setStrategicObjectsModifier, consoleSetStrategicObjectsModifier,
                              consoleStrategyHasActiveStrategy(),
                              AIStrategicObject::StrategicObjectType, float, StatusMap::Status,
                              kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setPrerequisite, consoleSetPrerequisite,
                            StrategyManager::getInstance() != 0,
                            std::string, kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(createPrerequisite, consoleCreatePrerequisite, bool,
                                StrategyManager::getInstance() != 0,
                                std::string, kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_1_2_ACTIVE(addCondition, consoleAddCondition,
                              StrategyManager::getInstance() != 0,
                              std::string, float, kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_6_RET_ACTIVE(createConstantCondition, consoleCreateConstantCondition, bool,
                                StrategyManager::getInstance() != 0,
                                std::string, StrategyCondition::CompareType,
                                StrategyCondition::Compare, StrategyCondition::ConditionSide,
                                StrategyCondition::Conditional, float, kAiStrategyConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setConditionStrength, consoleSetConditionStrength,
                            consoleStrategyHasActiveCondition(),
                            StrategyCondition::ConditionStrength, kAiStrategyConsoleTag)

// Handed to the console registry by AIModule::init.
ConsoleObjectList& aiStrategyConsoleObjects()
{
    return consoleBindingList(kAiStrategyConsoleTag);
}

} // namespace ai
} // namespace hfe
} // namespace dice
