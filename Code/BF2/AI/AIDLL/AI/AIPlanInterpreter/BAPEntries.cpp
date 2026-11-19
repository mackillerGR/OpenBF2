// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BAPEntries operations.

#include "BAPEntries.h"
#include "../AIBot/BotSubObjects.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include "../Main/AIDllFrontier.h"
#include "PlanInterpreter.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBotActionPlan/BAPConditions.h"
#include "../AIBotActionPlan/LookController.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include <cmath>
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPStatements.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadMember.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include <algorithm>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// PE 1006EEC0, the conditional Vec3 normaliser (file-local copies live in other TUs too).
int normaliseVector(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return 0;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = 0.0f;
        value.y = 0.0f;
        value.z = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
    return 0;
}

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

// 1, -1 or 0 by the sign. PE 100DE7B0.
float signOf(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (!(value >= 0.0f))
        return -1.0f;
    return 0.0f;
}

// An input's value when the control map binds it, else 0. PE 10101350.
float readInput(const io::PlayerInput* input, int index)
{
    if (index < 64 && ((input->mappingFlags >> index) & 1ULL))
        return input->inputs[index];
    return 0.0f;
}

// Whether any component of a position is beyond FLT_EPSILON from zero.
bool isSet(const Vec3& value)
{
    return std::fabs(value.x) > 1.1920929e-07f || std::fabs(value.y) > 1.1920929e-07f
        || std::fabs(value.z) > 1.1920929e-07f;
}

float distanceBetween(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The primary object's handle for a handle, as the change/repair target comparisons use it.
unsigned int primaryHandle(Bot* bot, unsigned int handle)
{
    Information* const info = informationManager->getInformation(handle);
    if (info && !info->isPrimary())
        return info->getPrimaryObject(bot->getSide());
    return handle;
}

// PE 101C4370, a free helper ([INFERRED] name). Leaves weapon zoom: presses the active
// weapon's zoom input while zoomed and the input is up, otherwise releases it. The equipment is
// not null-checked; the active weapon is looked up twice, as shipped.
void toggleSoldierZoom(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = equipment->getArmament();
    if (!armament)
        return;
    armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* const weapon =
        WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    if (!weapon || weapon->getWeaponZoom() == 64)
        return;
    io::PlayerInput* const input = bot->playerInput();
    if (weapon->isZoomed())
    {
        if (readInput(input, weapon->getWeaponZoom()) != 0.0f)
            input->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 0.0f);
        else
            input->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 1.0f);
        return;
    }
    input->setInput(static_cast<io::PlayerInputMap>(weapon->getWeaponZoom()), 0.0f);
}

// PE 101C4460, a free helper ([INFERRED] name). A bot simulating artillery stops: its path
// progress is cleared and it asks for pose 2. Otherwise the throttle runs at the squad member's
// speed over the equipment's maximum, or at full sign while focusing, with sprint only while
// focusing. The control info is not read.
void setSoldierThrottle(Bot* bot, io::PlayerInput* input, IPIControlInfo*, float maxSpeed, bool focusing)
{
    if (static_cast<BotMain*>(bot)->getBotIsSimulatingArtillery())
    {
        bot->getBotNavigation()->clearProgress();
        bot->requestSoldierPose(2, true);
        return;
    }

    const float squadSpeed = bot->getSquadMember()->getMaxSpeed();
    if (focusing)
    {
        const float throttle = signOf(maxSpeed);
        if ((input->mappingFlags & (1ULL << io::PIThrottle)) != 0)
            input->inputs[io::PIThrottle] = throttle;
        if ((input->mappingFlags & (1ULL << io::PISprint)) != 0)
            input->inputs[io::PISprint] = 1.0f;
        return;
    }
    if ((input->mappingFlags & (1ULL << io::PIThrottle)) != 0)
        input->inputs[io::PIThrottle] = squadSpeed / maxSpeed;
    if ((input->mappingFlags & (1ULL << io::PISprint)) != 0)
        input->inputs[io::PISprint] = 0.0f;
}

} // anonymous namespace

