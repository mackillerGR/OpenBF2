#include "PlanInterpreter.h"

#include "BAPEntry.h"
#include "BAPStatementType.h"

#include <string>
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

PlanInterpreter::PlanInterpreter(int typeCount)
    : m_typeCount(typeCount)
{
    m_entries = new BAPEntry**[m_typeCount];
    for (int type = 0; type < m_typeCount; ++type)
    {
        m_entries[type] = new BAPEntry*[LevelCount];
        m_entries[type][0] = 0;
        m_entries[type][1] = 0;
        m_entries[type][2] = 0;
    }
    m_handled = new unsigned long long[LevelCount];
    m_handled[0] = 0;
    m_handled[1] = 0;
    m_handled[2] = 0;
}

PlanInterpreter::~PlanInterpreter()
{
    for (int type = 0; type < m_typeCount; ++type)
    {
        for (int level = 0; level < LevelCount; ++level)
        {
            if (m_entries[type][level])
                m_entries[type][level] = 0;
        }
        delete[] m_entries[type];
        m_entries[type] = 0;
    }
    delete[] m_entries;
    m_entries = 0;
    delete[] m_handled;
    m_handled = 0;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-plan-execution] Replace with
// the PE 10184940 body once BotScript's statement accessors are recovered.
bool PlanInterpreter::execute(BotScript*, Bot*, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::PlanInterpreter::execute");
}

bool PlanInterpreter::addEntry(BotLodLevel level, BAPEntry* entry)
{
    const int type = entry->getTargetId();
    if (m_typeCount <= type)
        return false;
    if (m_entries[type][level])
        return false;
    for (int each = level; each < LevelCount; ++each)
    {
        if (m_entries[type][each])
            break;
        m_entries[type][each] = entry;
        m_handled[each] |= 1ULL << (type & 63);
    }
    return true;
}

bool PlanInterpreter::addEntry(BAPEntry* entry)
{
    return addEntry(BotLodFull, entry);
}

bool PlanInterpreter::removeEntry(BotLodLevel level, BAPEntry* entry)
{
    const int type = entry->getTargetId();
    if (m_typeCount <= type)
        return false;
    for (int each = level; each < LevelCount; ++each)
    {
        if (m_entries[type][each] == entry)
        {
            m_entries[type][each] = 0;
            m_handled[each] &= ~(1ULL << (type & 63));
        }
    }
    return true;
}

bool PlanInterpreter::activateDefault(BAPStatementType type, BotLodLevel level)
{
    if (m_typeCount <= type)
        return false;
    if (m_entries[type][level])
        return false;
    for (int each = level; each < LevelCount; ++each)
    {
        if (m_entries[type][each])
            break;
        m_handled[each] |= 1ULL << (static_cast<int>(type) & 63);
    }
    return true;
}

