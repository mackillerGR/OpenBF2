#include "BAPConditions.h"

#include <ostream>

#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Value 48 prints nothing.
const char* const kConditionTypePrintNames[] =
{
    "LookDeviation", "Precision", "Precision3d", "BombPrecision3d", "&&", "EndOfPath", "false",
    "Generic1Arg", "Generic2Arg", "InsideBattleZone", "Iteration", "LineOfFire",
    "PointDistance", "PointInFront", "!", "DistanceToProj", "StrategicObjectStatus",
    "ObjectBehind", "ObjectDistance", "ObjectDistanceToPoint", "ObjectEnemyOccupied",
    "ObjectExists", "ObjectHealth", "ObjectInFront", "ObjectInLandingZone",
    "ObjectInsideFrustum", "ObjectLineOfFire", "ObjectManDown", "ObjectMoved", "ObjectOccupied",
    "ObjectSpeed", "ObjectTouchingGround", "ObjectValidAiming", "ObjectValidPosition",
    "ObjectVisible", "||", "PointAt", "Position", "SharedCondition", "Time", "true",
    "WeaponMagAmmo", "WeaponTotalAmmo", "TotalWeaponAmmo", "WeaponReadyToFire",
    "WeaponTargetReachable", "WeaponHeat", "WeaponHasLockOnTarget", 0, "IsTriggered",
    "Allow fire", "Sprinting", "Zoomed", "Detonator active", "Weapon active", "<",
    "Ok to throw grenade", "Splash damage", "Triggerable Active",
};

} // anonymous namespace

// PE 100D8620. Values outside the table print nothing. [INFERRED] placement.
std::ostream& operator<<(std::ostream& out, BAPConditionType type)
{
    const unsigned int index = static_cast<unsigned int>(type);
    if (index < sizeof(kConditionTypePrintNames) / sizeof(kConditionTypePrintNames[0]) &&
        kConditionTypePrintNames[index])
        out << kConditionTypePrintNames[index];
    return out;
}

void BAPCondition::printDebug(std::ostream& out)
{
    out << getType();
}

BAPCondition::~BAPCondition()
{
}

BAPConditionList::~BAPConditionList()
{
}

// PE 100F2910 / 100F2920 / 100F2930.
bool BAPConTrue::evaluate(Bot*) { return true; }
BAPConditionType BAPConTrue::getType() { return static_cast<BAPConditionType>(40); }
BAPConTrue::~BAPConTrue() {}

// PE 100F20B0 / 100F20C0 / 100F20D0.
bool BAPConFalse::evaluate(Bot*) { return false; }
BAPConditionType BAPConFalse::getType() { return static_cast<BAPConditionType>(6); }
BAPConFalse::~BAPConFalse() {}

// The condition is claimed before it joins the list, at the front.
BAPConNot::BAPConNot(BAPCondition* condition)
{
    condition->m_owner = this;
    m_conditions.push_front(condition);
}

// PE 101A9B10.
bool BAPConNot::evaluate(Bot* bot)
{
    return !m_conditions.front()->evaluate(bot);
}

BAPConditionType BAPConNot::getType() { return static_cast<BAPConditionType>(14); }

// PE 101A9A40. [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPConNot::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPConNot::printDebug");
}

// PE 101A9A90: the conditions are destroyed before the list goes.
BAPConNot::~BAPConNot()
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// PE 101A9D80. Neither the equipment nor its armament is null-checked, and the active weapon is
// looked up twice, the first result unused, as shipped.
bool BAPConZoomed::evaluate(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = equipment->getArmament();
    armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    return weapon->isZoomed();
}

BAPConditionType BAPConZoomed::getType() { return static_cast<BAPConditionType>(52); }

// PE 101A9D50.
BAPConZoomed::~BAPConZoomed() {}

} // namespace ai
} // namespace hfe
} // namespace dice