bool hasAvoidedObstacleMoved(const Vec2& point, unsigned int object, float radius)
{
    Information* const info = informationManager->getInformation(object);
    if (!info)
        return false;
    const Vec3& position = info->getPosition();
    const float dx = point.x - position.x;
    const float dy = point.y - position.z;
    const float distanceSq = dy * dy + dx * dx;
    if (distanceSq < radius * radius)
        return true;
    const float outer = radius + 1.5f;
    return distanceSq > outer * outer;
}

// The equipment is not null-checked. Only obstacles overlapping the equipment's reach (their
// radius plus 1), within 2 vertically and ahead of it are considered. A target's own primary
// handle is looked up and dropped, as shipped.
bool findObstacleToAvoid(Bot* bot, const Vec2& towards, const Vec3&, unsigned int targetObject, Vec2& avoidPoint,
                         unsigned int& avoidObject, float& avoidOffset, Vec2& avoidCentre, float& avoidRadius,
                         bool& avoidingTarget, Information* ignore, bool allowIgnored)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();
    Vec2 best;
    best.x = best.y = 0.0f;
    float bestAlong = 99999.0f;
    const Vec3& position = equipment->getPosition();
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;

    const float tx = towards.x - flat.x;
    const float ty = towards.y - flat.y;
    const float length = std::sqrt(tx * tx + ty * ty);
    if (length < 9.9999997e-05f)
        return false;
    const float inverse = 1.0f / length;
    Vec2 direction;
    direction.x = tx * inverse;
    direction.y = ty * inverse;
    Vec2 across;
    across.x = direction.y;
    across.y = -direction.x;

    Vec3 obstaclePosition;
    obstaclePosition.x = obstaclePosition.y = obstaclePosition.z = 0.0f;
    IPIMobile* obstacleMobile = 0;
    primaryHandle(bot, targetObject);
    avoidOffset = 0.0f;
    const unsigned int change = primaryHandle(bot, bot->getChangeTarget());
    const unsigned int repair = primaryHandle(bot, bot->getRepairTarget());

    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        Information* const other = informationManager->getInformation(obstacles[i]);
        if (equipment == other)
            continue;
        if (other == ignore && !allowIgnored)
            continue;
        if (!other)
            continue;
        IPIPhysical* const physical = other->getPhysical();
        if (!physical)
            continue;
        const unsigned int flags = other->getObjectTypeFlags();
        if ((flags >> 21) & 1)
            continue;
        IPIMobile* const mobile = other->getMobile();
        if (!mobile && !((flags >> 25) & 1))
            continue;

        const bool isTarget = obstacles[i] == repair || obstacles[i] == change;
        const float health = physical->getHealth();
        bool byTarget;
        float radius;
        if (!((other->getObjectTypeFlags() >> 25) & 1) && !(IBotManager::instance->getVehicleBailValue() > health))
        {
            byTarget = false;
            radius = physical->getSmallestRadius();
        }
        else if (isTarget)
        {
            byTarget = true;
            radius = physical->getSmallestRadius();
        }
        else
        {
            byTarget = true;
            const float capped = std::min(10.0f, physical->getSmallestRadius() + 4.0f);
            const float smallest = physical->getSmallestRadius();
            radius = (smallest < capped ? capped : smallest) * 1.3f;
        }

        if (!(std::fabs(other->getPosition().y - position.y) <= 2.0f))
            continue;
        const Vec3& otherPosition = other->getPosition();
        Vec2 otherFlat;
        otherFlat.x = otherPosition.x;
        otherFlat.y = otherPosition.z;
        const float rx = otherFlat.x - flat.x;
        const float ry = otherFlat.y - flat.y;
        const float along = ry * direction.y + rx * direction.x;
        const float distanceSq = ry * ry + rx * rx;
        const float reach = radius + 1.0f;
        if (!(distanceSq <= reach * reach))
            continue;
        if (along / std::sqrt(distanceSq) < 0.1f)
            continue;
        if (!(std::fabs(ry * across.y + rx * across.x) < reach))
            continue;
        if (!(along < bestAlong))
            continue;

        best = otherFlat;
        bestAlong = along;
        avoidRadius = radius;
        obstaclePosition = other->getPosition();
        avoidObject = obstacles[i];
        avoidCentre = otherFlat;
        obstacleMobile = mobile;
        avoidingTarget = byTarget;
    }

    if (!(bestAlong < 99999.0f))
        return false;

    const int map = equipment->getMobile()->getVehicleNumber();
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    bot->getBotNavigation();
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    Vec3 hitNegative;
    hitNegative.x = hitNegative.y = hitNegative.z = 0.0f;
    Vec3 hitPositive;
    hitPositive.x = hitPositive.y = hitPositive.z = 0.0f;
    Vec3 side;
    side.x = across.x;
    side.y = 0.0f;
    side.z = across.y;
    unsigned int polygon;
    float positiveRoom;

    if (obstacleMobile)
    {
        const IPIMobile::ValidPos& valid = obstacleMobile->getLastValidPosition(map);
        const Vec3& from = valid.position;
        Vec3 to;
        to.x = side.x * 15.0f + from.x;
        to.y = side.y * 15.0f + from.y;
        to.z = side.z * 15.0f + from.z;
        pathfinding->trace(valid.polygon, valid.quadTree, map, from, to, hitPositive, polygon, equipmentType, -1,
                           0.0f, 0, 0);
        positiveRoom = isSet(hitPositive) ? distanceBetween(hitPositive, obstaclePosition) : 0.0f;
        to.x = from.x - side.x * 15.0f;
        to.y = from.y - side.y * 15.0f;
        to.z = from.z - side.z * 15.0f;
        pathfinding->trace(valid.polygon, valid.quadTree, map, from, to, hitNegative, polygon, equipmentType, -1,
                           0.0f, 0, 0);
    }
    else
    {
        if (!pathfinding->isValidPosition(map, equipmentType, obstaclePosition, true))
            return true;
        Vec3 to;
        to.x = side.x * 15.0f + obstaclePosition.x;
        to.y = side.y * 15.0f + obstaclePosition.y;
        to.z = side.z * 15.0f + obstaclePosition.z;
        pathfinding->trace(map, obstaclePosition, to, hitPositive, polygon, equipmentType, -1, 0.0f, 0, 0);
        positiveRoom = isSet(hitPositive) ? distanceBetween(hitPositive, obstaclePosition) : 0.0f;
        to.x = obstaclePosition.x - side.x * 15.0f;
        to.y = obstaclePosition.y - side.y * 15.0f;
        to.z = obstaclePosition.z - side.z * 15.0f;
        pathfinding->trace(map, obstaclePosition, to, hitNegative, polygon, equipmentType, -1, 0.0f, 0, 0);
    }
    const float negativeRoom = isSet(hitNegative) ? distanceBetween(hitNegative, obstaclePosition) : 0.0f;

    avoidPoint = best;
    const float sideOfLine = (best.y - flat.y) * across.y + (best.x - flat.x) * across.x;
    const float reach = avoidRadius + 1.0f;
    bool goNegative;
    if (sideOfLine > 0.0f)
        goNegative = negativeRoom > reach || negativeRoom > positiveRoom;
    else
        goNegative = !(reach < positiveRoom) && !(positiveRoom > negativeRoom);
    float offset = goNegative ? negativeRoom : positiveRoom;
    if (!(offset < avoidRadius))
        offset = avoidRadius;
    const float signedOffset = goNegative ? -offset : offset;
    avoidPoint.x = best.x + across.x * signedOffset;
    avoidPoint.y = best.y + across.y * signedOffset;
    avoidOffset = offset;
    return false;
}

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

