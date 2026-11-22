// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BAPEntries operations.

#include "BAPEntries.h"
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"
#include "../../../../Math/Vec3.h"
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
#include "../AIBot/CommunicationAgent.h"
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

// EntryTankMoveTo::execute follows the tank reset helper below.


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

namespace {

// PE 1006ED90: clamps to [low, high]; an unordered value yields low.
float clampInput(float value, float low, float high)
{
    if (!(value >= low))
        return low;
    if (value > high)
        return high;
    return value;
}

// The brake limits of the car and tank resets: writable data in the image that nothing writes.
float s_carBrakeLimit = 0.7f;    // PE 102AE3BC
float s_tankBrakeLimit = 0.7f;   // PE 102AE1B4

// PE 101CD4F0 (car) and PE 101C74D0 (tank), the same body over their own limit ([INFERRED]
// name). Releases a vehicle's movement group: inputs 0-2 go to zero and the throttle (input 3)
// brakes against the forward speed, clamped to the limit in size, at least 0.4 unless the speed
// is within 0.05 of standing (then released). Look releases inputs 4 and 5; actions release 8,
// 31, 14-30, 38, 37, 10 and 32. Unbound inputs are skipped.
bool resetVehicleControls(io::PlayerInput* input, bool movement, bool look, bool actions, float forwardSpeed,
                          float brakeLimit)
{
    if (movement)
    {
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(2), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(1), 0.0f);
        const float brake = -clampInput(forwardSpeed, -brakeLimit, brakeLimit);
        const float size = std::fabs(brake);
        float throttle;
        if (!(size > 0.05f))
            throttle = 0.0f;
        else if (!(0.4f < size))
            throttle = signOf(brake) * 0.4f;
        else
            throttle = signOf(brake) * size;
        input->setInput(static_cast<io::PlayerInputMap>(3), throttle);
    }
    if (look)
    {
        input->setInput(static_cast<io::PlayerInputMap>(4), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(5), 0.0f);
    }
    if (!actions)
        return true;
    input->setInput(static_cast<io::PlayerInputMap>(8), 0.0f);
    input->setInput(static_cast<io::PlayerInputMap>(31), 0.0f);
    for (int each = 14; each <= 30; ++each)
        input->setInput(static_cast<io::PlayerInputMap>(each), 0.0f);
    input->setInput(static_cast<io::PlayerInputMap>(38), 0.0f);
    input->setInput(static_cast<io::PlayerInputMap>(37), 0.0f);
    input->setInput(static_cast<io::PlayerInputMap>(10), 0.0f);
    input->setInput(static_cast<io::PlayerInputMap>(32), 0.0f);
    return true;
}

} // anonymous namespace

// PE 10138720. As EntryCarResetControls, without the dropped reads and over the tank's limit.
bool EntryTankResetControls::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPResetControls* const reset = static_cast<BAPResetControls*>(script);
    if (reset->hasInputMask())
    {
        bot->playerInput()->setInput(static_cast<io::PlayerInputMap>(reset->m_input), 0.0f);
        finished = true;
        failed = true;
        return true;
    }
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    failed = true;
    const Vec3& speed = equipment->getMobile()->getSpeed();
    const Vec3 facing = equipment->getFaceing();
    const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;
    finished = resetVehicleControls(bot->playerInput(), reset->m_movement, reset->m_look, reset->m_actions, forward,
                                    s_tankBrakeLimit);
    return true;
}


namespace {

// The bounds of the tank's speed-sensitive steering: writable data in the image that nothing writes.
float s_tankSteerMinSpeed = 1.0f;    // PE 102AE1A8
float s_tankSteerFullSpeed = 2.0f;   // PE 102AE1A4

// Read in place of a tank's physics state test and never written, so the test always runs.
bool s_forceTankPhysics = false;     // PE 102C05D8 (bss)

// Shortens the free distances ahead of and behind `origin` along `direction` to the nearest
// obstacle the bot's navigation lists (other than `ignore`, the bot's own equipment and soldiers)
// whose smallest radius reaches the line: the distance along the line to where its circle meets
// it. PE 101E1970; [INFERRED] name.
void obstacleClearance(Bot* bot, const Vec3& origin, const Vec3& direction, float& front, float& back,
                       unsigned int ignore)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        if (obstacles[i] == ignore)
            continue;
        Information* const obstacle = informationManager->getInformation(obstacles[i]);
        if (equipment == obstacle || !obstacle)
            continue;
        IPIPhysical* const physical = obstacle->getPhysical();
        if (!physical || ((obstacle->getObjectTypeFlags() >> 21) & 1))
            continue;
        const float radius = physical->getSmallestRadius();
        const Vec3& position = obstacle->getPosition();
        Vec3 offset;
        offset.x = position.x - origin.x;
        offset.y = position.y - origin.y;
        offset.z = position.z - origin.z;
        const float along = offset.x * direction.x + offset.y * direction.y + offset.z * direction.z;
        if (along > 0.0f)
        {
            if (radius + front < along)
                continue;
            Vec3 onLine;
            onLine.x = along * direction.x;
            onLine.y = along * direction.y;
            onLine.z = along * direction.z;
            const float dx = offset.x - onLine.x;
            const float dy = offset.y - onLine.y;
            const float dz = offset.z - onLine.z;
            const float lateral = std::sqrt(dx * dx + dy * dy + dz * dz);   // PE 1006EE90
            if (lateral > radius)
                continue;
            const float hit = along - std::sqrt(radius * radius - lateral * lateral);
            if (hit > front)
                continue;
            front = hit;
        }
        else
        {
            const float behind = -along;
            if (behind > radius + back)
                continue;
            Vec3 onLine;
            onLine.x = along * direction.x;
            onLine.y = along * direction.y;
            onLine.z = along * direction.z;
            const float dx = offset.x - onLine.x;
            const float dy = offset.y - onLine.y;
            const float dz = offset.z - onLine.z;
            const float lateral = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (lateral > radius)
                continue;
            const float hit = behind - std::sqrt(radius * radius - lateral * lateral);
            if (hit > back)
                continue;
            back = hit;
        }
    }
}

