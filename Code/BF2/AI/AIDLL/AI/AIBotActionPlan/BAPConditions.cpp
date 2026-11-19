#include "BAPConditions.h"
#include "../AIUtils/checkPointer.h"

#include <cfloat>
#include <cmath>
#include <ostream>

#include "../AIBot/BotMain.h"
#include "../../../../Math/Mat4.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "BAPStatements.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIBot/BotSenses.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/ObjectOccupancy.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
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

std::ostream& operator<<(std::ostream& out, BAPCondition& condition)
{
    condition.printDebug(out);
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

// PE 101A9A40.
void BAPConNot::printDebug(std::ostream& out)
{
    out << "NOT(" << *m_conditions.front() << ")";
}

void BAPConNot::addCondition(BAPCondition* condition)
{
    if (m_conditions.size() != 0)
        return;
    condition->m_owner = this;
    m_conditions.push_front(condition);
}

namespace {

// Prints the conditions between parentheses, separated by `separator`.
void printConditions(std::ostream& out, const std::list<BAPCondition*>& conditions, const char* separator)
{
    std::list<BAPCondition*>::const_iterator it = conditions.begin();
    out << "(";
    if (it != conditions.end())
    {
        out << **it;
        for (++it; it != conditions.end(); ++it)
            out << separator << **it;
    }
    out << ")";
}

// Destroys the owned conditions.
void destroyConditions(std::list<BAPCondition*>& conditions)
{
    for (std::list<BAPCondition*>::iterator it = conditions.begin(); it != conditions.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

} // anonymous namespace

bool BAPConAnd::evaluate(Bot* bot)
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (!(*it)->evaluate(bot))
            return false;
    }
    return true;
}

BAPConditionType BAPConAnd::getType() { return static_cast<BAPConditionType>(4); }

void BAPConAnd::printDebug(std::ostream& out) { printConditions(out, m_conditions, " && "); }

BAPConAnd::~BAPConAnd() { destroyConditions(m_conditions); }

void BAPConAnd::addCondition(BAPCondition* condition)
{
    condition->m_owner = this;
    m_conditions.push_back(condition);
}

bool BAPConOr::evaluate(Bot* bot)
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if ((*it)->evaluate(bot))
            return true;
    }
    return false;
}

BAPConditionType BAPConOr::getType() { return static_cast<BAPConditionType>(35); }

void BAPConOr::printDebug(std::ostream& out) { printConditions(out, m_conditions, " || "); }

BAPConOr::~BAPConOr() { destroyConditions(m_conditions); }