// PE 10134A90. Steers a soldier along its move: the look direction carries the (avoiding)
// direction and the throttle the speed. A finished move (with firing allowed), a failed or
// arrived steer, or an avoidance that needs nothing ends the step with the movement released.
bool EntryInfanteryMoveTo::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    Vec2 avoid;
    avoid.x = avoid.y = 0.0f;
    Vec2 steer;
    steer.x = steer.y = 0.0f;

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment || !equipment->getMobile())
    {
        failed = true;
        finished = false;
        return true;
    }

    IAIObjectPhysical* const physical = equipment->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(equipment->getSide());
    equipment->getPosition();

    BAPAMove* const move = static_cast<BAPAMove*>(script);
    if (bot->isFireAllowed() && move->isFinished(bot))
    {
        failed = true;
        finished = true;
        return true;
    }

    failed = false;
    bool steerFailed;
    finished = move->getSteering(steer, failed, steerFailed, 0);
    if (finished)
    {
        if (steerFailed || failed)
        {
            finished = resetSoldierControls(bot, bot->playerInput(), true, false, false);
            return true;
        }
        if (!move->getAvoidanceSteering(bot, steer, avoid, 0, false))
        {
            bot->getSensingAgent()->setLookDirection(avoid);
            toggleSoldierZoom(bot);
            io::PlayerInput* const input = bot->playerInput();
            IPIControlInfo* const controlInfo = equipment->getControlInfo();
            const bool focusing = move->isFocusing();
            setSoldierThrottle(bot, input, controlInfo, equipment->getMobile()->getMaxSpeed(), focusing);
            if (static_cast<int>(equipment->getSoldierPose()) != 4)
                bot->requestSoldierPose(1, true);
            return true;
        }
    }
    finished = resetSoldierControls(bot, bot->playerInput(), true, false, false);
    failed = true;
    return true;
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
        bot->playerInput()->setInput(static_cast<io::PlayerInputMap>(reset->m_input), 0.0f);
        finished = true;
        failed = true;
        return true;
    }
    resetSoldierControls(bot, bot->playerInput(), reset->m_movement, reset->m_look, reset->m_actions);
    finished = true;
    failed = true;
    return true;
}