// The obstacle ahead to slow down for (PE 101E2210; [INFERRED] name). Below speed 2 nothing is
// sought (answers 9999). Otherwise every unit the navigation lists within 15 beyond the two
// smallest radii and in the vehicle's lane (moving direction, facing signed by the speed) is
// weighed: a live friendly soldier ahead becomes the blocker to warn; a vehicle ahead (or close
// and moving) answers its speed along the lane less 1 (at least 0), or -20 when the two closing
// speeds differ by more than 10 and the bot's move status is 0, which also sets `stopped`. A
// friendly vehicle within 5 becomes the blocker. The lowest answer wins and its handle becomes
// the bot's slow-down target. The bot's change target (unless primary) is passed over, as is the
// collided object while obstacles are ignored and the speeds close slowly.
float findSlowDownTarget(Bot* bot, Information* equipment, const Vec3& velocity, float speed, const Vec3& position,
                         bool& stopped)
{
    stopped = false;
    if (!(speed > 2.0f))
        return 9999.0f;
    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();
    const Vec3 facing = equipment->getFaceing();
    const float heading = facing.z * velocity.z + facing.y * velocity.y + velocity.x * facing.x;
    float sign;
    if (heading > 0.0f)
        sign = 1.0f;
    else if (heading < 0.0f)
        sign = -1.0f;
    else
        sign = 0.0f;
    const Vec3 lane0 = equipment->getFaceing();
    Vec3 lane;
    lane.x = sign * lane0.x;
    lane.y = sign * lane0.y;
    lane.z = sign * lane0.z;
    IPIPhysical* const ownPhysical = equipment->getPhysical();
    IPIMobile* const ownMobile = equipment->getMobile();
    float best = 9999.0f;
    unsigned int bestHandle = static_cast<unsigned int>(-1);
    Information* blocker = 0;
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        Information* const other = informationManager->getInformation(obstacles[i]);
        if (!other || other == equipment)
            continue;
        IPIPhysical* const physical = other->getPhysical();
        if (!physical)
            continue;
        const bool soldier = (other->getObjectTypeFlags() >> 21) & 1;
        if (soldier && !(physical->getHealth() > 0.0f))
            continue;
        IPIMobile* const mobile = other->getMobile();
        if (!mobile)
            continue;
        Information* const changeTarget = informationManager->getInformation(bot->getChangeTarget());
        if (changeTarget && !changeTarget->isPrimary()
            && changeTarget->getPrimaryObject(bot->getSide()) == obstacles[i])
            continue;
        Vec3 offset;
        offset.x = offset.y = offset.z = 0.0f;
        const Vec3 local = physical->getSphereLocalOffset();
        const Mat4& transformation = other->getTransformation();   // PE 100EDB40: rotation only
        offset.x = local.z * transformation.m[8] + local.y * transformation.m[4] + local.x * transformation.m[0];
        offset.y = transformation.m[1] * local.x + transformation.m[9] * local.z + transformation.m[5] * local.y;
        offset.z = transformation.m[2] * local.x + transformation.m[10] * local.z + transformation.m[6] * local.y;
        const Vec3& otherPosition = other->getPosition();
        Vec3 centre;
        centre.x = offset.x + otherPosition.x;
        centre.y = offset.y + otherPosition.y;
        centre.z = offset.z + otherPosition.z;
        Vec3 toward;
        toward.x = centre.x - position.x;
        toward.y = centre.y - position.y;
        toward.z = centre.z - position.z;
        float gap = std::sqrt(toward.z * toward.z + toward.y * toward.y + toward.x * toward.x);
        const float ownRadius = ownPhysical->getSmallestRadius();
        const float radii = physical->getSmallestRadius() + ownRadius;
        if (gap > 9.999999747378752e-06f)
        {
            const float inverse = 1.0f / gap;
            toward.x = toward.x * inverse;
            toward.y = toward.y * inverse;
            toward.z = toward.z * inverse;
        }
        gap = gap - radii;
        if (gap > 15.0f)
            continue;
        const float otherSpeed = mobile->getSpeedMagnitude();
        const float along = toward.z * lane.z + toward.y * lane.y + toward.x * lane.x;
        const float reach = radii + gap;
        Vec3 ahead;
        ahead.x = lane.x * along * reach + position.x;
        ahead.y = lane.y * along * reach + position.y;
        ahead.z = lane.z * along * reach + position.z;
        const float sx = centre.x - ahead.x;
        const float sy = centre.y - ahead.y;
        const float sz = centre.z - ahead.z;
        if (radii * radii < sx * sx + sy * sy + sz * sz)
            continue;
        if (soldier)
        {
            if (!(along > 0.0f))
                continue;
            if (other->getSide() != equipment->getSide())
                continue;
            blocker = other;
            continue;
        }
        if (!(along >= 0.75f))
        {
            if (!(gap < 2.0f) || !(along > 0.0f) || !(otherSpeed > 2.0f))
                continue;
        }
        Vec3 otherVelocity = mobile->getSpeed();
        const bool moving = otherSpeed > 0.5f;
        float answer = std::max(0.0f, otherVelocity.z * lane.z + otherVelocity.y * lane.y
                                          + otherVelocity.x * lane.x - 1.0f);   // PE 10086630
        if (moving)
        {
            const float inverse = 1.0f / otherSpeed;   // PE 1009B6B0
            otherVelocity.x *= inverse;
            otherVelocity.y *= inverse;
            otherVelocity.z *= inverse;
        }
        const Vec3& ownVelocity = ownMobile->getSpeed();
        const Vec3& theirVelocity = mobile->getSpeed();
        const float cx = theirVelocity.x - ownVelocity.x;   // PE 1007A380
        const float cy = theirVelocity.y - ownVelocity.y;
        const float cz = theirVelocity.z - ownVelocity.z;
        bool closingFast;
        if (cx * cx + cy * cy + cz * cz > 100.0f)
        {
            closingFast = true;
            if (!static_cast<BotMain*>(bot)->getMoveToStatus())
            {
                answer = -20.0f;
                stopped = true;
            }
        }
        else
        {
            closingFast = false;
        }
        if (!(gap <= 8.0f) && !closingFast)
            continue;
        if (moving)
        {
            const float facingAlong = otherVelocity.z * lane.z + otherVelocity.y * lane.y + otherVelocity.x * lane.x;
            if (facingAlong < 0.6f && !(facingAlong <= -0.75f) && !closingFast)
                continue;
        }
        if (static_cast<BotMain*>(bot)->getIgnoreObstacles()
            && static_cast<BotMain*>(bot)->getCollidedObject() == obstacles[i] && !closingFast)
            continue;
        if (other->getSide() == equipment->getSide() && gap < 5.0f)
            blocker = other;
        if (answer < best)
        {
            best = answer;
            bestHandle = obstacles[i];
        }
    }
    if (blocker && equipment->getUnit()->isOccupied())
        bot->getCommunicationAgent()->warnBlocker(blocker);
    static_cast<BotMain*>(bot)->setSlowDownToTarget(bestHandle);
    return best;
}

// The signed angle from one unit vector to another, in (-pi, pi]. PE 1014B650 (a file-local copy
// also lives in BotNavigation.cpp).
float steerFlatAngle(const Vec2& from, const Vec2& to)
{
    float cross = from.y * to.x - from.x * to.y;
    if (-1.0f > cross)
        cross = -1.0f;
    else if (1.0f < cross)
        cross = 1.0f;
    cross = cross < -1.0f ? -1.0f : (cross > 1.0f ? 1.0f : cross);
    float angle = static_cast<float>(1.5707963705062866 - std::acos(static_cast<double>(cross)));
    if (to.y * from.y + from.x * to.x < 0.0f)
    {
        if (angle > 0.0f)
            angle = 1.0f * 3.1415927410125732f - angle;
        else if (angle < 0.0f)
            angle = -1.0f * 3.1415927410125732f - angle;
        else
            angle = 0.0f * 3.1415927410125732f - angle;
    }
    return angle;
}

