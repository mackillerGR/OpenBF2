// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BAPEntries operations.

#include "BAPEntries.h"

#include "../Main/AIDllFrontier.h"
#include "PlanInterpreter.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPStatements.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// PE 101C4530, a free helper ([INFERRED] name). Releases the soldier's movement group (action,
// sprint, yaw, pitch, roll, throttle and both mouse-look axes), points the vertical look along the
// equipment's transform, and releases the action group. Writes to unbound inputs are dropped.
// The equipment is not null-checked, as shipped.
bool resetSoldierControls(Bot* bot, io::PlayerInput* input, bool movement, bool look, bool actions)
{
    if (movement)
    {
        input->setInput(io::PIAction, 0.0f);
        input->setInput(io::PISprint, 0.0f);
        input->setInput(io::PIYaw, 0.0f);
        input->setInput(io::PIRoll, 0.0f);
        input->setInput(io::PIPitch, 0.0f);
        input->setInput(io::PIThrottle, 0.0f);
        input->setInput(io::PIMouseLookX, 0.0f);
        input->setInput(io::PIMouseLookY, 0.0f);
    }
    if (look)
    {
        Information* const equipment =
            informationManager->getInformation(bot->getCurrentEquipment());
        const Mat4& transform = equipment->getTransformation();
        input->setInput(io::PIMouseLookX, 0.0f);
        input->setInput(io::PIMouseLookY, -transform.m[9]);
    }
    if (actions)
    {
        input->setInput(io::PIFire, 0.0f);
        input->setInput(io::PIAltFire, 0.0f);
        for (int each = io::PIWeaponSelect1; each <= io::PIWeaponSelect8; ++each)
            input->setInput(static_cast<io::PlayerInputMap>(each), 0.0f);
        for (int each = io::PIPositionSelect1; each <= io::PIPositionSelect8; ++each)
            input->setInput(static_cast<io::PlayerInputMap>(each), 0.0f);
        input->setInput(io::PIUse, 0.0f);
        input->setInput(io::PIReload, 0.0f);
        input->setInput(io::PIFlareFire, 0.0f);
    }
    return true;
}

} // anonymous namespace

// Action-plan primitives return their statement type for registry dispatch.

// Empty body; see BAPEntry.h.
BAPEntry::~BAPEntry()
{
}

EntryTankMoveTo::EntryTankMoveTo()
{
}

EntryTankMoveTo::~EntryTankMoveTo()
{
}

int EntryTankMoveTo::getTargetId()
{
    return 7;
}

bool EntryTankMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankMoveTo::execute");
}


EntryTankMoveToObject::EntryTankMoveToObject()
{
}

EntryTankMoveToObject::~EntryTankMoveToObject()
{
}

int EntryTankMoveToObject::getTargetId()
{
    return 9;
}

bool EntryTankMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankMoveToObject::execute");
}


EntryTankMoveToDirection::EntryTankMoveToDirection()
{
}

EntryTankMoveToDirection::~EntryTankMoveToDirection()
{
}

int EntryTankMoveToDirection::getTargetId()
{
    return 8;
}

bool EntryTankMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankMoveToDirection::execute");
}


EntryTankResetControls::EntryTankResetControls()
{
}

EntryTankResetControls::~EntryTankResetControls()
{
}

int EntryTankResetControls::getTargetId()
{
    return 14;
}

bool EntryTankResetControls::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankResetControls::execute");
}


EntryTankTurnTo::EntryTankTurnTo()
{
}

EntryTankTurnTo::~EntryTankTurnTo()
{
}

int EntryTankTurnTo::getTargetId()
{
    return 24;
}

bool EntryTankTurnTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankTurnTo::execute");
}

void EntryTankTurnTo::getCentreOfFacingSide(const Vec2&, const Vec2&, const Vec2&,
                                           const Vec2&, Vec2&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTankTurnTo::getCentreOfFacingSide");
}


EntryCarMoveTo::EntryCarMoveTo()
{
}

EntryCarMoveTo::~EntryCarMoveTo()
{
}