// ---- Trigger entries ------------------------------------------------------------------------

namespace {

// Whether the soldier is steering: throttle, yaw or sprint held where mapped.
bool isSteering(const io::PlayerInput* input)
{
    if ((input->mappingFlags & (1ULL << io::PIThrottle)) && input->inputs[io::PIThrottle] != 0.0f)
        return true;
    if ((input->mappingFlags & (1ULL << io::PIYaw)) && input->inputs[io::PIYaw] != 0.0f)
        return true;
    if ((input->mappingFlags & (1ULL << io::PISprint)) && input->inputs[io::PISprint] != 0.0f)
        return true;
    return false;
}

// The active weapon's record, or null without an armament or an active weapon. The equipment
// is not checked, as shipped.
WeaponInfo* triggerWeapon(Information* equipment, IPIArmament*& armament)
{
    armament = equipment->getArmament();
    if (!armament || armament->getActiveWeapon() == -1)
        return 0;
    return WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
}

bool isSoldier(const Information* information)
{
    return (information->getObjectTypeFlags() >> 21) & 1;
}

// Whether the firing target holds an exposed soldier and is not itself a bot.
bool targetHasExposedSoldier(Information* target)
{
    return target && target->getAIObject()
        && !IBotManager::instance->getBot(target->getAIObject())
        && target->getUnit() && target->getUnit()->hasExposedSoldier();
}

// The skill scale passed with the bot skill: 3 against a non-primary target type the equipment
// cannot match, 1 otherwise. The isPrimary query is on the equipment, as shipped.
float skillScale(Information* equipment, Information* target)
{
    if ((equipment->getObjectTypeFlags() >> 4) & 1)
        return 1.0f;
    if (!((target->getObjectTypeFlags() >> 4) & 1))
        return 1.0f;
    if (equipment->isPrimary() || ((equipment->getObjectTypeFlags() >> 7) & 1))
        return 1.0f;
    return 3.0f;
}

const float kSoldierSplashSkill = 0.4f;     // PE 1027FF68
const float kVehicleSplashSkill = 0.8f;     // PE 1027E208
const float kDirectFireSkill = 0.5f;        // PE 1027B230

const float kTurretPi = 3.14159274f;            // PE 10280740 (also the stopped yaw, 40490FDB)
const float kTurretHalfPi = 1.57079637f;        // PE 10280744
const float kTurretDegree = 0.0174532924f;      // PE 102AAF70
const float kTurretDeadZoneDegrees = 0.25f;     // PE 1027B208

} // anonymous namespace

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