void BAPConOr::addCondition(BAPCondition* condition)
{
    condition->m_owner = this;
    m_conditions.push_back(condition);
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

namespace {

// PE 100E9DE0, the conditional Vec2 normaliser (file-local copies also live beside the statements
// and the squad code).
void normaliseFlat(Vec2& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = 0.0f;
        value.y = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
}

// PE 1014ADC0: the horizontal part (x, z) of a position.
Vec2 flatOf(const Vec3& position)
{
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;
    return flat;
}

} // anonymous namespace

BAPConPosition::BAPConPosition(const Vec2& point, float radius)
{
    m_flatPoint = new Vec2(point);
    m_radiusSq = radius * radius;
    m_spatial = false;
}

BAPConPosition::BAPConPosition(const Vec3& point, float radius)
{
    m_point = new Vec3(point);
    m_radiusSq = radius * radius;
    m_spatial = true;
}

// The flat test is strict and the spatial one inclusive, as shipped.
bool BAPConPosition::evaluate(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return true;
    const Vec3& position = equipment->getPosition();
    if (m_spatial)
    {
        const float dx = position.x - m_point->x;
        const float dy = position.y - m_point->y;
        const float dz = position.z - m_point->z;
        return dz * dz + dx * dx + dy * dy <= m_radiusSq;
    }
    const float dx = position.x - m_flatPoint->x;
    const float dz = position.z - m_flatPoint->y;
    return dz * dz + dx * dx < m_radiusSq;
}

BAPConditionType BAPConPosition::getType() { return static_cast<BAPConditionType>(37); }

BAPConPosition::~BAPConPosition()
{
    if (m_spatial)
        delete m_point;
    else
        delete m_flatPoint;
}

BAPConObjectMoved::BAPConObjectMoved(unsigned int object, float distance)
    : m_object(object), m_distanceSq(distance * distance)
{
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
}

bool BAPConObjectMoved::evaluate(Bot*)
{
    Information* const object = informationManager->getInformation(m_object);
    if (!object)
        return false;
    const Vec3& position = object->getPosition();
    const float dx = position.x - m_lastPosition.x;
    const float dy = position.y - m_lastPosition.y;
    const float dz = position.z - m_lastPosition.z;
    if (dz * dz + dy * dy + dx * dx <= m_distanceSq)
        return false;
    m_lastPosition = object->getPosition();
    return true;
}

BAPConditionType BAPConObjectMoved::getType() { return static_cast<BAPConditionType>(28); }

BAPConObjectMoved::~BAPConObjectMoved() {}   // PE 101A7230

BAPConObjectOccupied::BAPConObjectOccupied(unsigned int object) : m_object(object) {}

bool BAPConObjectOccupied::evaluate(Bot*)
{
    Information* const object = informationManager->getInformation(m_object);
    if (!object || !object->getUnit())
        return false;
    return object->getUnit()->isOccupied();
}

BAPConditionType BAPConObjectOccupied::getType() { return static_cast<BAPConditionType>(29); }

BAPConObjectOccupied::~BAPConObjectOccupied() {}   // PE 101AC3E0

BAPConObjectEnemyOccupied::BAPConObjectEnemyOccupied(unsigned int object) : m_object(object) {}

bool BAPConObjectEnemyOccupied::evaluate(Bot* bot)
{
    if (!isOccupiedObject(m_object, bot->getSide()))
        return false;
    Information* const object = informationManager->getInformation(m_object);
    if (!object)
        return false;
    return object->getSide() != bot->getSide();
}

BAPConditionType BAPConObjectEnemyOccupied::getType() { return static_cast<BAPConditionType>(20); }

BAPConObjectEnemyOccupied::~BAPConObjectEnemyOccupied() {}   // PE 101AC490

BAPConObjectBehind::BAPConObjectBehind(unsigned int object, float threshold, bool estimate)
    : m_object(object), m_threshold(threshold), m_estimate(estimate)
{
}

// The equipment is not null-checked, as shipped.
bool BAPConObjectBehind::evaluate(Bot* bot)
{
    Information* const object = informationManager->getInformation(m_object);
    if (!object)
        return true;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Vec2 objectFlat;
    if (m_estimate && object->getMobile())
    {
        Vec3 estimate;
        estimate.x = estimate.y = estimate.z = 0.0f;
        object->getMobile()->getPositionEstimate(estimate);
        objectFlat = flatOf(estimate);
    }
    else
    {
        objectFlat = flatOf(object->getPosition());
    }
    const Vec2 equipmentFlat = flatOf(equipment->getPosition());
    const float* const m = object->getTransformation().m;
    Vec3 forward;
    forward.x = m[8];
    forward.y = m[9];
    forward.z = m[10];
    Vec2 facing = flatOf(forward);
    normaliseFlat(facing);
    Vec2 toEquipment;
    toEquipment.x = equipmentFlat.x - objectFlat.x;
    toEquipment.y = equipmentFlat.y - objectFlat.y;
    normaliseFlat(toEquipment);
    return toEquipment.y * facing.y + toEquipment.x * facing.x <= m_threshold;
}

BAPConditionType BAPConObjectBehind::getType() { return static_cast<BAPConditionType>(17); }

BAPConObjectBehind::~BAPConObjectBehind() {}   // PE 101AC670

BAPConObjectDistance::BAPConObjectDistance(unsigned int object, float distance, bool estimate, bool flat,
                                           bool outside)
    : m_object(object), m_minSq(0.0f), m_maxSq(distance * distance), m_estimate(estimate), m_flat(flat),
      m_outside(outside), m_lastSq(0.0f)
{
}

BAPConObjectDistance::BAPConObjectDistance(unsigned int object, float minimum, float maximum,
                                           bool estimate, bool flat)
    : m_object(object), m_minSq(minimum * minimum), m_maxSq(maximum * maximum), m_estimate(estimate),
      m_flat(flat), m_outside(false), m_lastSq(0.0f)
{
}

// The equipment is not null-checked, as shipped.
bool BAPConObjectDistance::evaluate(Bot* bot)
{
    Information* const object = informationManager->getInformation(m_object);
    if (!object)
        return true;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    float distanceSq;
    if (m_estimate)
    {
        Vec3 position;
        position.x = position.y = position.z = 0.0f;
        if (object->getMobile())
            object->getMobile()->getPositionEstimate(position);
        else
            position = object->getPosition();
        const Vec3& own = equipment->getPosition();
        if (m_flat)
        {
            const float dx = position.x - own.x;
            const float dz = position.z - own.z;
            distanceSq = dz * dz + dx * dx;
        }
        else
        {
            const float dx = own.x - position.x;
            const float dy = own.y - position.y;
            const float dz = own.z - position.z;
            distanceSq = dx * dx + dy * dy + dz * dz;
        }
    }
    else if (m_flat)
    {
        const float ownX = equipment->getPosition().x;
        const float ownZ = equipment->getPosition().z;
        const float objectZ = object->getPosition().z;
        const float objectX = object->getPosition().x;
        const float dx = objectX - ownX;
        const float dz = objectZ - ownZ;
        distanceSq = dz * dz + dx * dx;
    }
    else
    {
        const Vec3& objectPosition = object->getPosition();
        const Vec3& own = equipment->getPosition();
        const float dx = own.x - objectPosition.x;
        const float dy = own.y - objectPosition.y;
        const float dz = own.z - objectPosition.z;
        distanceSq = dx * dx + dy * dy + dz * dz;
    }
    m_lastSq = distanceSq;
    if (m_outside)
        return distanceSq > m_maxSq;
    if (distanceSq < m_minSq)
        return false;
    return distanceSq <= m_maxSq;
}

BAPConditionType BAPConObjectDistance::getType() { return static_cast<BAPConditionType>(18); }

void BAPConObjectDistance::printDebug(std::ostream& out)
{
    if (m_outside)
    {
        out << "ObjectDistance[";
        if (m_lastSq != 0.0f)
            out << std::sqrt(m_lastSq);
        else
            out << "not calculated";
        out << "] > " << std::sqrt(m_maxSq);
        return;
    }
    out << "ObjectDistance(" << std::sqrt(m_minSq) << " <= [";
    if (m_lastSq != 0.0f)
        out << std::sqrt(m_lastSq);
    else
        out << "not calculated";
    out << "] <= " << std::sqrt(m_maxSq) << ")";
}

BAPConObjectDistance::~BAPConObjectDistance() {}   // PE 101AC9A0

// ---- BAPConPositionDistance -----------------------------------------------------------------

BAPConPositionDistance::BAPConPositionDistance(const Vec3& position, float distance, bool flat)
{
    m_maxSq = distance;
    m_flat = flat;
    m_minSq = 0.0f;
    m_lastSq = 0.0f;
    m_position = position;
    m_maxSq = distance * m_maxSq;
}

// The bot's current equipment information is not null-checked, as shipped.
bool BAPConPositionDistance::evaluate(Bot* bot)
{
    Information* const own = informationManager->getInformation(bot->getCurrentEquipment());
    float distanceSq;
    if (m_flat)
    {
        const Vec3& a = own->getPosition();
        const Vec3& b = own->getPosition();
        const float dx = m_position.x - b.x;
        const float dz = m_position.z - a.z;
        distanceSq = dz * dz + dx * dx;
    }
    else
    {
        const Vec3& at = own->getPosition();
        const float dx = m_position.x - at.x;
        const float dy = m_position.y - at.y;
        const float dz = m_position.z - at.z;
        distanceSq = dz * dz + dy * dy + dx * dx;
    }
    m_lastSq = distanceSq;
    if (distanceSq < m_minSq)
        return false;
    return distanceSq <= m_maxSq;
}

BAPConditionType BAPConPositionDistance::getType() { return static_cast<BAPConditionType>(12); }
BAPConPositionDistance::~BAPConPositionDistance() {}

// ---- BAPConObjectHealthBelow ----------------------------------------------------------------

BAPConObjectHealthBelow::BAPConObjectHealthBelow(unsigned int object, float threshold)
    : m_object(object), m_threshold(threshold)
{
}

bool BAPConObjectHealthBelow::evaluate(Bot*)
{
    Information* const info = informationManager->getInformation(m_object);
    if (!info)
        return false;
    return info->getPhysical()->getHealth() <= m_threshold;
}

BAPConditionType BAPConObjectHealthBelow::getType() { return static_cast<BAPConditionType>(22); }

void BAPConObjectHealthBelow::printDebug(std::ostream& out)
{
    out << "ObjectHealth <= " << m_threshold;
}

BAPConObjectHealthBelow::~BAPConObjectHealthBelow() {}

// ---- BAPConFireAllowed ----------------------------------------------------------------------

BAPConFireAllowed::BAPConFireAllowed() {}
bool BAPConFireAllowed::evaluate(Bot* bot) { return bot->isFireAllowed(); }
BAPConditionType BAPConFireAllowed::getType() { return static_cast<BAPConditionType>(50); }
BAPConFireAllowed::~BAPConFireAllowed() {}

// ---- BAPConSprinting ------------------------------------------------------------------------

BAPConSprinting::BAPConSprinting() {}

bool BAPConSprinting::evaluate(Bot* bot)
{
    const io::PlayerInput* const input = bot->playerInput();
    return (input->mappingFlags & (1ull << io::PISprint)) && input->inputs[io::PISprint] != 0.0f;
}

BAPConditionType BAPConSprinting::getType() { return static_cast<BAPConditionType>(51); }
BAPConSprinting::~BAPConSprinting() {}

// ---- BAPConWeaponZoomed ---------------------------------------------------------------------

BAPConWeaponZoomed::BAPConWeaponZoomed() {}

bool BAPConWeaponZoomed::evaluate(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = equipment->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    return weapon->isZoomed();
}

BAPConditionType BAPConWeaponZoomed::getType() { return static_cast<BAPConditionType>(52); }
BAPConWeaponZoomed::~BAPConWeaponZoomed() {}

// ---- BAPConWeaponHeat -----------------------------------------------------------------------

BAPConWeaponHeat::BAPConWeaponHeat(BotScriptValue<WeaponInfo*>* weapon, float high, float low)
    : m_weapon(weapon), m_high(high), m_low(low), m_overheated(false)
{
}

bool BAPConWeaponHeat::evaluate(Bot*)
{
    const float heat = m_weapon->m_value->getHeat();
    if (m_overheated)
    {
        if (!(heat <= m_low))
            return false;
        m_overheated = false;
        return true;
    }
    if (heat < m_high)
        return true;
    m_overheated = true;
    return false;
}

BAPConditionType BAPConWeaponHeat::getType() { return static_cast<BAPConditionType>(46); }
BAPConWeaponHeat::~BAPConWeaponHeat() {}

// ---- BAPConWeaponReady / BAPConWeaponStateOne ---------------------------------------------

namespace {

// The active weapon's info for the bot's current equipment; the equipment is not null-checked.
WeaponInfo* activeWeaponInfo(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = equipment->getArmament();
    return WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
}

} // anonymous namespace

BAPConWeaponReady::BAPConWeaponReady(BotScriptValue<WeaponInfo*>* weapon) : m_weapon(weapon) {}

bool BAPConWeaponReady::evaluate(Bot* bot)
{
    WeaponInfo* const active = activeWeaponInfo(bot);
    if (m_weapon->m_value != active)
        return false;
    return active->isReadyToFire();
}

BAPConditionType BAPConWeaponReady::getType() { return static_cast<BAPConditionType>(44); }
BAPConWeaponReady::~BAPConWeaponReady() {}

BAPConWeaponStateOne::BAPConWeaponStateOne(BotScriptValue<WeaponInfo*>* weapon) : m_weapon(weapon) {}

bool BAPConWeaponStateOne::evaluate(Bot* bot)
{
    WeaponInfo* const active = activeWeaponInfo(bot);
    if (m_weapon->m_value != active)
        return false;
    return active->getWeaponState() == 1;
}

BAPConditionType BAPConWeaponStateOne::getType() { return static_cast<BAPConditionType>(53); }
BAPConWeaponStateOne::~BAPConWeaponStateOne() {}

// ---- BAPConStrategicStatus ------------------------------------------------------------------

BAPConStrategicStatus::BAPConStrategicStatus(AIStrategicObject* object, int side, bool invert)
    : m_object(object), m_side(side), m_invert(invert)
{
}

bool BAPConStrategicStatus::evaluate(Bot*)
{
    const int status = m_object->getStatus(m_side);
    if (m_invert)
        return status == 0;
    return status != 0;
}

BAPConditionType BAPConStrategicStatus::getType() { return static_cast<BAPConditionType>(16); }
BAPConStrategicStatus::~BAPConStrategicStatus() {}

// ---- BAPConCanFireAt ------------------------------------------------------------------------

namespace {

// Line-of-fire cone: the radius grows from kConeNear to kConeFar over 1/kConeGrowth metres along
// the line; a friend nearer than kCloseFriend metres also counts when the bot faces it. Module
// data in the PE (102AD810..818, 10283EB0), never written. [INFERRED] names.
float kCloseFriend = 5.0f;
float kConeNear = 0.5f;
float kConeFar = 1.5f;
const float kConeGrowth = 0.02f;

// PE 100E9DE0, the Vec2 normaliser (the same file-local copy as SensingAgent.cpp).
int normalise(Vec2& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return 0;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = 0.0f;
        value.y = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    return 0;
}

} // anonymous namespace

BAPConCanFireAt::BAPConCanFireAt(unsigned int target, bool flag)
    : m_target(target), m_flagC(true), m_flagD(flag)
{
}

bool BAPConCanFireAt::evaluate(Bot* bot)
{
    Information* const own = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_target);
    if (!own || !target)
        return false;
    SensedObject* const sensed = static_cast<SensedObject*>(bot->getSensedObjectFromHandle(m_target));
    bool canFire = sensed ? !((sensed->m_senseFlags >> 3) & 1) : false;
    IPIArmament* const armament = own->getArmament();
    if (armament && armament->getWeapon(armament->getActiveWeapon()) != -1)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
            armament->getWeapon(armament->getActiveWeapon()));
        if (weapon && sensed && !weapon->getFiresThroughTransparent())
            canFire = canFire && (sensed->m_senseFlags & 0x48u) != 0;
    }
    if (canFire && ((own->getObjectTypeFlags() >> 21) & 1))
        canFire = !friendInLineOfFire(bot, own, target);
    return canFire;
}