int EntryCarMoveTo::getTargetId()
{
    return 7;
}

bool EntryCarMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryCarMoveTo::execute");
}


EntryCarMoveToDirection::EntryCarMoveToDirection()
{
}

EntryCarMoveToDirection::~EntryCarMoveToDirection()
{
}

int EntryCarMoveToDirection::getTargetId()
{
    return 8;
}

// A retail stub - see the block above. , and all it does is succeed.
bool EntryCarMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    return true;
}


EntryCarMoveToObject::EntryCarMoveToObject()
{
}

EntryCarMoveToObject::~EntryCarMoveToObject()
{
}

int EntryCarMoveToObject::getTargetId()
{
    return 9;
}

bool EntryCarMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryCarMoveToObject::execute");
}


EntryCarResetControls::EntryCarResetControls()
{
}

EntryCarResetControls::~EntryCarResetControls()
{
}

int EntryCarResetControls::getTargetId()
{
    return 14;
}

bool EntryCarResetControls::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryCarResetControls::execute");
}


EntryPlaneMoveTo::EntryPlaneMoveTo()
{
}

EntryPlaneMoveTo::~EntryPlaneMoveTo()
{
}

int EntryPlaneMoveTo::getTargetId()
{
    return 7;
}

bool EntryPlaneMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryPlaneMoveTo::execute");
}


EntryPlaneMoveToDirection::EntryPlaneMoveToDirection()
{
}

EntryPlaneMoveToDirection::~EntryPlaneMoveToDirection()
{
}

int EntryPlaneMoveToDirection::getTargetId()
{
    return 8;
}

bool EntryPlaneMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryPlaneMoveToDirection::execute");
}


EntryPlaneMoveToObject::EntryPlaneMoveToObject()
{
}

EntryPlaneMoveToObject::~EntryPlaneMoveToObject()
{
}

int EntryPlaneMoveToObject::getTargetId()
{
    return 9;
}

bool EntryPlaneMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryPlaneMoveToObject::execute");
}


EntryPlaneAimAt::EntryPlaneAimAt()
{
}

EntryPlaneAimAt::~EntryPlaneAimAt()
{
}

int EntryPlaneAimAt::getTargetId()
{
    return 6;
}

bool EntryPlaneAimAt::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryPlaneAimAt::execute");
}


EntryPlaneResetControls::EntryPlaneResetControls()
{
}

EntryPlaneResetControls::~EntryPlaneResetControls()
{
}

int EntryPlaneResetControls::getTargetId()
{
    return 14;
}

bool EntryPlaneResetControls::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryPlaneResetControls::execute");
}


EntryBoatMoveTo::EntryBoatMoveTo()
{
}

EntryBoatMoveTo::~EntryBoatMoveTo()
{
}

int EntryBoatMoveTo::getTargetId()
{
    return 7;
}

bool EntryBoatMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryBoatMoveTo::execute");
}


EntryBoatMoveToObject::EntryBoatMoveToObject()
{
}

EntryBoatMoveToObject::~EntryBoatMoveToObject()
{
}

int EntryBoatMoveToObject::getTargetId()
{
    return 9;
}

bool EntryBoatMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryBoatMoveToObject::execute");
}


EntryBoatMoveToDirection::EntryBoatMoveToDirection()
{
}

EntryBoatMoveToDirection::~EntryBoatMoveToDirection()
{
}

int EntryBoatMoveToDirection::getTargetId()
{
    return 8;
}

bool EntryBoatMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryBoatMoveToDirection::execute");
}


EntryBoatResetControls::EntryBoatResetControls()
{
}

EntryBoatResetControls::~EntryBoatResetControls()
{
}

int EntryBoatResetControls::getTargetId()
{
    return 14;
}

bool EntryBoatResetControls::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryBoatResetControls::execute");
}


EntryInfanteryMoveTo::EntryInfanteryMoveTo()
{
}

EntryInfanteryMoveTo::~EntryInfanteryMoveTo()
{
}

int EntryInfanteryMoveTo::getTargetId()
{
    return 7;
}