// PE 10134040. Weapon-select inputs finish once the weapon is active; the zoom input waits while
// steering. Fire inputs first set the weapon's firing pose and the bot skill for the shot, and are
// held back during a pose change. Then, after the delay: the release condition or a pressed
// one-shot releases and finishes as failed; otherwise the press condition presses (failed for a
// one-shot) or the input is released.
bool EntryTrigger::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPTrigger* const trigger = static_cast<BAPTrigger*>(script);
    const int which = trigger->m_input;
    BotMain* const main = static_cast<BotMain*>(bot);
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* armament;
    WeaponInfo* const weapon = triggerWeapon(equipment, armament);
    io::PlayerInput* const input = bot->playerInput();
    const bool steering = isSteering(input);
    const bool fire = which == io::PIFire || which == io::PIAltFire;
    bool allowed = true;
    if (fire && main->botInPoseTransition())
        allowed = false;

    if (weapon) {
        if (which >= io::PIWeaponSelect1 && which <= io::PIWeaponSelect8) {
            if (weapon->getWeaponActivate() == which) {
                finished = true;
                failed = true;
                return true;
            }
            if (trigger->m_pressed) {
                finished = true;
                failed = false;
                return true;
            }
        } else if (weapon->getWeaponZoom() == which) {
            if (steering)
                return true;
        } else if (fire) {
            if (!steering && weapon->getWeaponType() != 4) {
                int pose = 1;
                if (isSoldier(equipment)) {
                    pose = weapon->getFiringPose();
                    if (armament->getActiveWeapon() == 0
                        || (bot->getFiringTarget() != static_cast<unsigned int>(-1)
                            && weapon->getWeaponType() == 3)) {
                        const Vec3& own = equipment->getPosition();
                        Information* const target =
                            informationManager->getInformation(bot->getFiringTarget());
                        if (target) {
                            const Vec3& at = target->getPosition();
                            const float dx = at.x - own.x;
                            const float dz = at.z - own.z;
                            if (dz * dz + dx * dx < 1.0f && target->getSoldierPose() == 3)
                                pose = 2;
                        }
                    }
                }
                const int current = equipment->getSoldierPose();
                if (current != 4 && current != pose)
                    bot->requestSoldierPose(pose, false);
            }

            Information* const target = informationManager->getInformation(bot->getFiringTarget());
            float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
            const bool exposed = targetHasExposedSoldier(target);
            if (weapon->getWeaponZoom() != 0x40 && bot->getSpawnKit() == 1)
                weapon->getMaxRange();   // the result is unused, as shipped
            if (weapon->getIsThrown()) {
                skill = 1.0f;
            } else if (weapon->getExplosionRadius() != 0.0f) {
                const float factor = isSoldier(equipment) ? kSoldierSplashSkill : kVehicleSplashSkill;
                skill = 1.0f - (1.0f - skill) * factor;
            } else if (!exposed) {
                skill = 1.0f - (1.0f - skill) * kDirectFireSkill;
            }
            if (weapon->getWeaponGuidanceType() != 0)
                skill = 1.0f;
            if (((equipment->getObjectTypeFlags() >> 4) & 1) || target) {
                const float scale = ((equipment->getObjectTypeFlags() >> 4) & 1)
                    ? 1.0f : skillScale(equipment, target);
                weapon->setBotSkill(skill, bot->getFiringTargetTime(), scale);
            }
        }
    }

    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(now >= trigger->m_startTime))
        return true;
    if ((trigger->m_release && trigger->m_release->evaluate(bot))
        || (trigger->m_pressed && trigger->m_flag30)) {
        input->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 0.0f);
        trigger->m_pressed = false;
        finished = true;
        failed = true;
        return true;
    }
    if (allowed && trigger->m_until->evaluate(bot)) {
        input->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 1.0f);
        trigger->m_pressed = true;
        finished = true;
        failed = !trigger->m_flag30;
        return true;
    }
    input->setInput(static_cast<io::PlayerInputMap>(trigger->m_input), 0.0f);
    trigger->m_pressed = false;
    finished = true;
    failed = false;
    return true;
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

