#include "BotScript.h"

#include <ostream>

#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

int BotScript::s_created = 0;
int BotScript::s_destroyed = 0;

BotScript::BotScript()
    : m_owner(0)
    , m_inputMask(0)
{
    ++s_created;
}

// PE 10198F00 (deleting form).
BotScript::~BotScript()
{
    ++s_destroyed;
}

// PE 10198ED0.
unsigned long long BotScript::getInputMask()
{
    return m_inputMask;
}

// PE 101DAC90.
void BotScript::getDebugString(Bot*, std::string& out)
{
    out = "Standard debug.";
}

// PE 101DACB0.
void BotScript::printDebug(std::ostream& out)
{
    IBotManager::instance->tabulate(out);
    out << getType() << std::endl;
}

namespace {

// The printer's spellings differ from the script spellings in two places ("if", "while"); value
// 30 prints nothing.
const char* const kStatementTypePrintNames[] =
{
    "Parallel", "Serial", "PlanEnd", "if", "Wait", "while", "AimAt", "MoveTo",
    "MoveToDirection", "MoveToObject", "MoveAwayFromObject", "Trigger", "TriggerContinously",
    "Idle", "ResetControls", "InfoWrapper", "AimingWrapper", "FinishedWrapper", "WeaponWrapper",
    "LookAtWrapper", "Sense", "OverrideSensingAgent", "PlaceC4", "LookAt", "Turn", "Roll3d",
    "SoldierPose", "UpdateVehicle", "ChangeWeapon", "ICChangeVehicle", 0, "ICTurnLock",
    "Evaluate", "NotifyBot", "PhysicsEnable", "SetSensingAgentFocus", "MergeSoldierInputs",
    "SendRadioMessage", "Debug", "ForgetTarget", "UseFlareOnMissileLock", "RunOverObject",
    "HonkHorn", "EvaluateCondition", "Revive",
};

} // anonymous namespace

// PE 100D8B70. Values outside the table print nothing. [INFERRED] placement.
std::ostream& operator<<(std::ostream& out, BAPStatementType type)
{
    const unsigned int index = static_cast<unsigned int>(type);
    if (index < sizeof(kStatementTypePrintNames) / sizeof(kStatementTypePrintNames[0]) &&
        kStatementTypePrintNames[index])
        out << kStatementTypePrintNames[index];
    return out;
}

} // namespace ai
} // namespace hfe
} // namespace dice