bool EntryInfanteryMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryInfanteryMoveTo::execute");
}


EntryInfanteryMoveToObject::EntryInfanteryMoveToObject()
{
}

EntryInfanteryMoveToObject::~EntryInfanteryMoveToObject()
{
}

int EntryInfanteryMoveToObject::getTargetId()
{
    return 9;
}

bool EntryInfanteryMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryInfanteryMoveToObject::execute");
}


EntryInfanteryMoveToDirection::EntryInfanteryMoveToDirection()
{
}

EntryInfanteryMoveToDirection::~EntryInfanteryMoveToDirection()
{
}

int EntryInfanteryMoveToDirection::getTargetId()
{
    return 8;
}

// A retail stub - see the block above. , and all it does is succeed.
bool EntryInfanteryMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    return true;
}


EntryInfanteryResetControls::EntryInfanteryResetControls()
{
}

EntryInfanteryResetControls::~EntryInfanteryResetControls()
{
}

int EntryInfanteryResetControls::getTargetId()
{
    return 14;
}

// PE 101345A0. A statement with an input mask releases its one input; any other releases the
// groups it names through the soldier helper above.
bool EntryInfanteryResetControls::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPResetControls* const reset = static_cast<BAPResetControls*>(script);
    if (reset->hasInputMask())
    {
        bot->playerInput()->setInput(static_cast<io::PlayerInputMap>(reset->m_input), false);
        finished = true;
        failed = true;
        return true;
    }
    resetSoldierControls(bot, bot->playerInput(), reset->m_movement, reset->m_look, reset->m_actions);
    finished = true;
    failed = true;
    return true;
}


EntryTrigger::EntryTrigger()
{
}

EntryTrigger::~EntryTrigger()
{
}

int EntryTrigger::getTargetId()
{
    return 11;
}

bool EntryTrigger::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTrigger::execute");
}


EntryTriggerContinously::EntryTriggerContinously()
{
}

EntryTriggerContinously::~EntryTriggerContinously()
{
}

int EntryTriggerContinously::getTargetId()
{
    return 12;
}

bool EntryTriggerContinously::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryTriggerContinously::execute");
}


EntryKeyTurretAimAt::EntryKeyTurretAimAt()
{
}

EntryKeyTurretAimAt::~EntryKeyTurretAimAt()
{
}

int EntryKeyTurretAimAt::getTargetId()
{
    return 6;
}

bool EntryKeyTurretAimAt::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryKeyTurretAimAt::execute");
}


EntryMouseTurretAimAt::EntryMouseTurretAimAt()
{
}

EntryMouseTurretAimAt::~EntryMouseTurretAimAt()
{
}

int EntryMouseTurretAimAt::getTargetId()
{
    return 6;
}

bool EntryMouseTurretAimAt::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMouseTurretAimAt::execute");
}


EntryMouseTurretLookAt::EntryMouseTurretLookAt()
{
}

EntryMouseTurretLookAt::~EntryMouseTurretLookAt()
{
}

int EntryMouseTurretLookAt::getTargetId()
{
    return 23;
}

bool EntryMouseTurretLookAt::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMouseTurretLookAt::execute");
}


EntrySense::EntrySense()
{
}

EntrySense::~EntrySense()
{
}

int EntrySense::getTargetId()
{
    return 20;
}

bool EntrySense::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntrySense::execute");
}


EntrySoldierPose::EntrySoldierPose()
{
}

EntrySoldierPose::~EntrySoldierPose()
{
}

int EntrySoldierPose::getTargetId()
{
    return 26;
}

bool EntrySoldierPose::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntrySoldierPose::execute");
}


EntryMoveToMedium::EntryMoveToMedium()
{
}

EntryMoveToMedium::~EntryMoveToMedium()
{
}

int EntryMoveToMedium::getTargetId()
{
    return 7;
}

bool EntryMoveToMedium::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToMedium::execute");
}


EntryMoveToMediumBoat::EntryMoveToMediumBoat()
{
}

EntryMoveToMediumBoat::~EntryMoveToMediumBoat()
{
}

