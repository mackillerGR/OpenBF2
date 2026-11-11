// Statement types of a bot action plan, and the script spellings of the two plan enumerations.
// [INFERRED] placement: the image carries no __FILE__ string for these declarations.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BAPSTATEMENTTYPE_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_BAPSTATEMENTTYPE_H

#include <istream>

namespace dice {
namespace hfe {
namespace ai {

// Enumerator names are the script spellings. Value 30 has no script spelling.
enum BAPStatementType : int
{
    BAPSTParallel = 0,
    BAPSTSerial,
    BAPSTPlanEnd,
    BAPSTIf,
    BAPSTWait,
    BAPSTWhile,
    BAPSTAimAt,
    BAPSTMoveTo,
    BAPSTMoveToDirection,
    BAPSTMoveToObject,
    BAPSTMoveAwayFromObject,
    BAPSTTrigger,
    BAPSTTriggerContinously,
    BAPSTIdle,
    BAPSTResetControls,
    BAPSTInfoWrapper,
    BAPSTAimingWrapper,
    BAPSTFinishedWrapper,
    BAPSTWeaponWrapper,
    BAPSTLookAtWrapper,
    BAPSTSense,
    BAPSTOverrideSensingAgent,
    BAPSTPlaceC4,
    BAPSTLookAt,
    BAPSTTurn,
    BAPSTRoll3d,
    BAPSTSoldierPose,
    BAPSTUpdateVehicle,
    BAPSTChangeWeapon,
    BAPSTICChangeVehicle,
    BAPSTUnnamed30,              // [UNRESOLVED] no extractor spelling
    BAPSTICTurnLock,
    BAPSTEvaluate,
    BAPSTNotifyBot,
    BAPSTPhysicsEnable,
    BAPSTSetSensingAgentFocus,
    BAPSTMergeSoldierInputs,
    BAPSTSendRadioMessage,
    BAPSTDebug,
    BAPSTForgetTarget,
    BAPSTUseFlareOnMissileLock,
    BAPSTRunOverObject,
    BAPSTHonkHorn,
    BAPSTEvaluateCondition,
    BAPSTRevive,
    BAPSTCount                   // 45: the interpreter's row count and the extractor's unknown value
};

enum BotLodLevel : int;

// Each statement type accepts its full spelling or the spelling without the BAPST prefix (the two
// IC types drop the IC as well); an unrecognised token yields BAPSTCount. PE 100DB980.
std::istream& operator>>(std::istream& stream, BAPStatementType& type);

// Accepts BLLFull / Full / 0, BLLMedium / Medium / 1 and BLLLight / Light / 2; anything else,
// including BLLNone and None, yields 3. PE 100DB840.
std::istream& operator>>(std::istream& stream, BotLodLevel& level);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BAPSTATEMENTTYPE_H