bool BAPConCanFireAt::friendInLineOfFire(Bot* bot, Information* own, Information* target)
{
    IPIArmament* const armament = own->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3& muzzle = *reinterpret_cast<const Vec3*>(&origin.m[12]);
    const Vec3& targetPosition = target->getPosition();
    Vec3 direction;
    direction.x = targetPosition.x - muzzle.x;
    direction.y = targetPosition.y - muzzle.y;
    direction.z = targetPosition.z - muzzle.z;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    if (length > 1e-4f)
    {
        const float inverse = 1.0f / length;
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
    }
    const std::vector<IAIPlayer*>& players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
    bool found = false;
    for (unsigned int i = 0; !players.empty() && i < players.size(); ++i)
    {
        IAIPlayer* const player = players[i];
        if (!player || player->getSide() != bot->getSide())
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const info =
            informationManager->getInformation(vehicle->getInformation(bot->getSide()));
        if (info == own || info == target || !info)
            continue;
        if (!((info->getObjectTypeFlags() >> 21) & 1))
            continue;
        const Vec3& position = info->getPosition();
        Vec3 relative;
        relative.x = position.x - muzzle.x;
        relative.y = position.y - muzzle.y;
        relative.z = position.z - muzzle.z;
        const float along = relative.z * direction.z + relative.y * direction.y +
                            relative.x * direction.x;
        if (along > length)
            continue;
        Vec3 offset;
        offset.x = direction.x * along - relative.x;
        offset.y = direction.y * along - relative.y;
        offset.z = direction.z * along - relative.z;
        const float offsetSq = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
        float radius = (kConeFar - kConeNear) * (along * kConeGrowth) + kConeNear;
        if (weapon->getWeaponZoom() != 0x40 && bot->getSpawnKit() == 1 &&
            weapon->getMaxRange() > 100.0f)
            radius = kConeNear;
        if (!(offsetSq < radius * radius))
            continue;
        found = true;
        if (!(along < kCloseFriend))
            continue;
        Vec2 line;
        line.x = direction.x;
        line.y = direction.z;
        normalise(line);
        const Vec3 facing = own->getFaceing();
        Vec2 facing2d;
        facing2d.x = facing.x;
        facing2d.y = facing.z;
        if (facing2d.y * line.y + facing2d.x * line.x > 0.9f)
        {
            static_cast<BotMain*>(bot)->setFriendsInLineOfFireTime(
                static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
            return true;
        }
    }
    static_cast<BotMain*>(bot)->setFriendsInLineOfFireTime(0.0);
    return found;
}

BAPConditionType BAPConCanFireAt::getType() { return static_cast<BAPConditionType>(26); }
BAPConCanFireAt::~BAPConCanFireAt() {}

// ---- BAPConMagazine -------------------------------------------------------------------------

BAPConMagazine::BAPConMagazine(BotScriptValue<WeaponInfo*>* weapon, BotScriptValue<int>* shots,
                               bool changed)
    : m_weapon(weapon), m_changed(changed), m_shots(shots)
{
}

bool BAPConMagazine::evaluate(Bot*)
{
    if (m_changed)
    {
        if (m_weapon->m_value->getNumberOfMags() == 0)
            return m_shots->m_value != 0;
        if (m_weapon->m_value->getNumberOfMags() < 0)
            return false;
        return m_weapon->m_value->getShotsInMag(0) != m_shots->m_value;
    }
    if (m_weapon->m_value->getNumberOfMags() < 0)
        return false;
    if (m_weapon->m_value->getNumberOfMags() == 0)
        return true;
    if (m_weapon->m_value->getShotsInMag(0) < 0)
        return false;
    return m_weapon->m_value->getShotsInMag(0) <= m_shots->m_value;
}

BAPConditionType BAPConMagazine::getType() { return static_cast<BAPConditionType>(41); }
BAPConMagazine::~BAPConMagazine() {}

// ---- BAPConShared ---------------------------------------------------------------------------

BAPConShared::BAPConShared(BAPCondition* inner)
{
    Value* const value = new Value;
    value->m_value = false;
    value->m_holders = 1;
    m_value = value;
    m_inner = inner;
    inner->m_owner = this;
}

BAPConShared::BAPConShared(Value* value)
    : m_value(value), m_inner(0)
{
    ++value->m_holders;
}

bool BAPConShared::evaluate(Bot* bot)
{
    if (!m_inner)
        return m_value->m_value;
    m_value->m_value = m_inner->evaluate(bot);
    return m_value->m_value;
}

BAPConditionType BAPConShared::getType() { return static_cast<BAPConditionType>(38); }

BAPConShared::~BAPConShared()
{
    delete m_inner;
    if (--m_value->m_holders == 0)
        delete m_value;
}

BAPConShared* BAPConShared::share()
{
    return checkPointer(new BAPConShared(m_value));
}

// ---- BAPConAimOnTarget ----------------------------------------------------------------------

namespace {

// PE 1009B6F0: the point transformed by the matrix, rows as basis vectors plus the translation.
void transformPoint(const Mat4& matrix, const Vec3& in, Vec3& out)
{
    const float* const m = matrix.m;
    const float z = in.x * m[2] + in.z * m[10] + in.y * m[6] + m[14];
    const float y = in.x * m[1] + in.z * m[9] + in.y * m[5] + m[13];
    const float x = in.z * m[8] + in.y * m[4] + in.x * m[0] + m[12];
    out.x = x;
    out.y = y;
    out.z = z;
}

// Seconds after the last failed sky attack before a missed aim counts as a failed static attack.
// PE 102843E0.
const double kSkyAttackGrace = 2.5;
// Seconds ahead of now at which the aim is solved. PE 10281270.
const double kAimLead = 0.1;

} // anonymous namespace

BAPConAimOnTarget::BAPConAimOnTarget(unsigned int target, unsigned int equipment,
                                     BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction,
                                     bool trackFailures)
    : m_target(target), m_equipment(equipment), m_valid(valid), m_direction(direction),
      m_trackFailures(trackFailures)
{
}

bool BAPConAimOnTarget::aimDirection(Bot* bot, Vec3& direction)
{
    if (m_valid && m_direction) {
        direction = m_direction->m_value;
        return m_valid->m_value;
    }

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getControlInfo())
        return false;
    IPIArmament* const armament = equipment->getArmament();
    if (!armament)
        return false;
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon)
        return false;
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    // Neither primary-object lookup is checked, as shipped.
    IPIMobile* const ownMobile =
        informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()))->getMobile();

    m_solver.setDrag(weapon->getDrag());
    m_solver.setSpeed(weapon->getExitVelocity());
    const float gravity = static_cast<const IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();
    m_solver.setGravity(weapon->getGravityModifier() * gravity);

    // Aim at the sensed point on the target when this bot has one, otherwise at its position.
    const SensedObject* const sensed =
        static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(m_target));
    Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
    if (sensed && !(sensed->m_senseFlags & 8))
        transformPoint(target->getTransformation(), sensed->m_senseNormal, aimPoint);
    else
        aimPoint = target->getPosition();

    IPIMobile* const targetMobile =
        informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->getMobile();
    const float* const origin = weapon->getAbsoluteTransformation().m + 12;
    const Vec3 relative = {aimPoint.x - origin[0], aimPoint.y - origin[1], aimPoint.z - origin[2]};

    Vec3 targetVelocity = {0.0f, 0.0f, 0.0f};
    Vec3 ownVelocity = {0.0f, 0.0f, 0.0f};
    if (ownMobile)
        ownVelocity = ownMobile->getSpeed();
    if (targetMobile)
        targetVelocity = targetMobile->getSpeed();

    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();
    m_solver.setTarget(relative, targetVelocity, ownVelocity, &weapon->getAbsoluteTransformation(), now);

    // The solved direction goes to a local and the caller's `direction` is left as it was: the
    // shipped body reuses the argument's stack slot before this point. Preserved retail defect.
    Vec3 solved = {0.0f, 0.0f, 0.0f};
    Vec3 work = {0.0f, 0.0f, 0.0f};
    const bool lob = weapon->getIndirect();
    return m_solver.solve(timer->getGameTime() + kAimLead, 0.5f, lob, solved, work, 0);
}