// The script spellings, in the order the shipped extractor tests them. The short spelling of
// BAPSTAimAt is "AImAt", as shipped.
namespace {

struct StatementTypeName
{
    const char* full;
    const char* brief;
    BAPStatementType type;
};

const StatementTypeName kStatementTypeNames[] =
{
    { "BAPSTParallel", "Parallel", BAPSTParallel },
    { "BAPSTSerial", "Serial", BAPSTSerial },
    { "BAPSTPlanEnd", "PlanEnd", BAPSTPlanEnd },
    { "BAPSTIf", "If", BAPSTIf },
    { "BAPSTWait", "Wait", BAPSTWait },
    { "BAPSTWhile", "While", BAPSTWhile },
    { "BAPSTAimAt", "AImAt", BAPSTAimAt },
    { "BAPSTMoveTo", "MoveTo", BAPSTMoveTo },
    { "BAPSTMoveToDirection", "MoveToDirection", BAPSTMoveToDirection },
    { "BAPSTMoveToObject", "MoveToObject", BAPSTMoveToObject },
    { "BAPSTMoveAwayFromObject", "MoveAwayFromObject", BAPSTMoveAwayFromObject },
    { "BAPSTTrigger", "Trigger", BAPSTTrigger },
    { "BAPSTTriggerContinously", "TriggerContinously", BAPSTTriggerContinously },
    { "BAPSTIdle", "Idle", BAPSTIdle },
    { "BAPSTResetControls", "ResetControls", BAPSTResetControls },
    { "BAPSTInfoWrapper", "InfoWrapper", BAPSTInfoWrapper },
    { "BAPSTAimingWrapper", "AimingWrapper", BAPSTAimingWrapper },
    { "BAPSTFinishedWrapper", "FinishedWrapper", BAPSTFinishedWrapper },
    { "BAPSTWeaponWrapper", "WeaponWrapper", BAPSTWeaponWrapper },
    { "BAPSTLookAtWrapper", "LookAtWrapper", BAPSTLookAtWrapper },
    { "BAPSTSense", "Sense", BAPSTSense },
    { "BAPSTOverrideSensingAgent", "OverrideSensingAgent", BAPSTOverrideSensingAgent },
    { "BAPSTPlaceC4", "PlaceC4", BAPSTPlaceC4 },
    { "BAPSTLookAt", "LookAt", BAPSTLookAt },
    { "BAPSTTurn", "Turn", BAPSTTurn },
    { "BAPSTRoll3d", "Roll3d", BAPSTRoll3d },
    { "BAPSTSoldierPose", "SoldierPose", BAPSTSoldierPose },
    { "BAPSTUpdateVehicle", "UpdateVehicle", BAPSTUpdateVehicle },
    { "BAPSTChangeWeapon", "ChangeWeapon", BAPSTChangeWeapon },
    { "BAPSTICChangeVehicle", "ChangeVehicle", BAPSTICChangeVehicle },
    { "BAPSTICTurnLock", "TurnLock", BAPSTICTurnLock },
    { "BAPSTEvaluate", "Evaluate", BAPSTEvaluate },
    { "BAPSTNotifyBot", "NotifyBot", BAPSTNotifyBot },
    { "BAPSTPhysicsEnable", "PhysicsEnable", BAPSTPhysicsEnable },
    { "BAPSTSetSensingAgentFocus", "SetSensingAgentFocus", BAPSTSetSensingAgentFocus },
    { "BAPSTMergeSoldierInputs", "MergeSoldierInputs", BAPSTMergeSoldierInputs },
    { "BAPSTSendRadioMessage", "SendRadioMessage", BAPSTSendRadioMessage },
    { "BAPSTDebug", "Debug", BAPSTDebug },
    { "BAPSTForgetTarget", "ForgetTarget", BAPSTForgetTarget },
    { "BAPSTUseFlareOnMissileLock", "UseFlareOnMissileLock", BAPSTUseFlareOnMissileLock },
    { "BAPSTRunOverObject", "RunOverObject", BAPSTRunOverObject },
    { "BAPSTHonkHorn", "HonkHorn", BAPSTHonkHorn },
    { "BAPSTEvaluateCondition", "EvaluateCondition", BAPSTEvaluateCondition },
    { "BAPSTRevive", "Revive", BAPSTRevive },
};

} // anonymous namespace

std::istream& operator>>(std::istream& stream, BAPStatementType& type)
{
    std::string token;
    stream >> token;
    for (size_t i = 0; i < sizeof(kStatementTypeNames) / sizeof(kStatementTypeNames[0]); ++i)
    {
        if (token == kStatementTypeNames[i].full || token == kStatementTypeNames[i].brief)
        {
            type = kStatementTypeNames[i].type;
            return stream;
        }
    }
    type = BAPSTCount;
    return stream;
}

std::istream& operator>>(std::istream& stream, BotLodLevel& level)
{
    std::string token;
    stream >> token;
    if (token == "BLLFull" || token == "Full" || token == "0")
        level = static_cast<BotLodLevel>(0);
    else if (token == "BLLMedium" || token == "Medium" || token == "1")
        level = static_cast<BotLodLevel>(1);
    else if (token == "BLLLight" || token == "Light" || token == "2")
        level = static_cast<BotLodLevel>(2);
    else
        level = static_cast<BotLodLevel>(3);
    return stream;
}

} // namespace ai
} // namespace hfe
} // namespace dice