int EntryMoveToMediumBoat::getTargetId()
{
    return 7;
}

bool EntryMoveToMediumBoat::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToMediumBoat::execute");
}


EntryMoveToMediumSoldier::EntryMoveToMediumSoldier()
{
}

EntryMoveToMediumSoldier::~EntryMoveToMediumSoldier()
{
}

int EntryMoveToMediumSoldier::getTargetId()
{
    return 7;
}

bool EntryMoveToMediumSoldier::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToMediumSoldier::execute");
}


EntryMoveToObjectMedium::EntryMoveToObjectMedium()
{
}

EntryMoveToObjectMedium::~EntryMoveToObjectMedium()
{
}

int EntryMoveToObjectMedium::getTargetId()
{
    return 9;
}

bool EntryMoveToObjectMedium::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToObjectMedium::execute");
}


EntryMoveToObjectMediumBoat::EntryMoveToObjectMediumBoat()
{
}

EntryMoveToObjectMediumBoat::~EntryMoveToObjectMediumBoat()
{
}

int EntryMoveToObjectMediumBoat::getTargetId()
{
    return 9;
}

bool EntryMoveToObjectMediumBoat::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToObjectMediumBoat::execute");
}


EntryMoveToObjectMediumSoldier::EntryMoveToObjectMediumSoldier()
{
}

EntryMoveToObjectMediumSoldier::~EntryMoveToObjectMediumSoldier()
{
}

int EntryMoveToObjectMediumSoldier::getTargetId()
{
    return 9;
}

bool EntryMoveToObjectMediumSoldier::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryMoveToObjectMediumSoldier::execute");
}


EntryInfoWrapper::EntryInfoWrapper()
{
}

EntryInfoWrapper::~EntryInfoWrapper()
{
}

int EntryInfoWrapper::getTargetId()
{
    return 15;
}

// PE 10130860. Runs the wrapped statement.
bool EntryInfoWrapper::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    return static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(
        static_cast<BAPWrapper*>(script)->m_child, bot, finished, failed);
}


EntryHeliMoveTo::EntryHeliMoveTo()
{
}

EntryHeliMoveTo::~EntryHeliMoveTo()
{
}

int EntryHeliMoveTo::getTargetId()
{
    return 7;
}

bool EntryHeliMoveTo::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliMoveTo::execute");
}


EntryHeliMoveToDirection::EntryHeliMoveToDirection()
{
}

EntryHeliMoveToDirection::~EntryHeliMoveToDirection()
{
}

int EntryHeliMoveToDirection::getTargetId()
{
    return 8;
}

bool EntryHeliMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliMoveToDirection::execute");
}


EntryHeliMoveToObject::EntryHeliMoveToObject()
{
}

EntryHeliMoveToObject::~EntryHeliMoveToObject()
{
}

int EntryHeliMoveToObject::getTargetId()
{
    return 9;
}

bool EntryHeliMoveToObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliMoveToObject::execute");
}


EntryHeliAimAt::EntryHeliAimAt()
{
}

EntryHeliAimAt::~EntryHeliAimAt()
{
}

int EntryHeliAimAt::getTargetId()
{
    return 6;
}

bool EntryHeliAimAt::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliAimAt::execute");
}


EntryHeliResetControls::EntryHeliResetControls()
{
}

EntryHeliResetControls::~EntryHeliResetControls()
{
}

int EntryHeliResetControls::getTargetId()
{
    return 14;
}

bool EntryHeliResetControls::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliResetControls::execute");
}


EntryHeliMoveAwayFromObject::EntryHeliMoveAwayFromObject()
{
}

EntryHeliMoveAwayFromObject::~EntryHeliMoveAwayFromObject()
{
}

int EntryHeliMoveAwayFromObject::getTargetId()
{
    return 10;
}

bool EntryHeliMoveAwayFromObject::execute(BotScript *, Bot *, bool&, bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::EntryHeliMoveAwayFromObject::execute");
}


} // namespace ai
} // namespace hfe
} // namespace dice