bool BAPConAimOnTarget::evaluate(Bot* bot)
{
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    const bool solved = aimDirection(bot, direction);
    bool onTarget = false;
    if (solved) {
        const unsigned int handle = m_equipment == static_cast<unsigned int>(-1)
            ? bot->getCurrentEquipment() : m_equipment;
        // The equipment lookup is not checked, as shipped.
        onTarget = informationManager->getInformation(handle)->getControlInfo()
                       ->validateCameraDirection(direction);
    }

    if (m_trackFailures) {
        const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        BotMain* const main = static_cast<BotMain*>(bot);
        if (main->getBotWantsToSimulateArtillery()) {
            if (!onTarget)
                main->setTimeOfLastFailedStaticSkyAttack(now);
        } else if (!onTarget
                   && (!solved || now - main->getTimeOfLastFailedStaticSkyAttack() > kSkyAttackGrace)) {
            main->setTimeOfLastFailedStaticAttack(now);
            main->setBotWantsToSimulateArtillery(false);
            main->setBotIsSimulatingArtillery(false);
        }
    }
    return onTarget;
}

BAPConditionType BAPConAimOnTarget::getType() { return static_cast<BAPConditionType>(32); }
BAPConAimOnTarget::~BAPConAimOnTarget() {}

// ---- BAPConFireSolution ---------------------------------------------------------------------