// PE 10133B80. Fire inputs set the firing pose (not while steering, nor for kit 4's sixth weapon
// slot on foot) and the bot skill, and are held back during a pose change; without an active
// weapon the statement fails at once. After the delay: the stop condition finishes as failed;
// the press condition presses; a pressed input stays held while the hold condition is false;
// otherwise it is released.
bool EntryTriggerContinously::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPTriggerContinously* const trigger = static_cast<BAPTriggerContinously*>(script);
    BotMain* const main = static_cast<BotMain*>(bot);
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* armament;
    WeaponInfo* const weapon = triggerWeapon(equipment, armament);
    const int which = trigger->m_input;
    const bool fire = which == io::PIFire || which == io::PIAltFire;
    bool allowed = true;
    io::PlayerInput* const input = bot->playerInput();

    if (fire) {
        if (main->botInPoseTransition())
            allowed = false;
        if (!weapon) {
            if (armament) {
                finished = false;
                failed = true;
                return true;
            }
        } else {
            if (!isSteering(input)
                && !(bot->getSpawnKit() == 4 && isSoldier(equipment)
                     && armament->getActiveWeapon() == 5)) {
                const int pose = isSoldier(equipment) ? static_cast<int>(weapon->getFiringPose()) : 1;
                const int current = equipment->getSoldierPose();
                if (current != 4 && current != pose)
                    bot->requestSoldierPose(pose, false);
            }

            Information* const target = informationManager->getInformation(bot->getFiringTarget());
            float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
            const bool exposed = targetHasExposedSoldier(target);
            weapon->getExplosionRadius();   // read twice, as shipped
            if (weapon->getExplosionRadius() != 0.0f) {
                const float factor = isSoldier(equipment) ? kSoldierSplashSkill : kVehicleSplashSkill;
                skill = 1.0f - (1.0f - skill) * factor;
            } else if (!exposed) {
                skill = 1.0f - (1.0f - skill) * kDirectFireSkill;
            }
            if (weapon->getWeaponGuidanceType() != 0)
                skill = 1.0f;
            if (((equipment->getObjectTypeFlags() >> 4) & 1) || target) {
                const float scale = ((equipment->getObjectTypeFlags() >> 4) & 1)
                    ? 1.0f : skillScale(equipment, target);
                weapon->setBotSkill(skill, bot->getFiringTargetTime(), scale);
            }
        }
    }

    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(now >= trigger->m_startTime))
        return true;
    if (trigger->m_stop && trigger->m_stop->evaluate(bot)) {
        finished = true;
        failed = true;
        return true;
    }
    if (allowed) {
        if (trigger->m_press->evaluate(bot)) {
            input->setInput(static_cast<io::PlayerInputMap>(which), 1.0f);
            trigger->m_pressed = true;
            finished = true;
            failed = false;
            return true;
        }
        if (trigger->m_hold && trigger->m_pressed && !trigger->m_hold->evaluate(bot)) {
            input->setInput(static_cast<io::PlayerInputMap>(which), 1.0f);
            finished = true;
            failed = false;
            return true;
        }
    }
    input->setInput(static_cast<io::PlayerInputMap>(which), false);
    trigger->m_pressed = false;
    finished = true;
    failed = false;
    return true;
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

// ---- Turret aim entry -----------------------------------------------------------------------

namespace {

// PE 1014B650: the signed angle from `a` to `b` (radians, in (-pi, pi]), from the clamped cross
// product folded by the sign of the dot product.
float signedAngle(float ax, float ay, float bx, float by)
{
    float cross = ay * bx - ax * by;
    if (-1.0f > cross)
        cross = -1.0f;
    else if (!(1.0f >= cross))
        cross = 1.0f;
    if (cross < -1.0f)
        cross = -1.0f;
    else if (cross > 1.0f)
        cross = 1.0f;
    const float angle = kTurretHalfPi - std::acos(cross);
    if (!(by * ay + ax * bx < 0.0f))
        return angle;
    if (angle > 0.0f)
        return 1.0f * kTurretPi - angle;
    if (angle >= 0.0f)
        return 0.0f * kTurretPi - angle;
    return -1.0f * kTurretPi - angle;
}

} // anonymous namespace