// Picks the obstacle to steer around (PE 101E1BA0; [INFERRED] name and parameter names). While
// driving forward, the navigation's obstacles (not soldiers, not the equipment) within the look
// distance ahead (the speed along the facing plus max(own radius, 5) plus both radii), in front
// and whose circle crosses the facing line are candidates, the one nearest that line winning; one
// more than 60 degrees off the facing only counts when it is the collided object or there is
// none. Without a candidate the collided object is used if it was among the obstacles. For the
// chosen one: `cosine` is the facing's cosine toward it, `touching` whether the radii overlap,
// `avoidAngle` the heading that passes it on the avoid side (`tangentAngle` its centre, or -666
// when already inside the avoidance circle), `offFacing` and `offPath` whether the facing and
// path lines clear its circle (the path also counts as clear when it turns back and the bot drives
// forward), and `pathAngle` the angle from the path (or the facing, when the two are not within
// cos 0.85) to it. An obstacle behind the path while the facing agrees with the path clears the
// collided object and the avoid handle and reports both lines clear, answering false. True with
// `avoidHandle` set when an obstacle was chosen.
bool chooseAvoidObstacle(Bot* bot, Information* equipment, IPIPhysical* ownPhysical, const Vec2& facing,
                         float& cosine, float& pathAngle, bool& offFacing, const Vec2& target, const Vec2& path,
                         float distance, bool& offPath, unsigned int& avoidHandle, unsigned int& collided,
                         bool& collidedSeen, float& avoidAngle, float& tangentAngle, float speedWeight,
                         bool& touching, int avoidSide, bool closeLook)
{
    IPIMobile* const mobile = equipment->getMobile();
    const float ownRadius = ownPhysical->getRadius();
    const std::vector<unsigned int>& obstacles = bot->getBotNavigation()->getObstacles();
    cosine = 0.0f;
    collidedSeen = false;
    unsigned int best = static_cast<unsigned int>(-1);
    float bestSide = 99999.0f;
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        const unsigned int handle = obstacles[i];
        if (collided != static_cast<unsigned int>(-1) && collided == handle)
            collidedSeen = true;
        if (speedWeight < 0.0f)
            continue;
        Information* const other = informationManager->getInformation(handle);
        if (equipment == other || !other)
            continue;
        IPIPhysical* const physical = other->getPhysical();
        if (!physical || ((other->getObjectTypeFlags() >> 21) & 1))
            continue;
        const float radii = physical->getRadius() + ownRadius;
        const Vec3& otherPosition = other->getPosition();
        const Vec3& ownPosition = equipment->getPosition();
        Vec3 offset;
        offset.x = otherPosition.x - ownPosition.x;
        offset.y = otherPosition.y - ownPosition.y;
        offset.z = otherPosition.z - ownPosition.z;
        const float xSquared = offset.x * offset.x;
        const float zSquared = offset.z * offset.z;
        const float distanceSquared = offset.y * offset.y + zSquared + xSquared;
        const Vec3 facing3 = equipment->getFaceing();
        const Vec3& velocity = mobile->getSpeed();
        const float margin = (5.0f >= ownRadius) ? 5.0f : ownRadius;
        const float look = facing3.z * velocity.z + facing3.y * velocity.y + facing3.x * velocity.x + margin + radii;
        if (look * look < distanceSquared)
            continue;
        if (!(distance * distance >= distanceSquared))
        {
            const float qx = offset.x - target.x;
            const float qz = offset.z - target.y;
            if (radii * radii < qx * qx + qz * qz)
                continue;
        }
        const float ahead = offset.x * facing.x + offset.z * facing.y;
        if (ahead < 0.0f)
            continue;
        const float sx = offset.x - facing.x * ahead;
        const float sz = offset.z - facing.y * ahead;
        const float side = sx * sx + sz * sz;
        if (radii * radii < side)
            continue;
        if (ahead / std::sqrt(zSquared + xSquared) < 0.5f && handle != collided
            && collided != static_cast<unsigned int>(-1))
            continue;
        if (side < bestSide)
        {
            bestSide = side;
            best = handle;
        }
    }
    if (best == static_cast<unsigned int>(-1))
    {
        if (!collidedSeen)
            return false;
        const unsigned int handle = collided;
        if (!informationManager->getInformation(handle) || handle == static_cast<unsigned int>(-1))
            return false;
        best = handle;
    }
    Information* const obstacle = informationManager->getInformation(best);
    if (!obstacle)
        return false;
    if (IPIMobile* const obstacleMobile = obstacle->getMobile())
        obstacleMobile->getSpeedMagnitude();   // read and dropped, as shipped
    IPIPhysical* const physical = obstacle->getPhysical();   // not null-checked, as shipped
    const float avoidRadius = physical->getRadius() * 0.8f + ownRadius;
    const float avoidRadiusSquared = avoidRadius * avoidRadius;
    const Vec3& ownFirst = equipment->getPosition();
    Vec2 own;
    own.x = equipment->getPosition().x;
    own.y = ownFirst.z;
    const Vec3& otherFirst = obstacle->getPosition();
    Vec2 centre;
    centre.x = obstacle->getPosition().x;
    centre.y = otherFirst.z;
    const float dx = centre.x - own.x;
    const float dz = centre.y - own.y;
    const float length = std::sqrt(dz * dz + dx * dx);
    if (!(length >= 9.999999747378752e-06f))
        return false;
    const float inverse = 1.0f / length;
    Vec2 toward;
    toward.x = dx * inverse;
    toward.y = dz * inverse;
    cosine = toward.y * facing.y + toward.x * facing.x;
    touching = physical->getRadius() + ownRadius > length;
    const float lead = closeLook ? 0.25f : 0.5f;
    const float predictedX = lead * mobile->getSpeed().x + own.x;
    const float predictedZ = lead * mobile->getSpeed().z + own.y;
    const float vx = centre.x - predictedX;
    const float vz = centre.y - predictedZ;
    const float span = std::sqrt(vz * vz + vx * vx);
    const float spanInverse = 1.0f / span;
    Vec2 unit;
    unit.x = vx * spanInverse;
    unit.y = vz * spanInverse;
    if (avoidRadius > span)
    {
        Vec2 tangent;
        if (avoidSide == 2)
        {
            tangent.x = unit.y;
            tangent.y = -unit.x;
        }
        else
        {
            tangent.y = unit.x;
            tangent.x = -unit.y;
        }
        avoidAngle = steerFlatAngle(facing, tangent);
        tangentAngle = -666.0f;
    }
    else
    {
        float ratio = avoidRadius / span;
        ratio = ratio < -1.0f ? -1.0f : (ratio > 1.0f ? 1.0f : ratio);   // PE 1006ED90
        avoidAngle = static_cast<float>(std::asin(static_cast<double>(ratio)));
        const float centreAngle = steerFlatAngle(facing, unit);
        tangentAngle = centreAngle;
        if (avoidSide == 2)
            avoidAngle = centreAngle + avoidAngle;
        else
            avoidAngle = centreAngle - avoidAngle;
    }
    const float reachFacing = cosine;
    const float fx = facing.x * reachFacing * length + own.x - centre.x;
    const float fz = facing.y * reachFacing * length + own.y - centre.y;
    offFacing = avoidRadiusSquared * 0.7f < fx * fx + fz * fz;
    const float reachPath = toward.y * path.y + toward.x * path.x;
    const float px = path.x * reachPath * length + own.x - centre.x;
    const float pz = path.y * reachPath * length + own.y - centre.y;
    if (px * px + pz * pz > avoidRadiusSquared)
        offPath = true;
    else if (!(reachPath < -0.1f))
        offPath = false;
    else
        offPath = speedWeight > 0.0f;
    if (toward.y * path.y + toward.x * path.x < 0.0f && facing.y * path.y + path.x * facing.x > 0.0f)
    {
        collided = static_cast<unsigned int>(-1);
        avoidHandle = static_cast<unsigned int>(-1);
        offPath = true;
        offFacing = true;
        return false;
    }
    if (std::fabs(facing.y * path.y + path.x * facing.x) > 0.85f)
        pathAngle = steerFlatAngle(path, toward);
    else
        pathAngle = steerFlatAngle(facing, toward);
    avoidHandle = best;
    return true;
}

// The horizontal part (x, z) of a position. PE 1014ADC0.
Vec2 steerFlat(const Vec3& position)
{
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;
    return flat;
}

// The conditional Vec2 normaliser. PE 100E9DE0.
void steerNormaliseFlat(Vec2& value)
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

// The conditional Vec3 normaliser. PE 1006EEC0.
void steerNormalise(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
}

// The signed angle of a flat target against a forward direction and its right-hand
// perpendicular. PE 1014B730 (a copy lives with the run-over statement).
float steerSignedAngle(const Vec2& forward, const Vec2& right, const Vec2& target)
{
    float sine = right.y * target.y + right.x * target.x;
    if (-1.0f > sine)
        sine = -1.0f;
    else if (!(1.0f >= sine))
        sine = 1.0f;
    sine = clampInput(sine, -1.0f, 1.0f);
    float angle = static_cast<float>(1.5707963705062866 - std::acos(static_cast<double>(sine)));
    if (forward.y * target.y + forward.x * target.x < 0.0f)
    {
        if (angle > 0.0f)
            angle = 1.0f * 3.1415927410125732f - angle;
        else if (angle < 0.0f)
            angle = -1.0f * 3.1415927410125732f - angle;
        else
            angle = 0.0f * 3.1415927410125732f - angle;
    }
    return angle;
}