namespace {

const float kMinRadiusSq = 0.01f;           // PE 1027B604
const float kMinRadius = 0.1f;              // PE 1027B220
const float kAngleScale = 0.02f;            // PE 10283EB0
const float kCloseHeight = 5.0f;            // PE 1027B20C
const float kCloseDistance = 15.0f;         // PE 1027B22C
const float kVerticalShare = 0.2f;          // PE 1027E4DC
const float kVerticalTight = 0.1f;          // PE 102AD4F0

// PE 1006EDE0: acos of the value clamped to [-1, 1].
float clampedAcos(float value)
{
    if (value < -1.0f)
        value = -1.0f;
    else if (value > 1.0f)
        value = 1.0f;
    return std::acos(value);
}

// PE 100DE7B0.
float signOf(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (value < 0.0f)
        return -1.0f;
    return 0.0f;
}

} // anonymous namespace

void BAPConFireSolution::init(float radius, bool closestApproach, bool tightAbove, bool skipAngle)
{
    float radiusSq = radius * radius;
    if (radiusSq < kMinRadiusSq)
        radiusSq = kMinRadiusSq;
    m_radiusSq = radiusSq;
    m_radius = radius < kMinRadius ? kMinRadius : radius;
    m_closestApproach = closestApproach;
    m_tightAbove = tightAbove;
    m_skipAngle = skipAngle;
    m_closest = FLT_MAX;
    if (!skipAngle)
        m_maxAngle = std::atan2(m_radius * kAngleScale, 1.0f);
}