// PE 10133190. Holds the sensing-agent override and steers the look along the aim statement's
// solution for the active weapon (or straight at the target when it has none), recording the
// pitch errors for the climb bias. The aim's condition stops it as failed; without equipment it
// finishes without failing.
bool EntryMouseTurretAimAt::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPAimAt* const aim = static_cast<BAPAimAt*>(script);
    Vec3 ownVelocity = {0.0f, 0.0f, 0.0f};
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    bot->getSensingAgent()->setOverride(true);
    if (aim->m_condition->evaluate(bot)) {
        finished = true;
        failed = true;
        aim->m_previousYaw = kTurretPi;
        return true;
    }
    failed = false;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment) {
        finished = true;
        failed = false;
        aim->m_previousYaw = kTurretPi;
        return true;
    }

    // Neither the armament nor the weapon record is checked, as shipped.
    IPIArmament* const armament = equipment->getArmament();
    IPIControlInfo* const control = equipment->getControlInfo();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    const Mat4& weaponFrame = weapon->getAbsoluteTransformation();
    IPIMobile* mobile = equipment->getMobile();
    if (!mobile && !equipment->isPrimary()) {
        Information* const primary =
            informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
        if (primary)
            mobile = primary->getMobile();
    }
    if (mobile)
        ownVelocity = mobile->getSpeed();

    const Mat4& origin = weapon->getProjectileOrigin();
    Vec3 previous = {0.0f, 0.0f, 0.0f};
    const bool hadDirection = aim->getAimDirection(previous);
    const bool lob = weapon->getIndirect();
    const float gravityModifier = weapon->getGravityModifier();
    const float speed = weapon->getExitVelocity();
    const float drag = weapon->getDrag();
    if (!aim->computeAim(origin, ownVelocity, direction, 0.5f, drag, speed, gravityModifier, lob,
                         bot)) {
        const Vec3 muzzle = {weaponFrame.m[12], weaponFrame.m[13], weaponFrame.m[14]};
        if (!aim->getRelativePosition(muzzle, direction)) {
            finished = true;
            failed = false;
            aim->m_previousYaw = kTurretPi;
            return true;
        }
        normaliseVector(direction);
    }

    Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
    aim->getAimPoint(aimPoint);
    static_cast<BotMain*>(bot)->getFireCorrection()->recordAim(direction, aimPoint);

    const float deadZone = kTurretDegree * kTurretDeadZoneDegrees;
    float yawSpeed = 0.0f;
    float pitchSpeed = 0.0f;
    if (hadDirection) {
        const double step =
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
        const float rate = static_cast<float>(1.0 / step);
        yawSpeed = signedAngle(previous.x, previous.z, direction.x, direction.z) * rate;
        pitchSpeed = signedAngle(previous.y, previous.z, direction.y, direction.z) * rate;
    }
    finished = true;

    const float previousYaw = aim->m_previousYaw;
    const float previousPitch = aim->m_pitchHistory[aim->m_pitchIndex];
    io::PlayerInput* const input = bot->playerInput();
    float climb = 0.0f;
    for (int i = 0; i < 10; ++i)
        climb += aim->m_pitchHistory[i];
    climb *= 0.1f;   // PE 101C8680
    float pitchError;
    float yawError;
    steerLook(bot, direction, origin, pitchError, previousPitch, yawError, previousYaw, deadZone,
              control, input, climb, yawSpeed, pitchSpeed);
    aim->m_pitchIndex = (aim->m_pitchIndex + 1) % 10;
    aim->m_pitchHistory[aim->m_pitchIndex] = pitchError;
    aim->m_previousYaw = yawError;
    return true;
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

// PE 10132ED0. Holds the sensing agent's override and steers the look toward the statement's
// target until its condition holds. The controller state resets to pi whenever the look ends or
// has nothing to look at.
bool EntryMouseTurretLookAt::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPALook* const look = static_cast<BAPALook*>(script);
    Vec3 direction = {};
    bot->getSensingAgent()->setOverride(true);
    if (look->m_until->evaluate(bot))
    {
        failed = true;
        finished = true;
        look->m_state20 = 3.14159274101257324f;
        return true;
    }
    failed = false;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
    {
        failed = true;
        finished = false;
        look->m_state20 = 3.14159274101257324f;
        return true;
    }
    direction = look->getTarget(finished);
    normaliseVector(direction);
    if (!finished)
    {
        failed = false;
        look->m_state20 = 3.14159274101257324f;
        return true;
    }
    finished = true;
    IPIControlInfo* const control = equipment->getControlInfo();
    float pitchError;
    float yawError;
    const float previousPitch = look->m_state24;
    const float previousYaw = look->m_state20;
    steerLook(bot, direction,
              *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation()),
              pitchError, previousPitch, yawError, previousYaw, look->m_deadZone, control,
              bot->playerInput(), 0.0f, 0.0f, 0.0f);
    look->m_state24 = pitchError;
    look->m_state20 = yawError;
    const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
    look->m_lastForward.x = camera.m[8];
    look->m_lastForward.y = camera.m[9];
    look->m_lastForward.z = camera.m[10];
    return true;
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