// A probe along the pathfinding map from `from` toward `to`; the distance to the hit, or `miss`
// when the trace leaves no hit point. With `material`, material 4 answers `water` instead.
float steerProbe(BotNavigation* navigation, const Vec3& from, const Vec3& to, int equipmentType, bool* water)
{
    Vec3 hit;
    hit.x = hit.y = hit.z = 0.0f;
    unsigned int handle;
    unsigned char material;
    IAIPathfinding::instance->trace(navigation, true, to, hit, handle, equipmentType, -1, 0.0f, 6,
                                    water ? &material : 0);
    if (water)
        *water = material == 4;
    const float eps = 1.1920928955078125e-07f;   // PE 1027C5A0
    if (std::fabs(hit.x) > eps || std::fabs(hit.y) > eps || std::fabs(hit.z) > eps)
    {
        const float dx = hit.x - from.x;
        const float dy = hit.y - from.y;
        const float dz = hit.z - from.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return -1.0f;
}

// The fraction of the radius within which a probe counts as blocked. Writable data that nothing
// writes. PE 102AE1A0.
float s_tankCloseRange = 1.3f;
float s_carCloseRange = 1.3f;    // PE 102AE3B8

// Reverse and turn the short way back toward the path (the status 8 and 12 fallback): beyond a
// quarter turn the steering wraps to the angle left to a half turn.
void reverseTurn(float& throttle, float& steering)
{
    throttle = -1.0f;
    const float size = std::fabs(steering);
    if (size > 1.5707963705062866f)
        steering = (steering > 0.0f ? 1.0f : (steering < 0.0f ? -1.0f : 0.0f)) * (3.1415927410125732f - size);
}

// The path-following steering of tanks (PE 101C53E0) and cars (PE 101CB5C0): two copies of one
// body in the image, differing only where `car` is tested below ([CHOSEN, not recovered]
// packaging as one function). [INFERRED] name and parameter roles, from the move entries' use. It probes the pathfinding map around the tank (ahead, behind
// and to both sides, at its corners), picks an obstacle to steer around, and runs the bot's
// move-to status machine (0 drive on; 2/3 turn while reversing; 4/5/13/14 back off and turn
// around an obstacle; 6 steer around it; 8 reverse toward the path; 9/11 turn in place; 12 make
// room), writing the throttle (or 666 to stop at an occupied steer point), steering angle, speed
// weight (+-1 by the driving direction) and maximum speed factor. False only without mobile or
// physical information.
bool vehicleSteering(bool car, Bot* bot, Information* equipment, const Vec2& direction, float distance,
                     BAPAMove* move, float& throttle, float& steering, float& speedWeight, float& maxSpeedFactor)
{
    IPIMobile* const mobile = equipment->getMobile();
    IPIPhysical* const ownPhysical = equipment->getPhysical();
    if (!mobile || !ownPhysical)
        return false;
    const float radius = ownPhysical->getRadius();
    const Vec2 path = direction;
    const Vec3& ownPosition = equipment->getPosition();
    Vec2 flatPosition = steerFlat(ownPosition);
    Vec2 facing = steerFlat(equipment->getFaceing());
    steerNormaliseFlat(facing);
    Vec2 right;
    right.x = facing.y;   // PE 1014AE20
    right.y = -facing.x;
    const Vec2 movePoint = steerFlat(move->m_target);   // PE 1019B1F0
    float remaining = move->slot14();
    if (remaining == 0.0f)
    {
        const float dx = flatPosition.x - movePoint.x;
        const float dy = flatPosition.y - movePoint.y;
        remaining = std::sqrt(dx * dx + dy * dy);
    }
    const float probeRadius = radius * (car ? 10.0f : 5.0f);
    const float facingPathDot = facing.x * path.x + facing.y * path.y;
    steering = steerSignedAngle(facing, right, path);
    const bool turningLeft = steering < 0.0f;
    float speedSigned = mobile->getSpeedMagnitude();
    const Vec3& velocity = mobile->getSpeed();
    const Vec3 facing3 = equipment->getFaceing();
    speedWeight = (facing3.z * velocity.z + facing3.y * velocity.y + velocity.x * facing3.x < 0.0f) ? -1.0f : 1.0f;
    speedSigned = speedSigned * speedWeight;
    bool stoppedForUnit;
    const float slowDown = findSlowDownTarget(bot, equipment, velocity, speedSigned, ownPosition, stoppedForUnit);
    Vec3 forward;   // PE 1014ADD0
    forward.x = facing.x;
    forward.y = 0.0f;
    forward.z = facing.y;
    throttle = 1.0f;
    const int vehicleNumber = mobile->getVehicleNumber();
    BotNavigation* const navigation = bot->getBotNavigation();
    const float probeLength = radius + 10.0f;
    const IPIMobile::ValidPos& valid =
        mobile->getLastValidPosition(IAIPathfinding::instance->getTraceMapIndex());
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    int status = static_cast<BotMain*>(bot)->getMoveToStatus();
    const bool reversing = status >= 8;
    float backClear = 0.0f;
    float frontClear = 0.0f;
    // [CHOSEN, not recovered] const_cast: the tree declares the box const; retail refreshes it.
    LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(ownPhysical->getLocalBoundingBox());
    if (box->m_derivedDataDirty)
        box->calcRadius();
    const float length = box->m_size.z;
    const float width = box->m_size.x;
    const Vec3 base = valid.position;
    Vec3 sideways;
    sideways.x = right.x * width;
    sideways.y = width * 0.0f;
    sideways.z = right.y * width;
    float sideFront = sideways.y;   // a reused slot: the obstacle checks below start from it
    Vec3 cornerLeft;
    cornerLeft.x = forward.x * length - sideways.x;
    cornerLeft.y = 0.0f * length - sideways.y;
    cornerLeft.z = forward.z * length - sideways.z;
    steerNormalise(cornerLeft);
    Vec3 cornerRight;
    cornerRight.x = forward.x * length + sideways.x;
    cornerRight.y = 0.0f * length + sideways.y;
    cornerRight.z = forward.z * length + sideways.z;
    steerNormalise(cornerRight);

    Vec3 point;
    point.x = forward.x * probeLength + base.x;
    point.y = probeLength * 0.0f + base.y;
    point.z = forward.z * probeLength + base.z;
    float probe = steerProbe(navigation, base, point, equipmentType, 0);
    if (probe >= 0.0f)
        frontClear = probe;
    bool water;
    const Vec3* const corners[2] = { &cornerLeft, &cornerRight };
    for (int c = 0; c < 2; ++c)
    {
        point.x = corners[c]->x * probeLength + base.x;
        point.y = corners[c]->y * probeLength + base.y;
        point.z = corners[c]->z * probeLength + base.z;
        probe = steerProbe(navigation, base, point, equipmentType, &water);
        if (!water && probe >= 0.0f)
            frontClear = std::min(frontClear, probe);   // PE 100C1A20
    }
    point.x = base.x - forward.x * probeLength;
    point.y = base.y - probeLength * 0.0f;
    point.z = base.z - forward.z * probeLength;
    probe = steerProbe(navigation, base, point, equipmentType, 0);
    if (probe >= 0.0f)
        backClear = probe;
    obstacleClearance(bot, base, forward, sideFront, backClear, static_cast<unsigned int>(-1));
    for (int c = 0; c < 2; ++c)
    {
        point.x = base.x - corners[c]->x * probeLength;
        point.y = base.y - corners[c]->y * probeLength;
        point.z = base.z - corners[c]->z * probeLength;
        probe = steerProbe(navigation, base, point, equipmentType, &water);
        if (!water && probe >= 0.0f)
            backClear = std::min(backClear, probe);
        obstacleClearance(bot, base, *corners[c], sideFront, backClear, static_cast<unsigned int>(-1));
    }
    // The two sideways probes: material 4 counts as fully clear, no hit as blocked.
    point.x = right.x * probeLength + base.x;
    point.y = probeLength * 0.0f + base.y;
    point.z = right.y * probeLength + base.z;
    probe = steerProbe(navigation, base, point, equipmentType, &water);
    const float rightClear = water ? probeLength : (probe >= 0.0f ? probe : 0.0f);
    point.x = base.x - right.x * probeLength;
    point.y = base.y - probeLength * 0.0f;
    point.z = base.z - right.y * probeLength;
    probe = steerProbe(navigation, base, point, equipmentType, &water);
    const float leftClear = water ? probeLength : (probe >= 0.0f ? probe : 0.0f);
    (void)vehicleNumber;

    const float closeRange = (car ? s_carCloseRange : s_tankCloseRange) * radius;
    unsigned int avoidHandle = static_cast<unsigned int>(-1);
    unsigned int collided = static_cast<BotMain*>(bot)->getCollidedObject();
    int avoidSide = static_cast<BotMain*>(bot)->getAvoidSide();
    Vec2 target;
    target.x = distance * direction.x;
    target.y = distance * direction.y;
    float cosine;
    float pathAngle;
    bool offFacing;
    bool offPath;
    bool collidedSeen;
    float avoidAngle;
    float tangentAngle;
    bool touching;
    bool avoiding = chooseAvoidObstacle(bot, equipment, ownPhysical, facing, cosine, pathAngle, offFacing, target,
                                        path, distance, offPath, avoidHandle, collided, collidedSeen, avoidAngle,
                                        tangentAngle, speedWeight, touching, avoidSide, car);
    bool clearCollided = false;
    if (collided != static_cast<unsigned int>(-1) && !collidedSeen)
    {
        avoiding = false;
        clearCollided = true;
    }
    else if (!avoiding)
    {
        clearCollided = true;
    }
    else
    {
        Information* const obstacle = informationManager->getInformation(avoidHandle);
        const float settle = static_cast<BotMain*>(bot)->getIgnoreObstacles() ? 6.25f : 1.0f;
        if (avoidHandle == collided && collided != static_cast<unsigned int>(-1))
        {
            const Vec3& now = obstacle->getPosition();
            const Vec3& then = static_cast<BotMain*>(bot)->getCollidedObjectPosition();
            const float mx = now.x - then.x;
            const float my = now.y - then.y;
            const float mz = now.z - then.z;
            if (!(mx * mx + my * my + mz * mz <= settle))
            {
                avoiding = false;
                clearCollided = true;
            }
        }
        else
        {
            static_cast<BotMain*>(bot)->setCollidedObjectPosition(obstacle->getPosition());
        }
    }
    bool sidesBlocked = false;
    if (clearCollided)
    {
        static_cast<BotMain*>(bot)->setCollidedObject(static_cast<unsigned int>(-1));
        avoidHandle = static_cast<unsigned int>(-1);
        collided = static_cast<unsigned int>(-1);
    }
    if (!avoiding)
    {
        static_cast<BotMain*>(bot)->setAvoidSide(0);
    }
    else
    {
        static_cast<BotMain*>(bot)->setCollidedObject(avoidHandle);
        collided = avoidHandle;
        if (!avoidSide)
        {
            bool goRight = true;
            Information* const obstacle = informationManager->getInformation(avoidHandle);
            IPIMobile* const obstacleMobile = obstacle->getMobile();
            IPIPhysical* const obstaclePhysical = obstacle->getPhysical();
            const IPIMobile::ValidPos* obstacleValid = 0;
            int number = 0;
            float halfWidth = 0.0f;
            float reach = 0.0f;
            if (obstacleMobile)
            {
                halfWidth = obstaclePhysical->getSmallestRadius() * 0.5f + radius;
                reach = halfWidth + 0.5f;
                number = mobile->getVehicleNumber();
                obstacleValid = &obstacleMobile->getLastValidPosition(number);
                if (obstacleValid->polygon == static_cast<unsigned int>(-1)
                    || obstacleValid->quadTree == static_cast<unsigned int>(-1))
                    obstacleValid = 0;
            }
            if (obstacleValid)
            {
                Vec3 hitA;
                hitA.x = hitA.y = hitA.z = 0.0f;
                Vec3 hitB;
                hitB.x = hitB.y = hitB.z = 0.0f;
                unsigned int handle;
                const Vec3& axis = *reinterpret_cast<const Vec3*>(&equipment->getTransformation().m[0]);
                Vec3 end;
                end.x = obstacleValid->position.x + axis.x * reach;   // PE 10070A70 / 10070A40
                end.y = obstacleValid->position.y + axis.y * reach;
                end.z = obstacleValid->position.z + axis.z * reach;
                IAIPathfinding::instance->trace(obstacleValid->polygon, obstacleValid->quadTree, number,
                                                obstacleValid->position, end, hitA, handle, equipmentType, -1,
                                                0.0f, 0, 0);
                const float eps = 1.1920928955078125e-07f;
                float spaceA = 0.0f;
                if (std::fabs(hitA.x) > eps || std::fabs(hitA.y) > eps || std::fabs(hitA.z) > eps)
                {
                    const float dx = hitA.x - obstacleValid->position.x;
                    const float dy = hitA.y - obstacleValid->position.y;
                    const float dz = hitA.z - obstacleValid->position.z;
                    spaceA = std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                const Vec3& axisAgain = *reinterpret_cast<const Vec3*>(&equipment->getTransformation().m[0]);
                end.x = obstacleValid->position.x - axisAgain.x * reach;   // PE 1007A380
                end.y = obstacleValid->position.y - axisAgain.y * reach;
                end.z = obstacleValid->position.z - axisAgain.z * reach;
                IAIPathfinding::instance->trace(obstacleValid->polygon, obstacleValid->quadTree, number,
                                                obstacleValid->position, end, hitB, handle, equipmentType, -1,
                                                0.0f, 0, 0);
                float spaceB = 0.0f;
                if (std::fabs(hitB.x) > eps || std::fabs(hitB.y) > eps || std::fabs(hitB.z) > eps)
                {
                    const float dx = hitB.x - obstacleValid->position.x;
                    const float dy = hitB.y - obstacleValid->position.y;
                    const float dz = hitB.z - obstacleValid->position.z;
                    spaceB = std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                const bool moreOnB = spaceB > spaceA;
                const Vec3& axisThird = *reinterpret_cast<const Vec3*>(&equipment->getTransformation().m[0]);
                obstacleClearance(bot, obstacleValid->position, axisThird, spaceA, spaceB, collided);
                const bool bothRoomy = std::min(spaceB, spaceA) > halfWidth;
                const float wider = std::max(spaceB, spaceA);
                sidesBlocked = wider < halfWidth;
                const bool tooNarrow = wider < halfWidth * 0.7f;
                if (bothRoomy)
                {
                    Vec2 across;
                    across.x = path.y;   // PE 1014AE20
                    across.y = -path.x;
                    const float dx = obstacleValid->position.x - flatPosition.x;
                    const float dz = obstacleValid->position.z - flatPosition.y;
                    goRight = across.y * dz + dx * across.x > 0.0f;
                }
                else if (tooNarrow)
                {
                    goRight = moreOnB;
                }
                else
                {
                    goRight = spaceB > spaceA;
                }
            }
            const int side = goRight ? 1 : 2;
            avoidSide = side;
            if (static_cast<BotMain*>(bot)->setAvoidSide(side) && !static_cast<BotMain*>(bot)->getIgnoreObstacles())
            {
                status = 0;
                static_cast<BotMain*>(bot)->setMoveToStatus(0);
                static_cast<BotMain*>(bot)->setIgnoreObstacles(true);
            }
        }
    }

    // Recomputed below; both default to false when no side was chosen this tick.
    bool hasCollided = collided != static_cast<unsigned int>(-1);
    static_cast<BotMain*>(bot)->setUnresolvedCollisionObject(collided);
    if (hasCollided)
    {
        Information* const obstacle = informationManager->getInformation(collided);
        if (!obstacle)
        {
            hasCollided = false;
            collided = static_cast<unsigned int>(-1);
        }
        else
        {
            const Vec2 centre = steerFlat(*reinterpret_cast<const Vec3*>(&obstacle->getTransformation().m[12]));
            const float obstacleRadius = obstacle->getPhysical()->getRadius();
            const float tx = movePoint.x - centre.x;
            const float ty = movePoint.y - centre.y;
            if (!(obstacleRadius * obstacleRadius <= ty * ty + tx * tx))
            {
                const float ox = centre.x - flatPosition.x;
                const float oy = centre.y - flatPosition.y;
                const float near = obstacle->getPhysical()->getRadius() + radius + 3.0f;
                if (!(near * near <= oy * oy + ox * ox))
                {
                    throttle = 666.0f;
                    steering = 0.0f;
                    return true;
                }
            }
        }
    }
    else
    {
        avoidSide = 0;
        static_cast<BotMain*>(bot)->setAvoidSide(0);
    }

    const bool pathLeft = pathAngle < 0.0f;
    const float roomA = radius * 2.7f;
    const float sideSum = leftClear + rightClear;
    const bool roomyA = sideSum > roomA && backClear + frontClear > roomA;
    const float roomB = radius * 3.0f;
    const bool roomyB = sideSum > roomB && backClear + frontClear > roomB;
    const float frontFraction = (frontClear - closeRange) / (probeLength - closeRange);
    const float backFraction = (backClear - closeRange) / (probeLength - closeRange);
    bool openGround;
    if (frontClear == 0.0f && backClear == 0.0f)
    {
        openGround = true;
        status = facingPathDot > 0.0f ? 0 : 8;
    }
    else
    {
        openGround = false;
    }
    const float maxSpeed = mobile->getMaxSpeed();
    const float halfSpeed = 0.5f * maxSpeed;
    if (!car)
        maxSpeedFactor = maxSpeed - (1.0f - (reversing ? backFraction : frontFraction)) * (maxSpeed - halfSpeed);
    else if (reversing)
        maxSpeedFactor = 5.0f - (1.0f - backFraction) * 3.0f;
    else if (!(frontFraction >= 0.95f))
        maxSpeedFactor = 5.0f - (1.0f - frontFraction) * 3.0f;
    else
        maxSpeedFactor = maxSpeed - (1.0f - frontFraction) * (maxSpeed - 2.0f);
    if (sidesBlocked && !(maxSpeedFactor < 7.0f))
        maxSpeedFactor = 7.0f;
    if (!hasCollided)
    {
        static_cast<BotMain*>(bot)->setIgnoreObstacles(false);
        static_cast<BotMain*>(bot)->setAvoidSide(0);
    }
    bool ignoring = static_cast<BotMain*>(bot)->getIgnoreObstacles();
    if (ignoring)
    {
        bool humanBlocker = false;
        if (hasCollided)
        {
            Information* const obstacle = informationManager->getInformation(collided);
            IAIObjectPhysical* const physics = obstacle->getAIObject()->getPhysical();
            physics->enablePhysics(1);
            physics->enablePhysics(2);
            if (static_cast<BotMain*>(bot)->getSlowDownToTarget() == collided)
                ignoring = true;
            if (obstacle->getAIObject() && !IBotManager::instance->getBot(obstacle->getAIObject())
                && obstacle->getUnit() && obstacle->getUnit()->isOccupied())
            {
                humanBlocker = true;
                ignoring = false;
            }
        }
        if (!humanBlocker && ignoring
            && (status == 4 || status == 5 || status == 6 || status == 13 || status == 14))
        {
            status = 0;
            static_cast<BotMain*>(bot)->setMoveToStatus(0);
        }
    }

    BotMain* const main = static_cast<BotMain*>(bot);
    // The throttle and steering for a turn on the spot away from an obstacle, by the speed.
    const float turnStep = std::min(1.0f, std::fabs(speedSigned));   // PE 100C1A20
    const bool frontOpen = !(frontClear < closeRange);
    const bool backOpen = !(backClear < closeRange);
    // Whether the obstacle ahead still blocks a forward turn (the case 2/3 guards).
    const bool blockedAhead = cosine > 0.5f && hasCollided && !offFacing && !ignoring;
    switch (status)
    {
    case 0:
        if (openGround)
        {
            throttle = 1.0f;
            maxSpeedFactor = halfSpeed;
            break;
        }
        if (!(facingPathDot < 0.0f) && frontOpen)
        {
            if (avoiding && !ignoring)
                main->setMoveToStatus(6);
            else
                throttle = 1.0f;
            break;
        }
        if (remaining <= probeRadius && facingPathDot < -0.6f && backClear > closeRange)
        {
            main->setMoveToStatus(8);
            break;
        }
        if (facingPathDot < 0.0f && !roomyA)
        {
            main->setMoveToStatus(12);
            break;
        }
        main->setMoveToStatus(turningLeft ? 9 : 11);
        break;
    case 12:
        if (facingPathDot > 0.0f)
        {
            main->setMoveToStatus(0);
            break;
        }
        if (remaining <= probeRadius && facingPathDot < -0.6f && backClear > closeRange)
        {
            main->setMoveToStatus(8);
            break;
        }
        if (!roomyB)
        {
            reverseTurn(throttle, steering);
            break;
        }
        main->setMoveToStatus(leftClear >= rightClear ? 11 : 9);
        break;
    case 3:
        if (turningLeft)
        {
            if (facingPathDot > 0.9f)
                main->setMoveToStatus(0);
            else if (!frontOpen || blockedAhead)
                main->setMoveToStatus(9);
            else
            {
                throttle = 1.0f;
                steering = -std::fabs(steering);
            }
            break;
        }
        main->setMoveToStatus(facingPathDot > 0.0f ? 2 : 11);
        break;
    case 11:
        if (turningLeft)
        {
            main->setMoveToStatus(facingPathDot > 0.0f ? 3 : 9);
            break;
        }
        if (facingPathDot > 0.9f)
            main->setMoveToStatus(0);
        else if (!backOpen)
            main->setMoveToStatus(2);
        else
        {
            throttle = -1.0f;
            steering = -std::fabs(steering);
        }
        break;
    case 2:
        if (turningLeft)
        {
            main->setMoveToStatus(facingPathDot > 0.0f ? 3 : 9);
            break;
        }
        if (facingPathDot > 0.9f)
            main->setMoveToStatus(0);
        else if (!frontOpen || blockedAhead)
            main->setMoveToStatus(11);
        else
        {
            throttle = 1.0f;
            steering = std::fabs(steering);
        }
        break;
    case 9:
        if (!turningLeft)
        {
            main->setMoveToStatus(facingPathDot > 0.0f ? 2 : 11);
            break;
        }
        if (facingPathDot > 0.9f && frontClear > closeRange)
            main->setMoveToStatus(0);
        else if (!backOpen)
            main->setMoveToStatus(3);
        else
        {
            throttle = -1.0f;
            steering = std::fabs(steering);
        }
        break;
    case 8:
        if (openGround)
        {
            throttle = -1.0f;
            const float flipped = steering * -1.0f;
            steering = flipped;
            const float size = std::fabs(flipped);
            if (size > 1.5707963705062866f)
                steering = (flipped > 0.0f ? 1.0f : (flipped < 0.0f ? -1.0f : 0.0f)) * (3.1415927410125732f - size);
            break;
        }
        if (facingPathDot > -0.5f || probeRadius + 5.0f < remaining || backClear < closeRange)
        {
            main->setMoveToStatus(0);
            throttle = 1.0f;
            break;
        }
        if (cosine < -0.6f && hasCollided)
        {
            main->setMoveToStatus(pathLeft ? 3 : 2);
            break;
        }
        reverseTurn(throttle, steering);
        break;
    case 6:
        if (hasCollided && !(offPath && cosine < 0.3f))
        {
            if (touching && !offFacing && cosine > 0.0f)
            {
                main->setMoveToStatus(avoidSide == 2 ? 14 : 13);
                break;
            }
            if (!(frontClear < closeRange))
            {
                steering = avoidAngle;
                break;
            }
            const int other = avoidSide == 2 ? 1 : 2;
            if (main->setAvoidSide(other) && !main->getIgnoreObstacles())
            {
                main->setMoveToStatus(0);
                main->setIgnoreObstacles(true);
            }
            else
            {
                main->setMoveToStatus(avoidSide == 2 ? 13 : 14);
            }
            break;
        }
        main->setMoveToStatus(steering >= 0.0f ? 2 : 3);
        break;
    // Backing off and turning around an obstacle: a tank steers by its speed (at most 0.2), a car
    // at full lock (2); a car stops backing off as soon as it touches the obstacle.
    case 13:
        if (!hasCollided || (offFacing && !pathLeft))
            main->setMoveToStatus(6);
        else if (!backOpen)
            main->setMoveToStatus(4);
        throttle = -1.0f;
        steering = car ? 2.0f : turnStep * 0.2f;
        break;
    case 14:
        if (!hasCollided || (offFacing && pathLeft))
            main->setMoveToStatus(6);
        else if (!backOpen)
            main->setMoveToStatus(5);
        throttle = -1.0f;
        steering = car ? -2.0f : turnStep * -0.2f;
        break;
    case 4:
        if (!hasCollided || (offFacing && !pathLeft))
        {
            main->setMoveToStatus(6);
            break;
        }
        if (frontClear < closeRange || (touching && (car || cosine > 0.8f)))
        {
            main->setMoveToStatus(13);
            break;
        }
        throttle = 1.0f;
        steering = car ? -2.0f : turnStep * -0.2f;
        break;
    case 5:
        if (!hasCollided || (offFacing && pathLeft))
        {
            main->setMoveToStatus(6);
            break;
        }
        if (frontClear < closeRange || (touching && (car || cosine > 0.8f)))
        {
            main->setMoveToStatus(14);
            break;
        }
        throttle = 1.0f;
        steering = car ? 2.0f : turnStep * 0.2f;
        break;
    default:
        break;
    }
    if (slowDown < maxSpeedFactor)
        maxSpeedFactor = slowDown;
    if (stoppedForUnit)
        steering = -2.0f;
    return true;
}

// 1, -1 or 0 by the sign. PE 100DE7B0.
float tankSign(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (value < 0.0f)
        return -1.0f;
    return 0.0f;
}

// Steers and throttles a tank (PE 101C7020; [INFERRED] name). As driveCar, except: steering
// fades in with the speed (none below s_tankSteerMinSpeed, full from s_tankSteerFullSpeed, as
// the square of the fraction); turning in place is not negated; there is no easing for roll or
// pitch; and the throttle chases the target speed with a signed square. The equipment's flat axes
// are read and dropped, and the direction is not read, as shipped. Always true.
bool driveTank(Information* equipment, IPIControlInfo* control, IPIMobile* mobile, IPIPhysical* /*physical*/,
               io::PlayerInput* input, const Vec2* /*direction*/, float cruise, float throttle, float steering,
               float speedWeight, float maxSpeedFactor)
{
    const Mat4& transformation = equipment->getTransformation();
    (void)transformation;
    const Vec3& velocity = mobile->getSpeed();
    const float speed = std::sqrt(velocity.z * velocity.z + velocity.x * velocity.x);
    float steer = steering;
    if (throttle != 0.0f && speedWeight != throttle && speed > 1.0f)
        steer = steering * -1.0f;
    const float speedSize = std::fabs(speed);
    if (!(speedSize >= s_tankSteerMinSpeed))
        steer = 0.0f;
    float fraction = speedSize / s_tankSteerFullSpeed;
    if (!(1.0f >= fraction))
        fraction = 1.0f;
    steer = fraction * fraction * steer;
    const float direction = steer > 0.0f ? 1.0f : (steer < 0.0f ? -1.0f : 0.0f);
    if (cruise != 0.0f)
    {
        IAIObjectPhysical* const physics = equipment->getAIObject()->getPhysical();
        physics->enablePhysics(1);
        physics->enablePhysics(2);
    }
    if (throttle == 0.0f)
    {
        float size = std::fabs(steer);
        if (throttle == -1.0f)
            size = size - 3.1415927410125732f;
        float lock = std::fabs(size * 3.0f);
        if (!(1.0f >= lock))
            lock = 1.0f;
        input->setInput(control->getDriveTurnControl(), lock * direction);
        input->setInput(control->getDriveThrottleControl(), throttle);
        return true;
    }
    float wheel = steer * 3.0f;
    if (-1.0f > wheel)
        wheel = -1.0f;
    else if (!(1.0f >= wheel))
        wheel = 1.0f;
    input->setInput(control->getDriveTurnControl(), wheel);
    float target = cruise * throttle;
    if (std::fabs(target) > maxSpeedFactor)
        target = tankSign(target) * maxSpeedFactor;
    const float chase = (target - speed * speedWeight) * 0.3333333432674408f;
    float size;
    float sign;
    if (!(1.0f >= chase))
    {
        size = 1.0f;
        sign = 1.0f;
    }
    else
    {
        size = chase;
        sign = chase > 0.0f ? 1.0f : (chase < 0.0f ? -1.0f : 0.0f);
    }
    float pedal = clampInput(sign * size * size + target * 0.06666667014360428f, -1.0f, 1.0f);
    if (pedal != 0.0f && speedSize < 1.5f)
        pedal = tankSign(pedal);
    input->setInput(control->getDriveThrottleControl(), pedal);
    return true;
}

} // anonymous namespace

// PE 10139250. The tank form of EntryCarMoveTo: the same flow over the tank's helpers, without
// the up-front finished/failed defaults or an equipment null check, as shipped. Physics is
// enabled when off. The turn factor applies while faster than 3 (sharpness past 0.35, times 2.5,
// slowing by up to 0.7), and the approach factor uses the squared distance to the steer point
// less the smallest radius and 0.5, over 196, never below 0.35.
bool EntryTankMoveTo::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment->getMobile();
    IPIControlInfo* const control = equipment->getControlInfo();
    IPIPhysical* const physical = equipment->getPhysical();
    if (!mobile || !control || !physical)
    {
        failed = true;
        finished = false;
        return true;
    }
    IAIObjectPhysical* const physics = equipment->getAIObject()->getPhysical();
    if (s_forceTankPhysics || !physics->hasPhysicsEnabled())
        physics->enablePhysics(equipment->getSide());
    BAPAMove* const move = static_cast<BAPAMove*>(script);
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double lastCheck = static_cast<BotMain*>(bot)->getLastCheckMoveToFinishedTime();
    failed = false;
    if (now - lastCheck > 1.0)
    {
        static_cast<BotMain*>(bot)->setLastCheckMoveToFinishedTime(now);
        if (move->isFinished(bot))
        {
            failed = true;
            finished = true;
            return true;
        }
    }

    Vec2 steer;
    steer.x = steer.y = 0.0f;
    bool steerFailed;
    finished = move->getSteering(steer, failed, steerFailed, 0);
    if (!finished || steerFailed || failed)
    {
        const bool stop = !finished;
        io::PlayerInput* const input = bot->playerInput();
        const Vec3& speed = mobile->getSpeed();
        const Vec3 facing = equipment->getFaceing();
        const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;   // PE 1006EE50
        finished = resetVehicleControls(input, true, false, false, forward, s_tankBrakeLimit);
        if (stop)
            failed = true;
        return true;
    }

    // Two reads: z comes from the first, x from the second, as shipped.
    const Vec3& first = equipment->getPosition();
    Vec2 flatPosition;
    flatPosition.x = equipment->getPosition().x;
    flatPosition.y = first.z;
    equipment->getTransformation();   // read twice and dropped, as shipped
    equipment->getTransformation();
    Vec2 direction;
    direction.x = steer.x - flatPosition.x;
    direction.y = steer.y - flatPosition.y;
    const float distance = std::sqrt(direction.y * direction.y + direction.x * direction.x);
    if (distance > 9.999999747378752e-06f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
    }
    float turnFactor = 1.0f;
    float turn;
    if (std::fabs(mobile->getSpeedMagnitude()) > 3.0f && move->getPathTurn(turn))
    {
        const float sharpness = std::max(std::fabs(turn) - 0.35f, 0.0f) * 2.5f;   // PE 10086630
        turnFactor = 1.0f - std::min(1.0f, sharpness) * 0.7f;                     // PE 100C1A20
    }
    float throttle;
    float steering;
    float speedWeight;
    float maxSpeedFactor;
    finished = vehicleSteering(false, bot, equipment, direction, distance, move, throttle, steering, speedWeight,
                               maxSpeedFactor);   // PE 101C53E0
    if (throttle == 666.0f)
    {
        io::PlayerInput* const input = bot->playerInput();
        const Vec3& speed = mobile->getSpeed();
        const Vec3 facing = equipment->getFaceing();
        const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;
        finished = resetVehicleControls(input, true, false, false, forward, s_tankBrakeLimit);
        failed = true;
        return true;
    }
    if (!finished)
        return true;
    const float maxSpeed = mobile->getMaxSpeed();
    const float dx = steer.x - flatPosition.x;
    const float dz = steer.y - flatPosition.y;
    const float remainingSquared = dz * dz + dx * dx;
    float approach = 1.0f;
    const float clearance = remainingSquared - physical->getSmallestRadius() - 0.5f;
    if (clearance < 196.0f)
        approach = std::max(0.35f, clearance / 196.0f);
    const float cruise = std::min(approach, turnFactor) * maxSpeed;
    finished = driveTank(equipment, control, mobile, physical, bot->playerInput(), &direction, cruise, throttle,
                         steering, speedWeight, maxSpeedFactor);
    return true;
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

namespace {

bool vehicleSteering(bool car, Bot* bot, Information* equipment, const Vec2& direction, float distance,
                     BAPAMove* move, float& throttle, float& steering, float& speedWeight, float& maxSpeedFactor);

// The car's path-following steering. PE 101CB5C0.
bool carSteering(Bot* bot, Information* equipment, const Vec2& direction, float distance, BAPAMove* move,
                 float& throttle, float& steering, float& speedWeight, float& maxSpeedFactor)
{
    return vehicleSteering(true, bot, equipment, direction, distance, move, throttle, steering, speedWeight,
                           maxSpeedFactor);
}

} // anonymous namespace

// PE 10137CD0. Steers a car along its move. Without equipment, mobile, control or physical
// information it fails unfinished. At most once a second (game time) it asks the move whether it
// is finished. A steer that is not ready, failed or asked for a stop releases the movement group
// while braking against the forward speed. Otherwise the cruise speed is the vehicle's maximum
// speed times the smaller of the turn factor (slowing for sharp path turns while rolling faster
// than 1) and the approach factor (slowing within 15 of the steer point, beyond the vehicle's
// smallest radius plus 0.5, never below 0.2).
bool EntryCarMoveTo::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    finished = true;
    failed = false;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
    {
        failed = true;
        finished = false;
        return true;
    }
    IAIObjectPhysical* const physics = equipment->getAIObject()->getPhysical();
    if (!physics->hasPhysicsEnabled())
        physics->enablePhysics(equipment->getSide());
    IPIMobile* const mobile = equipment->getMobile();
    IPIControlInfo* const control = equipment->getControlInfo();
    IPIPhysical* const physical = equipment->getPhysical();
    if (!mobile || !control || !physical)
    {
        failed = true;
        finished = false;
        return true;
    }
    BAPAMove* const move = static_cast<BAPAMove*>(script);
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double lastCheck = static_cast<BotMain*>(bot)->getLastCheckMoveToFinishedTime();
    failed = false;
    if (now - lastCheck > 1.0)
    {
        static_cast<BotMain*>(bot)->setLastCheckMoveToFinishedTime(now);
        if (move->isFinished(bot))
        {
            failed = true;
            finished = true;
            return true;
        }
    }

    Vec2 steer;
    steer.x = steer.y = 0.0f;
    bool steerFailed;
    finished = move->getSteering(steer, failed, steerFailed, 0);
    if (!finished)
    {
        io::PlayerInput* const input = bot->playerInput();
        const Vec3& speed = mobile->getSpeed();
        const Vec3 facing = equipment->getFaceing();
        const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;   // PE 1006EE50
        finished = resetVehicleControls(input, true, false, false, forward, s_carBrakeLimit);
        failed = true;
        return true;
    }
    if (steerFailed || failed)
    {
        io::PlayerInput* const input = bot->playerInput();
        const Vec3& speed = mobile->getSpeed();
        const Vec3 facing = equipment->getFaceing();
        const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;
        finished = resetVehicleControls(input, true, false, false, forward, s_carBrakeLimit);
        return true;
    }

    // Two reads: z comes from the first, x from the second, as shipped.
    const Vec3& first = equipment->getPosition();
    Vec2 flatPosition;
    flatPosition.x = equipment->getPosition().x;
    flatPosition.y = first.z;
    equipment->getTransformation();   // read twice and dropped, as shipped
    equipment->getTransformation();
    Vec2 direction;
    direction.x = steer.x - flatPosition.x;
    direction.y = steer.y - flatPosition.y;
    const float distance = std::sqrt(direction.y * direction.y + direction.x * direction.x);
    if (distance > 9.999999974752427e-07f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
    }
    float turnFactor = 1.0f;
    float turn;
    if (std::fabs(mobile->getSpeedMagnitude()) > 1.0f && move->getPathTurn(turn))
    {
        const float sharpness = std::max(std::fabs(turn) - 0.2f, 0.0f) * 1.8f;   // PE 10086630
        turnFactor = 1.0f - std::min(1.0f, sharpness) * 0.8f;                     // PE 100C1A20
    }
    float throttle;
    float steering;
    float speedWeight;
    float maxSpeedFactor;
    finished = carSteering(bot, equipment, direction, distance, move, throttle, steering, speedWeight,
                           maxSpeedFactor);
    if (throttle == 666.0f)
    {
        io::PlayerInput* const input = bot->playerInput();
        const Vec3& speed = mobile->getSpeed();
        const Vec3 facing = equipment->getFaceing();
        const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;
        finished = resetVehicleControls(input, true, false, false, forward, s_carBrakeLimit);
        failed = true;
        return true;
    }
    if (!finished)
        return true;
    const float maxSpeed = mobile->getMaxSpeed();
    float approach = 1.0f;
    Vec2 flatSpeed;
    const Vec3& velocity = mobile->getSpeed();
    flatSpeed.x = velocity.x;   // PE 1014ADC0; read and dropped
    flatSpeed.y = velocity.z;
    (void)flatSpeed;
    const float dx = steer.x - flatPosition.x;
    const float dz = steer.y - flatPosition.y;
    const float remaining = std::sqrt(dz * dz + dx * dx);
    const float clearance = std::max(remaining - physical->getSmallestRadius() - 0.5f, 0.0f);
    const float clearanceSquared = clearance * clearance;
    if (clearanceSquared < 225.0f)
        approach = std::max(0.2f, clearanceSquared * 0.004444444552063942f);
    const float cruise = std::min(approach, turnFactor) * maxSpeed;
    finished = driveCar(equipment, control, mobile, physical, bot->playerInput(), cruise, throttle, steering,
                        speedWeight, maxSpeedFactor);
    return true;
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


// PE 10137630. A statement with an input mask releases its one input; any other releases the
// car's control groups, braking against the speed along the facing, and fails. The equipment is
// not null-checked, as shipped.
bool EntryCarResetControls::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    BAPResetControls* const reset = static_cast<BAPResetControls*>(script);
    if (reset->hasInputMask())
    {
        bot->playerInput()->setInput(static_cast<io::PlayerInputMap>(reset->m_input), 0.0f);
        finished = true;
        failed = true;
        return true;
    }
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment->getMobile();
    failed = true;
    mobile->getSpeed();          // read and dropped, as shipped
    equipment->getFaceing();     // likewise
    equipment->getTransformation();
    const Vec3& speed = mobile->getSpeed();
    const Vec3 facing = equipment->getFaceing();
    const float forward = speed.z * facing.z + speed.y * facing.y + speed.x * facing.x;
    finished = resetVehicleControls(bot->playerInput(), reset->m_movement, reset->m_look, reset->m_actions, forward,
                                    s_carBrakeLimit);
    return true;
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

// PE 10134710. Steers a soldier toward the move's object like EntryInfanteryMoveTo, avoiding
// obstacles other than the object itself. Close after a failed static attack the failure time is
// renewed; otherwise a pending wish to simulate artillery is turned into a failed sky attack. A
// move that reports itself stuck makes the player commit suicide.
bool EntryInfanteryMoveToObject::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    Vec2 flat;
    flat.x = flat.y = 0.0f;
    Vec2 avoid;
    avoid.x = avoid.y = 0.0f;
    Vec2 steer;
    steer.x = steer.y = 0.0f;

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment ? equipment->getMobile() : 0;
    if (!equipment || !mobile)
    {
        failed = true;
        finished = false;
        return true;
    }

    IAIObjectPhysical* const physical = equipment->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(equipment->getSide());

    BAPAMove* const move = static_cast<BAPAMove*>(script);
    if (!informationManager->getInformation(move->getTargetObject()))
    {
        failed = true;
        finished = false;
        return true;
    }
    if (bot->isFireAllowed() && move->isFinished(bot))
    {
        failed = true;
        finished = true;
        return true;
    }

    failed = false;
    BotMain* const main = static_cast<BotMain*>(bot);
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - main->getTimeOfLastFailedStaticSkyAttack() < 1.5)
    {
        main->setTimeOfLastFailedStaticAttack(now);
    }
    else if (main->getBotWantsToSimulateArtillery())
    {
        main->setTimeOfLastFailedStaticSkyAttack(now);
        main->setBotWantsToSimulateArtillery(false);
    }
    vec3ToVec2XZ(equipment->getPosition(), flat);   // the result is unused, as shipped

    bool steerFailed;
    finished = move->getSteering(steer, failed, steerFailed, 0);
    if (!finished)
    {
        finished = resetSoldierControls(bot, bot->playerInput(), true, false, false);
        failed = true;
        return true;
    }
    if (steerFailed || failed)
    {
        finished = resetSoldierControls(bot, bot->playerInput(), true, false, false);
        return true;
    }

    Information* const target = informationManager->getInformation(move->getTargetObject());
    if (move->getAvoidanceSteering(bot, steer, avoid, target, move->slot8()))
    {
        finished = resetSoldierControls(bot, bot->playerInput(), true, false, false);
        failed = true;
        return true;
    }

    bot->getSensingAgent()->setLookDirection(avoid);
    toggleSoldierZoom(bot);
    io::PlayerInput* const input = bot->playerInput();
    IPIControlInfo* const controlInfo = equipment->getControlInfo();
    const bool focusing = move->isFocusing();
    setSoldierThrottle(bot, input, controlInfo, mobile->getMaxSpeed(), focusing);
    if (static_cast<int>(equipment->getSoldierPose()) != 4)
        bot->requestSoldierPose(1, true);
    if (move->slot20())
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->suicide(bot->getPlayer());
    return true;
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

// PE 10132BE0. Asks for the selector's pose, or for crouching (pose 1) while one of the mapped
// inputs 0, 3 or 13 holds a value (NaN counts). Done and failed once the equipment is in the pose;
// otherwise done, requesting the pose unless the soldier is in pose 4.
bool EntrySoldierPose::execute(BotScript* script, Bot* bot, bool& finished, bool& failed)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return false;
    BAPSoldierPose* const statement = static_cast<BAPSoldierPose*>(script);
    int pose = statement->getPose();
    const io::PlayerInput* const input = bot->playerInput();
    const unsigned long long mapped = input->mappingFlags;
    if (((mapped & 8) && !(input->inputs[3] == 0.0f)) || ((mapped & 1) && !(input->inputs[0] == 0.0f))
        || ((mapped & 0x2000) && !(input->inputs[13] == 0.0f)))
        pose = 1;
    if (static_cast<int>(equipment->getSoldierPose()) == pose)
    {
        finished = true;
        failed = true;
        return true;
    }
    const int current = static_cast<int>(equipment->getSoldierPose());
    if (current != 4 && current != pose)
        bot->requestSoldierPose(pose, statement->m_flag58);
    finished = true;
    failed = false;
    return true;
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