BAPConFireSolution::BAPConFireSolution(unsigned int target, BAPAimAt* aim, float radius,
                                       bool closestApproach, bool tightAbove, bool skipAngle)
    : m_target(target), m_aim(aim), m_valid(0), m_direction(0)
{
    init(radius, closestApproach, tightAbove, skipAngle);
}

BAPConFireSolution::BAPConFireSolution(unsigned int target, BotScriptValue<bool>* valid,
                                       BotScriptValue<Vec3>* direction,
                                       BotScriptValue<Vec3>* position, float radius,
                                       bool closestApproach, bool tightAbove, bool skipAngle)
    : m_target(target), m_aim(0), m_valid(valid), m_direction(direction), m_position(position)
{
    init(radius, closestApproach, tightAbove, skipAngle);
}

bool BAPConFireSolution::evaluate(Bot* bot)
{
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    BotMain* const main = static_cast<BotMain*>(bot);
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);

    if (m_valid) {
        if (!m_valid->m_value)
            return false;
        // Neither the equipment nor its armament is checked here, as shipped.
        equipment = informationManager->getInformation(bot->getCurrentEquipment());
        IPIArmament* const armament = equipment->getArmament();
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
            armament->getWeapon(armament->getActiveWeapon()));
        if (!weapon)
            return false;

        const Vec3& position = m_position->m_value;
        const Vec3& own = equipment->getPosition();
        const float dx = position.x - own.x;
        const float dy = position.y - own.y;
        const float dz = position.z - own.z;
        const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
        bool lenient = false;
        if (std::fabs(dy) < kCloseHeight && !((target->getObjectTypeFlags() >> 21) & 1)
            && !((equipment->getObjectTypeFlags() >> 21) & 1) && distance < kCloseDistance)
            lenient = true;
        const bool staticAttack =
            !((equipment->getObjectTypeFlags() >> 21) & 1) && main->getUseStaticAttack();
        lenient = lenient || staticAttack;

        const Vec3& direction = m_direction->m_value;
        const float* const forward = weapon->getProjectileOrigin().m + 8;
        if (!m_skipAngle && !lenient) {
            const float cosine =
                forward[2] * direction.z + forward[1] * direction.y + forward[0] * direction.x;
            if (clampedAcos(cosine) >= m_maxAngle)
                return false;
        }

        // How far the muzzle line passes from the position at its distance.
        const float ex = (forward[0] - direction.x) * distance;
        const float ey = (forward[1] - direction.y) * distance;
        const float ez = (forward[2] - direction.z) * distance;
        const float horizontalSq = ex * ex + ez * ez;
        const float verticalSq = signOf(ey) * ey * ey;
        const float share = m_radiusSq * kVerticalShare;
        const float lower = -(kVerticalTight * share);
        const float upper = m_tightAbove ? kVerticalTight * share : share;
        if (horizontalSq > m_radiusSq)
            return false;
        if (!lenient) {
            if (verticalSq > upper)
                return false;
            if (verticalSq < lower)
                return false;
        }
        return true;
    }

    if (!m_aim->isSolved())
        return false;
    double impactTime;
    m_aim->getImpactTime(impactTime);
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    m_aim->getAimDirection(direction);
    // The equipment from the top is not checked, as shipped.
    IPIArmament* const armament = equipment->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    if (weapon && weapon->useAimerOnly()) {
        const float* const forward = m_aim->getFrame().m + 8;
        if (direction.z * forward[2] + direction.y * forward[1] + direction.x * forward[0] >= 1.0f) {
            main->setTime276(timer->getGameTime());
            return true;
        }
    }

    Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
    m_aim->getAimPoint(aimPoint);
    Vec3 projectile = {0.0f, 0.0f, 0.0f};
    if (!m_aim->getProjectilePosition(projectile, impactTime - timer->getGameTime()))
        return false;
    const float mx = aimPoint.x - projectile.x;
    const float my = aimPoint.y - projectile.y;
    const float mz = aimPoint.z - projectile.z;
    const float missSq = mz * mz + my * my + mx * mx;

    if (!m_closestApproach) {
        if (missSq >= m_radiusSq)
            return false;
        main->setTime276(timer->getGameTime());
        return true;
    }

    // Wait until the miss stops shrinking, then judge the closest one.
    if (m_closest > kMinRadiusSq && missSq < m_closest) {
        m_closest = missSq;
        if (missSq > m_radiusSq)
            return false;
        main->setTime276(timer->getGameTime());
        return false;
    }
    const bool hit = m_closest <= m_radiusSq;
    m_closest = FLT_MAX;
    if (hit)
        main->setTime276(timer->getGameTime());
    return hit;
}

BAPConditionType BAPConFireSolution::getType() { return static_cast<BAPConditionType>(1); }
BAPConFireSolution::~BAPConFireSolution() {}

// ---- BAPConTimer ----------------------------------------------------------------------------

BAPConTimer::BAPConTimer(double duration, bool flag, bool running, bool expired)
{
    m_deadline = expired
        ? -1.0
        : static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + duration;
    m_duration = duration;
    m_started = running || expired;
    m_flag = flag;
}

bool BAPConTimer::evaluate(Bot*)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!m_started) {
        m_started = true;
        m_deadline = now + m_duration;
    }
    if (now < m_deadline)
        return false;
    m_started = false;
    return true;
}

BAPConditionType BAPConTimer::getType() { return static_cast<BAPConditionType>(39); }
BAPConTimer::~BAPConTimer() {}

} // namespace ai
} // namespace hfe
} // namespace dice
