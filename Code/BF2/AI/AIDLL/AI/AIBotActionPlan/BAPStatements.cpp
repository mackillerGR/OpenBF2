#include "../AIBot/BotNavigation.h"
#include "../AIBot/SquadManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/IAIPathfinding.h"
#include "BAPStatements.h"
#include <cfloat>
#include <algorithm>

#include "BAPConditions.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../AIBot/BotSubObjects.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/IPIArmament.h"
#include "LookController.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/BotSenses.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../Main/AIRandom.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Game/Objects/IObject.h"
#include "../../../../Game/Objects/ObjectLayoutAccess.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Physics/IPhysicsNode.h"
#include <cmath>
#include <cstring>

namespace dice {
namespace hfe {
namespace ai {

// ---- BAPStatementList ----------------------------------------------------------------------

BAPStatementList::BAPStatementList()
    : m_cursor()
    , m_failOnlyIfAllFail(false)
{
}

BAPStatementList::~BAPStatementList()
{
}

// ---- BAPParallel ---------------------------------------------------------------------------

BAPParallel::BAPParallel(int id, bool failOnlyIfAllFail, int limit, bool neverFail)
    : m_limit(limit)
    , m_neverFail(neverFail)
{
    m_cursor = m_children.end();
    m_failOnlyIfAllFail = failOnlyIfAllFail;
    m_inputMask = 0;
    m_id = id;
    // The freshly built list is cleared once more, as shipped.
    m_children.clear();
}

BAPParallel::BAPParallel(int id, int limit, bool neverFail)
    : m_limit(limit)
    , m_neverFail(neverFail)
{
    m_cursor = m_children.end();
    m_failOnlyIfAllFail = true;
    m_inputMask = 0;
    m_id = id;
    m_children.clear();
}

BAPParallel::~BAPParallel()
{
    for (std::list<BotScript*>::iterator it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// Every child runs each tick. A child's finished/failed locals carry over to the next child, and
// each child's input mask is read and dropped, as shipped.
bool BAPParallel::execute(Bot* bot, bool& finished, bool& failed)
{
    failed = m_failOnlyIfAllFail;
    bool childFailed = false;
    bool childFinished = true;
    finished = true;
    PlanInterpreter* const interpreter = static_cast<BotMain*>(bot)->getPlanInterpreter();
    const std::list<bool>::iterator flag = m_childFailed.begin();
    for (m_cursor = m_children.begin(); m_cursor != m_children.end(); ++m_cursor)
    {
        (*m_cursor)->getInputMask();
        interpreter->execute(*m_cursor, bot, childFinished, childFailed);
        finished = finished && childFinished;
        if (m_failOnlyIfAllFail)
            failed = failed && childFailed;
        else
            failed = failed || childFailed;
        *flag = childFailed;
    }
    if (m_neverFail)
        failed = false;
    return true;
}

BAPStatementType BAPParallel::getType() { return BAPSTParallel; }
bool BAPParallel::isComposite() { return true; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPParallel::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPParallel::printDebug");
}

void BAPParallel::addStatement(BotScript* statement)
{
    if (!statement)
        return;
    statement->m_owner = this;
    m_children.push_back(statement);
    m_inputMask |= statement->getInputMask();
    m_childFailed.push_back(false);
}

// ---- BAPSerial -----------------------------------------------------------------------------

BAPSerial::BAPSerial(int id, int limit)
    : m_limit(limit)
{
    m_cursor = m_children.end();
    m_id = id;
    m_failOnlyIfAllFail = true;
    // The freshly built list is cleared once more, as shipped.
    m_children.clear();
    m_cursor = m_children.begin();
}

BAPSerial::~BAPSerial()
{
    for (std::list<BotScript*>::iterator it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

bool BAPSerial::execute(Bot* bot, bool& finished, bool& failed)
{
    if (m_cursor == m_children.end())
    {
        finished = true;
        failed = true;
        m_cursor = m_children.begin();
        return true;
    }
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(*m_cursor, bot, finished, failed);
    if (m_failOnlyIfAllFail)
    {
        if (!failed)
            return true;
        ++m_cursor;
        if (m_cursor != m_children.end())
        {
            failed = false;
            return true;
        }
        failed = true;
        m_cursor = m_children.begin();
        return true;
    }
    ++m_cursor;
    if (failed || m_cursor == m_children.end())
    {
        failed = true;
        m_cursor = m_children.begin();
    }
    return true;
}

// The cursor is dereferenced without an end check, as shipped.
unsigned long long BAPSerial::getInputMask()
{
    return (*m_cursor)->getInputMask();
}

BAPStatementType BAPSerial::getType() { return BAPSTSerial; }
bool BAPSerial::isComposite() { return true; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPSerial::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPSerial::printDebug");
}

// The child's input mask is not merged, and the cursor returns to the first child.
void BAPSerial::addStatement(BotScript* statement)
{
    if (!statement)
        return;
    statement->m_owner = this;
    m_children.push_back(statement);
    m_cursor = m_children.begin();
}

// ---- BAPWait -------------------------------------------------------------------------------

BAPWait::BAPWait(BAPCondition* condition, int id)
    : m_condition(condition)
{
    m_id = id;
    condition->m_owner = this;
}

BAPWait::~BAPWait()
{
    if (m_condition)
        delete m_condition;
}

bool BAPWait::execute(Bot* bot, bool& finished, bool& failed)
{
    failed = m_condition->evaluate(bot);
    finished = true;
    return true;
}

unsigned long long BAPWait::getInputMask() { return 0; }
BAPStatementType BAPWait::getType() { return BAPSTWait; }
bool BAPWait::isComposite() { return true; }   // PE 101AD2C0

// ---- BAPICChangeVehicle --------------------------------------------------------------------

BAPICChangeVehicle::BAPICChangeVehicle(unsigned int target)
    : m_target(target)
{
}

BAPICChangeVehicle::~BAPICChangeVehicle() {}

bool BAPICChangeVehicle::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->setChangeTarget(m_target);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPICChangeVehicle::getType() { return BAPSTICChangeVehicle; }
bool BAPICChangeVehicle::isComposite() { return true; }

// ---- BAPSoldierPoseSelector ----------------------------------------------------------------

namespace {

// Whether the object is a soldier a human controls that is not standing.
bool isLowHumanSoldier(Information* object)
{
    if (!object || !((object->getObjectTypeFlags() >> 21) & 1))
        return false;
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(object->getPlayerId());
    return player && player->getSoldier() && object->getSoldierPose() != SPStanding;
}

} // anonymous namespace

BAPSoldierPoseSelector::BAPSoldierPoseSelector(int defaultPose, int abovePose, float threshold, int belowPose,
                                               float limit, unsigned int companion, float interval)
    : m_defaultPose(defaultPose), m_abovePose(abovePose), m_belowPose(belowPose), m_currentPose(defaultPose),
      m_interval(interval), m_lastTime(-3.4028234663852886e+38), m_companion(companion)
{
    m_threshold = (static_cast<float>(AIRandom::m_instance->next()) * 1.1641532182693481e-09f + 1.0f) * threshold;
    if (-1.0f > limit)
        m_limit = -1.0f;
    else if (1.0f < limit)
        m_limit = 1.0f;
    else
        m_limit = limit;
}

BAPSoldierPoseSelector::~BAPSoldierPoseSelector() {}
void BAPSoldierPoseSelector::setDefaultPose(int pose) { m_defaultPose = pose; }
void BAPSoldierPoseSelector::setAbovePose(int pose) { m_abovePose = pose; }
void BAPSoldierPoseSelector::setThreshold(float threshold) { m_threshold = threshold; }
void BAPSoldierPoseSelector::setBelowPose(int pose) { m_belowPose = pose; }
void BAPSoldierPoseSelector::setLimit(float limit) { m_limit = limit; }
void BAPSoldierPoseSelector::setInterval(float interval) { m_interval = interval; }

int BAPSoldierPoseSelector::getPose()
{
    if (m_companion != static_cast<unsigned int>(-1)
        && isLowHumanSoldier(informationManager->getInformation(m_companion)))
        return SPCrouching;
    return m_defaultPose;
}

int BAPSoldierPoseSelector::getAbovePose() { return m_abovePose; }
float BAPSoldierPoseSelector::getThreshold() { return m_threshold; }
int BAPSoldierPoseSelector::getBelowPose() { return m_belowPose; }
float BAPSoldierPoseSelector::getLimit() { return m_limit; }
float BAPSoldierPoseSelector::getInterval() { return m_interval; }
int BAPSoldierPoseSelector::getCurrentPose() { return m_currentPose; }

int BAPSoldierPoseSelector::update(float measure, float value, double time, Bot* bot, const Vec3& position)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return m_currentPose;
    if (equipment->getSoldierPose() == SPSwimming)
    {
        m_currentPose = SPSwimming;
        return m_currentPose;
    }
    if (!(m_interval <= time - m_lastTime))
        return m_currentPose;
    m_lastTime = time;
    if (m_companion != static_cast<unsigned int>(-1)
        && isLowHumanSoldier(informationManager->getInformation(m_companion)))
    {
        m_currentPose = SPCrouching;
        return m_currentPose;
    }
    int pose;
    if (measure > m_threshold)
        pose = m_abovePose;
    else if (value < m_limit)
        pose = m_belowPose;
    else
        pose = m_defaultPose;
    // Only kit types 1 and 3 lie down.
    if (pose == SPLying && equipment->getKitType() != 1 && equipment->getKitType() != 3)
        pose = SPCrouching;
    m_currentPose = pose;

    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const float water = terrain->getWaterLevel(position.x, position.z);
    const float depth = water - terrain->getHeight(position.x, position.z);
    if (depth > 0.4f)
    {
        m_currentPose = SPStanding;
        return m_currentPose;
    }
    if (depth > 0.0f && m_currentPose == SPLying)
        m_currentPose = SPCrouching;
    return m_currentPose;
}

// ---- BAPAMove ------------------------------------------------------------------------------

// The pose selector: standing by default, lying above a threshold scaled by a random factor in
// [1, 15), crouching below the limit 0, no companion, a ten-second interval.
BAPAMove::BAPAMove(Bot* bot)
    : BAPSoldierPoseSelector(SPStanding, SPLying,
                             static_cast<float>(AIRandom::m_instance->next()) * 3.259629011154175e-09f + 1.0f,
                             SPCrouching, 0.0f, static_cast<unsigned int>(-1), 10.0f)
    , m_focusing(false), m_until(0), m_int70(0), m_float74(0.0f), m_int78(0), m_int84(0), m_int88(0),
      m_avoidObject(static_cast<unsigned int>(-1)), m_nextAvoidTime(-1000.0)
{
    m_avoidPoint.x = m_avoidPoint.y = 0.0f;
    m_avoidCentre.x = m_avoidCentre.y = 0.0f;
    m_cachedSteer.x = m_cachedSteer.y = 0.0f;
    m_target.x = m_target.y = m_target.z = 0.0f;
    m_steerPoint.x = m_steerPoint.y = 0.0f;
    m_float74 = 1.0f;
    m_int78 = 0;
    static_cast<BotMain*>(bot)->setMoveToStatus(0);
}

BAPAMove::~BAPAMove()
{
    if (m_until)
        delete m_until;
}

unsigned long long BAPAMove::getInputMask() { return m_inputMask; }
BAPStatementType BAPAMove::getType() { return BAPSTMoveTo; }
bool BAPAMove::isComposite() { return false; }
bool BAPAMove::isFocusing() { return m_focusing; }
bool BAPAMove::slot8() { return false; }
const Vec2& BAPAMove::getSteerPoint() { return m_steerPoint; }
bool BAPAMove::getSteering(Vec2&, bool&, bool&, int) { return false; }

bool BAPAMove::isFinished(Bot* bot)
{
    return m_until && m_until->evaluate(bot);
}

bool BAPAMove::getPathPoint(Vec2&) { return false; }
bool BAPAMove::getPathTurn(float&) { return false; }
float BAPAMove::slot14() { return 0.0f; }
unsigned int BAPAMove::getTargetObject() { return static_cast<unsigned int>(-1); }

float BAPAMove::getDistance(Bot* bot)
{
    const Vec3& position = informationManager->getInformation(bot->getCurrentEquipment())->getPosition();
    const float dx = position.x - m_target.x;
    const float dy = position.y - m_target.y;
    const float dz = position.z - m_target.z;
    return std::sqrt(dx * dx + dz * dz + dy * dy);
}

float BAPAMove::getDistanceSq(Bot* bot)
{
    const Vec3& position = informationManager->getInformation(bot->getCurrentEquipment())->getPosition();
    const float dx = position.x - m_target.x;
    const float dy = position.y - m_target.y;
    const float dz = position.z - m_target.z;
    return dx * dx + dz * dz + dy * dy;
}

bool BAPAMove::slot20() { return false; }

void BAPAMove::setFocus(Bot* bot, bool focus, const Vec3& position, unsigned int object)
{
    m_focusing = focus;
    if (!focus)
        return;
    if (object == static_cast<unsigned int>(-1))
        bot->getSensingAgent()->setFocusPoint(position);
    else
        bot->getSensingAgent()->setFocusObject(object);
}

namespace {

// Component-wise equality within FLT_EPSILON. PE 100D2910.
bool isSamePosition3(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= 1.1920929e-07f && std::fabs(a.y - b.y) <= 1.1920929e-07f
        && std::fabs(a.z - b.z) <= 1.1920929e-07f;
}

// Whether two flat points differ by more than FLT_EPSILON in summed absolute components.
// PE 1019B040.
bool differs(const Vec2& a, const Vec2& b)
{
    return !(std::fabs(a.y - b.y) + std::fabs(a.x - b.x) <= 1.1920929e-07f);
}

// The conditional Vec2 normaliser: unit vectors are left alone, near-zero ones are zeroed.
// PE 100E9DE0.
void normaliseFlat2(Vec2& value)
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

} // anonymous namespace

// The cached answer is reused while the game time has not advanced past the last computation.
// The equipment is not null-checked.
bool BAPAMove::getAvoidanceSteering(Bot* bot, Vec2 steer, Vec2& out, Information* ignore, bool allowIgnored)
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now < m_nextAvoidTime)
    {
        out = m_cachedSteer;
        return m_cachedAvoidResult;
    }
    m_nextAvoidTime = now;
    m_cachedAvoidResult = true;

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Vec2 flat;
    flat.x = equipment->getPosition().x;
    flat.y = equipment->getPosition().z;

    if (slot18()
        && (m_avoidObject == static_cast<unsigned int>(-1)
            || hasAvoidedObstacleMoved(m_avoidPoint, m_avoidObject, m_avoidRadius)))
    {
        Vec2 beyond;
        beyond.x = beyond.y = 0.0f;
        bot->getBotNavigation()->getFirstPointBeyondDist(flat, 10.0f, beyond);
        if (findObstacleToAvoid(bot, beyond, m_target, getTargetObject(), m_avoidPoint, m_avoidObject,
                                m_avoidOffset, m_avoidCentre, m_avoidRadius, m_avoidingTarget, ignore,
                                allowIgnored))
            return true;
    }

    out.x = steer.x - flat.x;
    out.y = steer.y - flat.y;
    bool passing = false;
    float distance = std::sqrt(out.x * out.x + out.y * out.y);

    Information* const obstacle = informationManager->getInformation(m_avoidObject);
    if (!obstacle)
    {
        m_avoidObject = static_cast<unsigned int>(-1);
    }
    else
    {
        Vec2 toCentre;
        toCentre.x = m_avoidCentre.x - flat.x;
        toCentre.y = m_avoidCentre.y - flat.y;
        const float fx = flat.x - m_avoidCentre.x;
        const float fy = flat.y - m_avoidCentre.y;
        const float centreDistanceSq = fx * fx + fy * fy;
        const float reach = m_avoidRadius + 1.0f;
        if (!(centreDistanceSq <= reach * reach))
            m_avoidObject = static_cast<unsigned int>(-1);

        bool isTarget = false;
        if (m_avoidObject != static_cast<unsigned int>(-1))
        {
            unsigned int change = bot->getChangeTarget();
            Information* const changeInfo = informationManager->getInformation(change);
            if (changeInfo && !changeInfo->isPrimary())
                change = changeInfo->getPrimaryObject(bot->getSide());
            unsigned int repair = bot->getRepairTarget();
            Information* const repairInfo = informationManager->getInformation(repair);
            if (repairInfo && !repairInfo->isPrimary())
                repair = repairInfo->getPrimaryObject(bot->getSide());
            isTarget = m_avoidObject == repair || m_avoidObject == change;
        }

        passing = isTarget;
        if (m_avoidObject != static_cast<unsigned int>(-1))
        {
            if (!isTarget && !m_avoidingTarget)
            {
                if (toCentre.y * out.y + toCentre.x * out.x < 0.0f)
                    passing = true;
                else if (!ignore)
                    passing = false;
                else
                    passing = !(toCentre.x * toCentre.x + toCentre.y * toCentre.y < distance * distance);
            }

            const Vec3& objectPosition = obstacle->getPosition();
            Vec2 objectFlat;
            objectFlat.x = objectPosition.x;
            objectFlat.y = objectPosition.z;
            const float mx = m_avoidCentre.x - objectFlat.x;
            const float my = m_avoidCentre.y - objectFlat.y;
            if (!(mx * mx + my * my < 0.25f))
            {
                m_avoidObject = static_cast<unsigned int>(-1);
            }
            else
            {
                const float tx = m_target.x - flat.x;
                const float ty = m_target.z - flat.y;
                const float ox = objectFlat.x - m_target.x;
                const float oy = objectFlat.y - m_target.z;
                const float targetToObject = std::sqrt(ox * ox + oy * oy);
                const float threshold = m_avoidRadius - targetToObject + 0.8f;
                const bool targetInside = !(threshold * threshold <= tx * tx + ty * ty);

                bool keep = false;
                if (m_avoidingTarget && targetToObject <= m_avoidRadius)
                {
                    if (targetInside)
                        keep = true;
                    else if (m_avoidOffset < m_avoidRadius && centreDistanceSq <= reach * reach)
                        keep = true;
                }
                if (!keep && !(passing && !isTarget) && targetToObject <= m_avoidRadius && targetInside)
                    keep = true;
                if (keep)
                {
                    m_cachedSteer = out;
                    return true;
                }
                out.x = m_avoidPoint.x - flat.x;
                out.y = m_avoidPoint.y - flat.y;
                distance = std::sqrt(out.x * out.x + out.y * out.y);
            }
        }
    }

    if (isFocusing())
    {
        Vec2 zero;
        zero.x = zero.y = 0.0f;
        if (differs(steer, zero))
        {
            m_focusing = true;
            Vec3 point;
            point.x = steer.x;
            point.y = 0.0f;
            point.z = steer.y;
            bot->getSensingAgent()->setFocusPoint(point);
        }
    }

    if (m_avoidObject != static_cast<unsigned int>(-1) && distance < 0.3f)
        m_avoidObject = static_cast<unsigned int>(-1);

    if (m_avoidObject != static_cast<unsigned int>(-1) && !passing && distance > 0.5f)
    {
        Vec2 toPoint;
        toPoint.x = m_avoidPoint.x - flat.x;
        toPoint.y = m_avoidPoint.y - flat.y;
        normaliseFlat2(toPoint);
        Vec2 toCentre;
        toCentre.x = m_avoidCentre.x - flat.x;
        toCentre.y = m_avoidCentre.y - flat.y;
        normaliseFlat2(toCentre);
        if (toCentre.y * toPoint.y + toCentre.x * toPoint.x > 0.05f)
        {
            Vec2 away;
            away.x = toPoint.x + flat.x - m_avoidCentre.x;
            away.y = toPoint.y + flat.y - m_avoidCentre.y;
            normaliseFlat2(away);
            out.x = away.x * m_avoidOffset + m_avoidCentre.x - flat.x;
            out.y = away.y * m_avoidOffset + m_avoidCentre.y - flat.y;
            normaliseFlat2(out);
        }
        else
        {
            const float scale = 1.0f / distance;
            out.x *= scale;
            out.y *= scale;
        }
    }
    else if (distance > 0.001f)
    {
        const float scale = 1.0f / distance;
        out.x *= scale;
        out.y *= scale;
    }
    else
    {
        out.x = 1.0f;
        out.y = 0.0f;
    }

    m_cachedSteer = out;
    m_cachedAvoidResult = false;
    return false;
}

// ---- BAPMoveToFinding ----------------------------------------------------------------------


BAPMoveToFinding::BAPMoveToFinding(Bot* bot, const Vec3& target, float float60, float float64,
                                   BAPCondition* until, unsigned long long inputMask, int id, float maxRange,
                                   float minRange)
    : BAPAMove(bot), m_flagE8(false), m_flagE9(false), m_intEC(1), m_bot(bot), m_maxRange(maxRange),
      m_minRange(minRange), m_maxRangeSq(maxRange * maxRange), m_minRangeSq(minRange * minRange),
      m_arrived(false), m_pathRequested(false)
{
    for (int i = 0; i < 8; ++i)
        m_zeroC8[i] = 0;
    m_freeSpot.x = m_freeSpot.y = 0.0f;
    bot->getCurrentEquipment();
    m_id = id;
    m_target = target;
    m_float64 = float64;
    m_float60 = float60;
    m_until = until;
    if (until)
        until->m_owner = this;
    m_int70 = 0;
    m_pathRequested = false;
    m_flag6C = true;
    m_inputMask = inputMask;
    m_freeSpot.x = m_freeSpot.y = 0.0f;
}

BAPMoveToFinding::~BAPMoveToFinding()
{
}

bool BAPMoveToFinding::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPMoveToFinding::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPMoveToFinding::printDebug");
}

bool BAPMoveToFinding::isFinished(Bot* bot)
{
    if (!(m_until && m_until->evaluate(bot)) && !isWithinRange())
        return false;
    bot->getBotNavigation()->clearProgress();
    return true;
}

bool BAPMoveToFinding::getPathPoint(Vec2& point)
{
    return m_bot->getBotNavigation()->getSecondPoint(point);
}

bool BAPMoveToFinding::getPathTurn(float& turn)
{
    return m_bot->getBotNavigation()->getLargestAngleDiff(turn);
}

float BAPMoveToFinding::slot14() { return m_bot->getBotNavigation()->getDistanceLeft(); }
bool BAPMoveToFinding::slot18() { return true; }

bool BAPMoveToFinding::isWithinRange()
{
    const float distanceSq = getDistanceSq(m_bot);
    return distanceSq <= m_maxRangeSq && !(distanceSq < m_minRangeSq);
}

bool BAPMoveToFinding::slot20() { return m_bot->getBotNavigation()->isPathSearchPending(); }

// The equipment is not null-checked. When close to the target the free spot is searched only
// while it is already set; an unset (zero) spot is steered to as it is, as shipped. A ground
// position inside an artillery zone the equipment is not already in fails the move.
bool BAPMoveToFinding::getSteering(Vec2& point, bool& arrived, bool& failed, int)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (static_cast<BotMain*>(m_bot)->getShouldWaitForRepairs()
        || (equipment->getPhysical()->isInAir() && !equipment->getPhysical()->isInWater()))
    {
        arrived = false;
        failed = true;
        return true;
    }

    BotNavigation* navigation = m_bot->getBotNavigation();
    if (!m_pathRequested || !isSamePosition3(m_target, navigation->getGoal())
        || !m_bot->getBotNavigation()->isSearchActive())
    {
        m_pathRequested = true;
        m_flag6C = m_bot->getBotNavigation()->initPathfinding(m_target, 0.0f, m_maxRange);
    }

    bool steering = false;
    if (m_flag6C && m_bot->getBotNavigation()->updatePath(m_steerPoint, m_arrived, failed, 0.0f))
    {
        point = m_steerPoint;
        arrived = m_arrived;
        steering = true;
    }

    bool useFreeSpot = false;
    if (getDistance(m_bot) < m_minRange + 1.0f && m_minRange > 1.0f)
    {
        Vec2 zero;
        zero.x = zero.y = 0.0f;
        useFreeSpot = true;
        if (differs(m_freeSpot, zero))
        {
            IPIMobile* const mobile = equipment->getMobile();
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            const int equipmentType = equipment->getUnit()->getEquipmentType();
            bool found = false;
            for (float angle = 0.0f; !found && angle < 6.2831855f; angle += 0.5f)
            {
                const float radius = m_minRange + 1.0f;
                Vec3 candidate;
                candidate.x = radius * std::sin(angle) + m_target.x;
                candidate.y = radius * 0.0f + m_target.y;
                candidate.z = radius * std::cos(angle) + m_target.z;
                found = pathfinding->isValidPosition(mobile->getVehicleNumber(), equipmentType, candidate, true);
                if (found)
                {
                    m_freeSpot.x = candidate.x;
                    m_freeSpot.y = candidate.z;
                }
            }
            if (!found)
                return false;
        }
        point = m_freeSpot;
    }
    else if (!failed && isFinished(m_bot))
    {
        m_flag6C = true;
        arrived = true;
        m_arrived = true;
        point = m_steerPoint;
        steering = true;
    }

    if (!failed)
    {
        (void)useFreeSpot;
        const Vec3& position = equipment->getPosition();
        Vec3 ground;
        ground.x = point.x;
        ground.y = position.y;
        ground.z = point.y;
        SquadManager* const squads = SquadManager::m_instance;
        double elapsed;
        int zoneSide;
        bool flee = false;
        if (!((equipment->getObjectTypeFlags() >> 4) & 1))
        {
            if (squads->isInArtilleryDangerZone(ground, m_bot->getSide(), elapsed, zoneSide)
                && !squads->isInArtilleryDangerZone(equipment->getPosition(), m_bot->getSide(), elapsed, zoneSide))
            {
                failed = true;
                flee = true;
            }
        }
        if (!flee && ((equipment->getObjectTypeFlags() >> 21) & 1)
            && squads->isInArtilleryDangerZone(equipment->getPosition(), m_bot->getSide(), elapsed, zoneSide))
        {
            SquadMember* const member = m_bot->getSquadMember();
            if (member && member->isFleeingArtillery())
                setFocus(m_bot, true, ground, static_cast<unsigned int>(-1));
        }
    }

    if (!steering)
        m_pathRequested = false;
    return steering;
}

// ---- BAPSoldierPose ------------------------------------------------------------------------

BAPSoldierPose::BAPSoldierPose(int pose, BAPCondition* until, int id, bool flag)
    : BAPSoldierPoseSelector(pose, pose, 2.0f, pose, 0.0f, static_cast<unsigned int>(-1), 5.0f)
{
    m_until = until;
    m_condition = 0;
    m_flag58 = flag;
    until->m_owner = this;   // not null-checked, as shipped
    m_inputMask = 0x6000000000ULL;
    m_id = id;
}

BAPSoldierPose::BAPSoldierPose(int pose, BAPCondition* until, int id, unsigned int companion, bool flag)
    : BAPSoldierPoseSelector(pose, pose, 2.0f, pose, 0.0f, companion, 5.0f)
{
    m_until = until;
    m_condition = 0;
    m_flag58 = flag;
    until->m_owner = this;   // not null-checked, as shipped
    m_inputMask = 0x6000000000ULL;
    m_id = id;
}

BAPSoldierPose::BAPSoldierPose(int defaultPose, int abovePose, int threshold, int belowPose, float limit,
                               BAPCondition* until, BAPCondition* condition, int id, bool flag)
    : BAPSoldierPoseSelector(defaultPose, abovePose, static_cast<float>(threshold), belowPose, limit,
                             static_cast<unsigned int>(-1), 5.0f)
{
    m_until = until;
    m_condition = condition;
    m_flag58 = flag;
    if (m_until)
        m_until->m_owner = this;
    if (m_condition)
        m_condition->m_owner = this;
    m_id = id;
    m_inputMask = 0x6000000000ULL;
}

BAPSoldierPose::~BAPSoldierPose()
{
    if (m_until)
        delete m_until;
    if (m_condition)
        delete m_condition;
}

bool BAPSoldierPose::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPSoldierPose::getType() { return BAPSTSoldierPose; }

bool BAPSoldierPose::isComposite() { return false; }

// ---- BAPAMoveToObject ----------------------------------------------------------------------

BAPAMoveToObject::BAPAMoveToObject(Bot* bot, unsigned int target, float range, float float60,
                                   float float64, BAPCondition* until, unsigned long long inputMask, int id)
    : BAPAMove(bot)
{
    m_float60 = float60;
    m_rangeSq = range * range;
    m_targetObject = target;
    m_inputMask = inputMask;
    m_range = range;
    m_float64 = float64;
    m_id = id;
    m_until = until;
    if (until)
        until->m_owner = this;
}

BAPAMoveToObject::~BAPAMoveToObject()
{
}

bool BAPAMoveToObject::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPAMoveToObject::getType() { return BAPSTMoveToObject; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPAMoveToObject::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPAMoveToObject::printDebug");
}

bool BAPAMoveToObject::isFinished(Bot* bot)
{
    if (!(m_until && m_until->evaluate(bot)) && !(getDistanceSq(bot) < m_rangeSq))
        return false;
    bot->getBotNavigation()->clearProgress();
    return true;
}

unsigned int BAPAMoveToObject::getTargetObject() { return m_targetObject; }

float BAPAMoveToObject::getDistance(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_targetObject);
    const Vec3& from = equipment->getPosition();
    const Vec3& to = target->getPosition();
    const float dx = from.x - to.x;
    const float dy = from.y - to.y;
    const float dz = from.z - to.z;
    return std::sqrt(dx * dx + dz * dz + dy * dy);
}

float BAPAMoveToObject::getDistanceSq(Bot* bot)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_targetObject);
    const Vec3& from = equipment->getPosition();
    const Vec3& to = target->getPosition();
    const float dx = from.x - to.x;
    const float dy = from.y - to.y;
    const float dz = from.z - to.z;
    return dx * dx + dz * dz + dy * dy;
}

// ---- BAPMoveToObjectFinding ----------------------------------------------------------------

BAPMoveToObjectFinding::BAPMoveToObjectFinding(Bot* bot, unsigned int target, float range, float float60,
                                               float float64, BAPCondition* until,
                                               unsigned long long inputMask, int id,
                                               BAPCondition* condition, float minRange, int int120)
    : BAPAMoveToObject(bot, target, range, float60, float64, until, inputMask, id)
{
    for (int i = 0; i < 8; ++i)
        m_zeroE0[i] = 0;
    m_minRangeSq = minRange * minRange;
    m_minRange = minRange;
    m_time128 = -1.0;
    m_int120 = int120;
    m_flag101 = false;
    m_int104 = 1;
    m_bot = bot;
    m_condition = condition;
    m_flag110 = true;
    m_flag11C = false;
    m_flag11D = false;
    m_flag11E = false;
    if (minRange > m_range)
    {
        m_minRange = m_range;
        m_minRangeSq = m_rangeSq;
    }
    if (condition)
        condition->m_owner = this;
    m_flag11F = false;
    m_active = true;
}

BAPMoveToObjectFinding::~BAPMoveToObjectFinding()
{
    if (m_condition)
        delete m_condition;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPMoveToObjectFinding::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPMoveToObjectFinding::printDebug");
}

bool BAPMoveToObjectFinding::slot8() { return m_flag11D; }

namespace {

float lengthSquared(const Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// PE 1006EEC0; defined with the look-at wrapper below.
int normaliseVector(Vec3& value);

} // anonymous namespace

// PE 101A6A80. The fourth argument is handed on bit for bit as updatePath's distance.
bool BAPMoveToObjectFinding::getSteering(Vec2& point, bool& arrived, bool& failed, int arg)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment || !m_active)
    {
        m_active = false;
        return false;
    }
    IPIPhysical* const physical = equipment->getPhysical();
    if (static_cast<BotMain*>(m_bot)->getShouldWaitForRepairs()
        || (physical->isInAir() && !physical->isInWater()))
    {
        arrived = false;
        failed = true;
        return true;
    }
    Information* const target = informationManager->getInformation(m_targetObject);
    if (!target)
    {
        m_active = false;
        return false;
    }
    IPIMobile* const mobile = equipment->getMobile();   // not null-checked, as shipped
    const IPIMobile::ValidPos& ownValid = mobile->getLastValidPosition(mobile->getVehicleNumber());
    Information* const primary =
        informationManager->getInformation(target->getPrimaryObject(equipment->getSide()));
    IPIMobile* const primaryMobile = primary ? primary->getMobile() : 0;
    if (!primary || !primaryMobile)
    {
        m_active = false;
        return false;
    }
    const IPIMobile::ValidPos& targetValid = primaryMobile->getLastValidPosition(mobile->getVehicleNumber());
    if (ownValid.polygon == static_cast<unsigned int>(-1))
    {
        m_bot->getBotNavigation()->setPathSearchPending(true);
        m_active = false;
        return false;
    }
    if (targetValid.polygon == static_cast<unsigned int>(-1))
    {
        m_active = false;
        return false;
    }

    const Vec3& targetPosition = targetValid.position;
    const Vec3& ownPosition = ownValid.position;
    Vec3 offset;
    offset.x = targetPosition.x - ownPosition.x;
    offset.y = targetPosition.y - ownPosition.y;
    offset.z = targetPosition.z - ownPosition.z;
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);

    const SensedObject* const sensed =
        static_cast<const SensedObject*>(m_bot->getSensedObjectFromHandle(m_targetObject));
    bool seen = true;
    if (sensed)
    {
        seen = (sensed->m_senseFlags >> 3) & 1;
        if (!seen)
            m_flag11F = false;
    }
    const bool inRange = isWithinRange();
    const bool wasInBand = m_flag11C;
    const bool stale = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime()
                       - m_time128 > 1.0;

    // Decide whether to choose a new goal.
    bool choose = false;
    if (seen && !isSamePosition3(m_target, targetPosition) && !m_flag11F && (!m_flag11D || wasInBand))
    {
        m_flag11F = true;
        choose = true;
    }
    else if (!isSamePosition3(m_target, m_bot->getBotNavigation()->getGoal()))
    {
        choose = true;
    }
    else if (stale)
    {
        bool checkBand;
        if (!m_flag11D)
        {
            checkBand = !(distance < m_minRange);
            choose = !checkBand;
        }
        else
        {
            const float range = m_minRange;
            const Vec3 fromGoal = {targetPosition.x - m_target.x, targetPosition.y - m_target.y,
                                   targetPosition.z - m_target.z};
            checkBand = !(range * range > lengthSquared(fromGoal));
            if (!checkBand)
            {
                choose = wasInBand;
            }
        }
        if (checkBand && wasInBand && !inRange && !isSamePosition3(m_target, targetPosition))
            choose = true;
    }

    if (choose)
    {
        chooseGoal(distance, &offset, &ownPosition, targetValid.quadTree, targetValid.polygon,
                   &targetPosition, false);
    }
    else if (m_flag11E && m_bot->getBotNavigation()->isSearchActive())
    {
        if (stale && m_condition && m_condition->evaluate(m_bot))
        {
            if (!m_flag110 || m_flag11C)
            {
                m_flag110 = true;
                chooseGoal(distance, &offset, &ownPosition, targetValid.quadTree, targetValid.polygon,
                           &targetPosition, false);
            }
        }
        else
        {
            m_flag110 = false;
        }
    }
    else
    {
        Vec3 unused;
        Vec3 zero = {0.0f, 0.0f, 0.0f};
        chooseGoal(0.0f, &unused, &zero, targetValid.quadTree, targetValid.polygon, &targetPosition, true);
    }

    const bool active = m_active;
    m_flag11E = true;
    if (!active)
        return false;
    if (!m_flag11D)
        m_bot->getBotNavigation()->updateLastPoint(primary);

    float pathDistance;
    std::memcpy(&pathDistance, &arg, sizeof(pathDistance));
    const bool steering = m_bot->getBotNavigation()->updatePath(m_steerPoint, m_flag11C, failed, pathDistance);
    const Vec3& position = equipment->getPosition();
    Vec3 ground;
    ground.x = m_steerPoint.x;
    ground.y = position.y;
    ground.z = m_steerPoint.y;
    if (steering)
    {
        point = m_steerPoint;
        arrived = m_flag11C;
        if (!((equipment->getObjectTypeFlags() >> 4) & 1))
        {
            double elapsed;
            int zoneSide;
            SquadManager* const squads = SquadManager::m_instance;
            if (squads->isInArtilleryDangerZone(ground, m_bot->getSide(), elapsed, zoneSide)
                && !squads->isInArtilleryDangerZone(equipment->getPosition(), m_bot->getSide(), elapsed,
                                                    zoneSide))
            {
                failed = true;
                return steering;
            }
        }
    }
    if (failed)
    {
        if (!steering)
            m_flag11E = false;
        return steering;
    }

    if (!m_flag11D && ((equipment->getObjectTypeFlags() >> 21) & 1)
        && std::fabs(target->getPosition().y - equipment->getPosition().y) < 2.0f)
    {
        // Near the object a soldier with a usable weapon keeps looking at it.
        const float distanceLeft = m_bot->getBotNavigation()->getDistanceLeft();
        IPIPhysical* const targetPhysical = target->getPhysical();
        IPIArmament* const armament = equipment->getArmament();
        if (!armament || armament->getActiveWeapon() == -1)
        {
            m_active = false;
            return false;
        }
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
            armament->getWeapon(armament->getActiveWeapon()));
        if (!weapon)
        {
            m_active = false;
            return false;
        }
        Vec3 zero = {0.0f, 0.0f, 0.0f};
        if ((!m_bot->isFiring() || !(weapon->getMaxRange() > 5.0f) || weapon->getWeaponType() == 4)
            && targetPhysical->getRadius() + 25.0f > distanceLeft)
            setFocus(m_bot, true, zero, m_targetObject);
        else
            setFocus(m_bot, false, zero, static_cast<unsigned int>(-1));
    }
    else if ((equipment->getObjectTypeFlags() >> 21) & 1)
    {
        double elapsed;
        int zoneSide;
        if (SquadManager::m_instance->isInArtilleryDangerZone(equipment->getPosition(), m_bot->getSide(),
                                                              elapsed, zoneSide))
        {
            SquadMember* const member = m_bot->getSquadMember();
            if (member && member->isFleeingArtillery())
                setFocus(m_bot, true, ground, static_cast<unsigned int>(-1));
        }
    }

    if (!failed && isFinished(m_bot))
    {
        m_active = true;
        arrived = true;
        m_flag11C = true;
        point = m_steerPoint;
        return true;
    }
    if (!steering)
        m_flag11E = false;
    return steering;
}

// PE 101A62A0.
void BAPMoveToObjectFinding::chooseGoal(float distance, const Vec3*, const Vec3* from, unsigned int island,
                                        unsigned int polygon, const Vec3* targetPosition, bool direct)
{
    m_flag11D = false;
    m_flag11C = false;
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!equipment)
    {
        m_active = false;
        return;
    }
    Information* const target = informationManager->getInformation(m_targetObject);
    if (!target)
    {
        m_active = false;
        return;
    }
    IPIMobile* targetMobile = target->getMobile();
    if (!targetMobile && !target->isPrimary())
    {
        Information* const primary =
            informationManager->getInformation(target->getPrimaryObject(m_bot->getSide()));
        if (primary)
            targetMobile = primary->getMobile();
    }
    IPIPhysical* const targetPhysical = target->getPhysical();
    float reach = FLT_MAX;
    if (targetPhysical)
        reach = targetPhysical->getRadius() * 1.1f;
    const int equipmentType = equipment->getUnit()->getEquipmentType();

    unsigned int goalIsland = island;
    unsigned int goalPolygon = polygon;
    if (distance < m_minRange && !direct)
    {
        Vec3 candidates[3];
        const Vec3& velocity = targetMobile->getSpeed();   // not null-checked, as shipped
        Vec3 flat = {velocity.x, 0.0f, velocity.z};
        const float speed = std::sqrt(flat.x * flat.x + flat.z * flat.z);
        const bool soldier = (equipment->getObjectTypeFlags() >> 21) & 1;
        const float length = std::min(10.0f, (m_range - m_minRange) * 0.2f) + m_minRange;
        Vec3& first = candidates[0];
        Vec3& second = candidates[1];
        Vec3& third = candidates[2];
        if (soldier && speed > 1.5f)
        {
            // Step aside of a running target, on the side the bot is already on.
            const float inverse = 1.0f / speed;
            flat.x *= inverse;
            flat.y *= inverse;
            flat.z *= inverse;
            Vec3 side = {flat.z, flat.y, -flat.x};
            const Vec3 away = {from->x - targetPosition->x, from->y - targetPosition->y,
                               from->z - targetPosition->z};
            const bool sameSide = !(side.z * away.z + side.y * away.y + side.x * away.x < 0.0f);
            if (!sameSide)
            {
                side.x = -side.x;
                side.y = -side.y;
                side.z = -side.z;
            }
            side.x *= length;
            side.y *= length;
            side.z *= length;
            first = side;
            second.x = -side.x;
            second.y = -side.y;
            second.z = -side.z;
            if (sameSide)
            {
                third.x = side.z;
                third.y = side.y;
                third.z = -side.x;
            }
            else
            {
                third.x = -side.z;
                third.y = -side.y;
                third.z = side.x;
            }
        }
        else
        {
            if (soldier)
            {
                first.x = from->x - targetPosition->x;
                first.y = from->y - targetPosition->y;
                first.z = from->z - targetPosition->z;
                normaliseVector(first);
            }
            else
            {
                const Mat4& transformation = equipment->getTransformation();
                first.x = transformation.m[8];
                first.y = transformation.m[9];
                first.z = transformation.m[10];
            }
            first.x *= length;
            first.y *= length;
            first.z *= length;
            second.x = first.z;
            second.y = first.y;
            second.z = -first.x;
            third.x = -first.z;
            third.y = -first.y;
            third.z = first.x;
        }
        m_flag11D = true;
        setFocus(m_bot, true, equipment->getPosition(), static_cast<unsigned int>(-1));

        BotNavigation* const navigation = m_bot->getBotNavigation();
        float farthest = 0.0f;
        Vec3 reached = {0.0f, 0.0f, 0.0f};
        Vec3 goal = {0.0f, 0.0f, 0.0f};
        unsigned int blockedPolygon = static_cast<unsigned int>(-1);
        unsigned int tracedPolygon;
        bool clear = false;
        for (int i = 0; i < 3 && !clear; ++i)
        {
            const Vec3 probe = {candidates[i].x + targetPosition->x, candidates[i].y + targetPosition->y,
                                candidates[i].z + targetPosition->z};
            clear = IAIPathfinding::instance->trace(navigation, false, probe, reached, tracedPolygon,
                                                    equipmentType, -1, 0.0f, 0, 0);
            if (!clear)
            {
                const float dx = targetPosition->x - reached.x;
                const float dy = targetPosition->y - reached.y;
                const float dz = targetPosition->z - reached.z;
                const float lengthSq = dz * dz + dy * dy + dx * dx;
                if (lengthSq > farthest)
                {
                    farthest = lengthSq;
                    goal.x = (reached.x - targetPosition->x) * 0.98f + targetPosition->x;
                    goal.y = (reached.y - targetPosition->y) * 0.98f + targetPosition->y;
                    goal.z = (reached.z - targetPosition->z) * 0.98f + targetPosition->z;
                    blockedPolygon = tracedPolygon;
                }
            }
        }
        goalIsland = static_cast<unsigned int>(navigation->getInfantryIsland());
        if (clear)
        {
            m_target.x = (reached.x - targetPosition->x) * 0.98f + targetPosition->x;
            m_target.y = (reached.y - targetPosition->y) * 0.98f + targetPosition->y;
            m_target.z = (reached.z - targetPosition->z) * 0.98f + targetPosition->z;
            goalPolygon = tracedPolygon;
        }
        else
        {
            m_target = goal;
            goalPolygon = blockedPolygon;
        }
    }
    else
    {
        m_flag11D = false;
        Vec3 zero = {0.0f, 0.0f, 0.0f};
        setFocus(m_bot, false, zero, static_cast<unsigned int>(-1));
        m_target = *targetPosition;
    }

    m_int70 = 0;
    m_flag102 = false;
    if (m_flag11D)
        m_target.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getHeight(m_target.x, m_target.z);
    m_active = m_bot->getBotNavigation()->initPathfinding(goalIsland, goalPolygon, m_target, reach, m_range);
    m_time128 = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

bool BAPMoveToObjectFinding::isFinished(Bot* bot)
{
    const bool stopped = m_until && m_until->evaluate(bot);
    bool finished;
    if (!m_flag11D)
        finished = stopped || isWithinRange();
    else
        finished = stopped || (m_flag11C && isWithinRange());
    if (!finished)
        return false;
    bot->getBotNavigation()->clearProgress();
    return true;
}

bool BAPMoveToObjectFinding::getPathTurn(float& turn)
{
    return m_bot->getBotNavigation()->getLargestAngleDiff(turn);
}

float BAPMoveToObjectFinding::slot14() { return m_bot->getBotNavigation()->getDistanceLeft(); }

float BAPMoveToObjectFinding::getDistance(Bot* bot) { return BAPAMoveToObject::getDistance(bot); }
float BAPMoveToObjectFinding::getDistanceSq(Bot* bot) { return BAPAMoveToObject::getDistanceSq(bot); }

bool BAPMoveToObjectFinding::slot18() { return true; }

bool BAPMoveToObjectFinding::isWithinRange()
{
    const float distanceSq = getDistanceSq(m_bot);
    return distanceSq <= m_rangeSq && !(distanceSq < m_minRangeSq);
}

// ---- BAPSetSensingAgentFocus --------------------------------------------------------------

BAPSetSensingAgentFocus::BAPSetSensingAgentFocus(float weight, const Vec3& direction, int id, bool flag)
    : m_weight(weight), m_flag(flag), m_direction(direction), m_useObject(false)
{
    m_id = id;
}

BAPSetSensingAgentFocus::~BAPSetSensingAgentFocus()
{
}

bool BAPSetSensingAgentFocus::execute(Bot* bot, bool& finished, bool& failed)
{
    if (m_useObject)
        bot->getSensingAgent()->setPlanTarget(m_weight, m_object, m_flag);
    else
        bot->getSensingAgent()->setPlanTarget(m_weight, m_direction, m_flag);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPSetSensingAgentFocus::getType() { return BAPSTSetSensingAgentFocus; }
bool BAPSetSensingAgentFocus::isComposite() { return true; }

// ---- BAPMoveTo -----------------------------------------------------------------------------

// The equipment is not null-checked, as shipped.
BAPMoveTo::BAPMoveTo(Bot* bot, const Vec3& target, float speed, float cruiseSpeed, BAPCondition* until,
                     unsigned long long inputMask, float radius, int id)
    : m_int18(0), m_int1C(0), m_int20(0), m_speed(speed), m_cruiseSpeed(cruiseSpeed), m_until(until),
      m_target(target), m_radius(radius), m_int50(0), m_int54(0)
{
    m_vec40.x = m_vec40.y = m_vec40.z = 0.0f;
    m_forward.x = m_forward.y = m_forward.z = 0.0f;
    m_up.x = m_up.y = m_up.z = 0.0f;
    m_right.x = m_right.y = m_right.z = 0.0f;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    m_id = id;
    if (m_until)
        m_until->m_owner = this;
    m_altitude = equipment->getAltitude(equipment->getMobile()->getSpeed());
    m_inputMask = inputMask;
    const float* const m = equipment->getTransformation().m;
    m_forward.x = m[8];
    m_forward.y = m[9];
    m_forward.z = m[10];
    m_right.x = m[0];
    m_right.y = m[1];
    m_right.z = m[2];
    m_up.x = m[4];
    m_up.y = m[5];
    m_up.z = m[6];
    m_vec40.x = m_vec40.y = m_vec40.z = 0.0f;
}

BAPMoveTo::~BAPMoveTo()
{
    if (m_until)
        delete m_until;
}

bool BAPMoveTo::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

unsigned long long BAPMoveTo::getInputMask() { return m_inputMask; }
BAPStatementType BAPMoveTo::getType() { return BAPSTMoveTo; }
bool BAPMoveTo::isComposite() { return false; }

bool BAPMoveTo::isStopConditionMet(Bot* bot)
{
    return m_until && m_until->evaluate(bot);
}

// ---- BAPICTurnLock -------------------------------------------------------------------------

BAPICTurnLock::BAPICTurnLock(bool lock) : m_lock(lock) {}
BAPICTurnLock::~BAPICTurnLock() {}

bool BAPICTurnLock::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->setPlanLock(m_lock);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPICTurnLock::getType() { return BAPSTICTurnLock; }
bool BAPICTurnLock::isComposite() { return true; }

// ---- BAPUpdateVehicle ----------------------------------------------------------------------

BAPUpdateVehicle::BAPUpdateVehicle(int id) { m_id = id; }
BAPUpdateVehicle::~BAPUpdateVehicle() {}

bool BAPUpdateVehicle::execute(Bot* bot, bool& finished, bool& failed)
{
    static_cast<BotMain*>(bot)->requestEquipmentUpdate();
    finished = true;
    failed = true;
    return true;
}

unsigned long long BAPUpdateVehicle::getInputMask() { return 0; }
BAPStatementType BAPUpdateVehicle::getType() { return BAPSTUpdateVehicle; }
bool BAPUpdateVehicle::isComposite() { return true; }

// ---- BAPPlanEnd ---------------------------------------------------------------------------

BAPPlanEnd::BAPPlanEnd(int id, int mode, BAPCondition* condition)
    : m_condition(condition), m_mode(mode)
{
    m_id = id;
    if (condition)
        condition->m_owner = this;
}

BAPPlanEnd::~BAPPlanEnd()
{
    delete m_condition;
}

bool BAPPlanEnd::execute(Bot* bot, bool& finished, bool& failed)
{
    switch (m_mode)
    {
    case 0:
        if (m_condition)
        {
            if (m_condition->evaluate(bot))
            {
                failed = true;
                finished = false;
            }
            else
            {
                failed = false;
                finished = true;
            }
        }
        else
        {
            failed = true;
            finished = false;
        }
        break;
    case 1:
        if (m_condition)
        {
            failed = m_condition->evaluate(bot);
            finished = true;
        }
        else
        {
            failed = true;
            finished = false;
        }
        break;
    case 2:
        if (m_condition)
        {
            failed = !m_condition->evaluate(bot);
            finished = true;
        }
        else
        {
            failed = false;
            finished = false;
        }
        break;
    default:
        break;
    }
    return true;
}

unsigned long long BAPPlanEnd::getInputMask() { return 0; }
BAPStatementType BAPPlanEnd::getType() { return BAPSTPlanEnd; }
bool BAPPlanEnd::isComposite() { return true; }

// ---- BAPForgetTarget ------------------------------------------------------------------------

BAPForgetTarget::BAPForgetTarget(unsigned int target) : m_target(target) {}
BAPForgetTarget::~BAPForgetTarget() {}

bool BAPForgetTarget::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->getSensingAgent()->getSenses()->forgetObject(m_target);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPForgetTarget::getType() { return BAPSTForgetTarget; }
bool BAPForgetTarget::isComposite() { return true; }

// ---- BAPEvaluateCondition -------------------------------------------------------------------

BAPEvaluateCondition::BAPEvaluateCondition(BAPCondition* condition, int id)
    : m_condition(condition)
{
    m_id = id;
    condition->m_owner = this;
}

BAPEvaluateCondition::~BAPEvaluateCondition()
{
    delete m_condition;
}

bool BAPEvaluateCondition::execute(Bot* bot, bool& finished, bool& failed)
{
    m_result = m_condition->evaluate(bot);
    finished = true;
    failed = true;
    return true;
}

unsigned long long BAPEvaluateCondition::getInputMask() { return 0; }
BAPStatementType BAPEvaluateCondition::getType() { return BAPSTEvaluateCondition; }
bool BAPEvaluateCondition::isComposite() { return true; }

// ---- BAPUseFlareOnMissileLock --------------------------------------------------------------

BAPUseFlareOnMissileLock::BAPUseFlareOnMissileLock(int arg, unsigned long long inputMask, int id)
    : m_arg18(arg), m_lockTime(-1.0), m_locked(false)
{
    m_inputMask = inputMask;
    m_id = id;
}

BAPUseFlareOnMissileLock::~BAPUseFlareOnMissileLock() {}

// The equipment is not null-checked, as shipped.
bool BAPUseFlareOnMissileLock::execute(Bot* bot, bool& finished, bool& failed)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIUnit* const unit = equipment->getUnit();
    const int status = unit->getMissileLockStatus();
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!m_locked && status > 0)
    {
        m_lockTime = now;
        m_locked = true;
    }
    const double delay = ((equipment->getObjectTypeFlags() >> 4) & 1) ? 1.0 : 0.0;
    if (m_locked && !(now - m_lockTime < delay))
    {
        io::PlayerInput* const input = bot->playerInput();
        if (input->mappingFlags & (1ULL << io::PIFlareFire))
            input->inputs[io::PIFlareFire] = 1.0f;
        unit->setMissileLockStatus(0);
        m_locked = false;
    }
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPUseFlareOnMissileLock::getType() { return BAPSTUseFlareOnMissileLock; }
bool BAPUseFlareOnMissileLock::isComposite() { return true; }

// ---- BAPSendRadioMessage -------------------------------------------------------------------

BAPSendRadioMessage::BAPSendRadioMessage(int type, int channel, unsigned int priority, float delay, int id,
                                         int flag)
    : m_type(type), m_channel(channel), m_priority(priority), m_flag(flag), m_delay(delay)
{
    m_id = id;
}

BAPSendRadioMessage::~BAPSendRadioMessage() {}

bool BAPSendRadioMessage::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->getCommunicationAgent()->enqueueRadioMessage(m_type, m_channel, m_priority, m_delay, m_flag);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPSendRadioMessage::getType() { return BAPSTSendRadioMessage; }
bool BAPSendRadioMessage::isComposite() { return true; }

// ---- BAPResetControls ----------------------------------------------------------------------

BAPResetControls::BAPResetControls(bool movement, bool look, bool actions, int id)
    : m_movement(movement)
    , m_look(look)
    , m_actions(actions)
    , m_input(0)
{
    m_id = id;
    m_inputMask = 0;
}

BAPResetControls::~BAPResetControls() {}

// The fallback when no entry is installed: finished and failed at once.
bool BAPResetControls::execute(Bot*, bool& finished, bool& failed)
{
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPResetControls::getType() { return BAPSTResetControls; }
bool BAPResetControls::isComposite() { return false; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPResetControls::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPResetControls::printDebug");
}

// ---- BAPIf ---------------------------------------------------------------------------------

// The condition is claimed without a null check; the branches only when present.
BAPIf::BAPIf(BAPCondition* condition, BotScript* then, BotScript* otherwise, int id,
             bool reevaluate, bool reevaluateOnlyInElse)
    : m_condition(condition)
    , m_then(then)
    , m_otherwise(otherwise)
    , m_current(0)
    , m_lastChosen(0)
    , m_reevaluate(reevaluate)
    , m_reevaluateOnlyInElse(reevaluateOnlyInElse)
{
    m_id = id;
    condition->m_owner = this;
    if (m_then)
        m_then->m_owner = this;
    if (m_otherwise)
        m_otherwise->m_owner = this;
}

BAPIf::~BAPIf()
{
    if (m_condition)
        delete m_condition;
    if (m_then)
        delete m_then;
    if (m_otherwise)
        delete m_otherwise;
}

// The condition picks a branch when nothing runs yet, or every tick while reevaluating (only from
// the else branch when so configured). A failed branch is dropped. No branch: finished, failed.
bool BAPIf::execute(Bot* bot, bool& finished, bool& failed)
{
    bool choose;
    if (!m_reevaluate)
        choose = !m_current;
    else if (!m_reevaluateOnlyInElse || m_current != m_otherwise)
        choose = true;
    else
        choose = !m_current;

    if (choose)
    {
        m_current = m_condition->evaluate(bot) ? m_then : m_otherwise;
        m_lastChosen = m_current;
        if (!m_current)
        {
            finished = true;
            failed = true;
            return true;
        }
    }
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_current, bot, finished, failed);
    if (failed)
        m_current = 0;
    return true;
}

unsigned long long BAPIf::getInputMask()
{
    const unsigned long long thenMask = m_then ? m_then->getInputMask() : 0;
    const unsigned long long otherwiseMask = m_otherwise ? m_otherwise->getInputMask() : 0;
    return otherwiseMask | thenMask;
}

BAPStatementType BAPIf::getType() { return BAPSTIf; }
bool BAPIf::isComposite() { return true; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the bodies and validate the plan debugger.
void BAPIf::getDebugString(Bot*, std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPIf::getDebugString");
}

void BAPIf::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPIf::printDebug");
}

// ---- BAPTrigger ----------------------------------------------------------------------------

// Both conditions are claimed when present. The start time is the game time plus the delay.
BAPTrigger::BAPTrigger(int input, BAPCondition* until, BAPCondition* release,
                       unsigned long long inputMask, int id, double delay, bool flag)
    : m_input(input)
    , m_until(until)
    , m_release(release)
    , m_pressed(false)
    , m_flag30(flag)
{
    if (m_until)
        m_until->m_owner = this;
    if (m_release)
        m_release->m_owner = this;
    m_id = id;
    m_inputMask = inputMask;
    m_startTime =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + delay;
}

BAPTrigger::~BAPTrigger()
{
    if (m_until)
        delete m_until;
    if (m_release)
        delete m_release;
}

// The fallback when no entry is installed: unfinished and failed.
bool BAPTrigger::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPTrigger::getType() { return BAPSTTrigger; }
bool BAPTrigger::isComposite() { return false; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the bodies and validate the plan debugger.
void BAPTrigger::getDebugString(Bot*, std::string&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPTrigger::getDebugString");
}

void BAPTrigger::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPTrigger::printDebug");
}

// ---- BAPWhile ------------------------------------------------------------------------------

BAPWhile::BAPWhile(BAPCondition* condition, BotScript* body, int id)
    : m_condition(condition)
    , m_body(body)
    , m_abort(false)
    , m_testCondition(true)
    , m_testAfterFailure(true)
{
    m_id = id;
    condition->m_owner = this;
    if (m_body)
        m_body->m_owner = this;
}

BAPWhile::BAPWhile(BAPCondition* condition, BotScript* body, bool testAfterFailure, int id)
    : m_condition(condition)
    , m_body(body)
    , m_abort(false)
    , m_testCondition(true)
    , m_testAfterFailure(testAfterFailure)
{
    m_id = id;
    condition->m_owner = this;
    if (m_body)
        m_body->m_owner = this;
}

BAPWhile::~BAPWhile()
{
    if (m_condition)
        delete m_condition;
    if (m_body)
        delete m_body;
}

// Without a body the condition alone decides. The condition is tested before the first run and
// again after the body fails; while the body succeeds it runs again untested. The loop never
// reports a failed iteration upward.
bool BAPWhile::execute(Bot* bot, bool& finished, bool& failed)
{
    if (!m_body)
    {
        finished = true;
        m_abort = false;
        failed = !m_condition->evaluate(bot);
        return true;
    }
    if (m_abort)
    {
        finished = true;
        failed = true;
        m_abort = false;
        return true;
    }
    if (m_testCondition && !m_condition->evaluate(bot))
    {
        finished = true;
        failed = true;
        return true;
    }
    static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_body, bot, finished, failed);
    if (m_testAfterFailure)
        m_testCondition = failed;
    else
        m_testCondition = true;
    failed = false;
    return true;
}

unsigned long long BAPWhile::getInputMask()
{
    if (!m_body)
        return 0;
    return m_body->getInputMask();
}

BAPStatementType BAPWhile::getType() { return BAPSTWhile; }
bool BAPWhile::isComposite() { return true; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPWhile::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPWhile::printDebug");
}

// ---- BAPWrapper ----------------------------------------------------------------------------

BAPWrapper::BAPWrapper(BotScript* child, int id)
    : m_child(child)
{
    m_id = id;
}

// The child is not destroyed here.
BAPWrapper::~BAPWrapper()
{
}

bool BAPWrapper::execute(Bot* bot, bool& finished, bool& failed)
{
    return static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(m_child, bot, finished, failed);
}

unsigned long long BAPWrapper::getInputMask()
{
    return m_child->getInputMask();
}

BAPStatementType BAPWrapper::getType() { return BAPSTInfoWrapper; }
bool BAPWrapper::isComposite() { return true; }

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Debug output only.
// [RETIRE-WHEN: bap-debug-print] Recover the body and validate the plan debugger.
void BAPWrapper::printDebug(std::ostream&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPWrapper::printDebug");
}

int BAPWrapper::getWrapperIndex() { return 9; }

// ---- BAPInfoWrapper ------------------------------------------------------------------------

BAPInfoWrapper::BAPInfoWrapper(const Vec3& position, BotScript* child, int id, Bot* bot)
    : BAPWrapper(child, id)
    , m_position(position)
    , m_bot(bot)
{
    if (bot && bot->getSquadMember())
        bot->getSquadMember()->setFlag49(true);
    m_child->m_owner = this;
}

BAPInfoWrapper::~BAPInfoWrapper()
{
    if (m_bot && m_bot->getSquadMember())
        m_bot->getSquadMember()->setFlag49(false);
    if (m_child)
        delete m_child;
}

int BAPInfoWrapper::getWrapperIndex() { return 4; }

// The child is claimed without a null check, as shipped.
BAPMoveToWrapper::BAPMoveToWrapper(const Vec3& position, BotScript* child, int id)
    : BAPWrapper(child, id)
    , m_position(position)
{
    m_child->m_owner = this;
}

BAPMoveToWrapper::~BAPMoveToWrapper()
{
    if (m_child)
        delete m_child;
}

int BAPMoveToWrapper::getWrapperIndex() { return 2; }

// ---- BAPMergeSoldierInputs ----------------------------------------------------------------

BAPMergeSoldierInputs::BAPMergeSoldierInputs(int id)
    : m_yawError(0.0f)
    , m_pitchError(0.0f)
{
    m_id = id;
}

BAPMergeSoldierInputs::~BAPMergeSoldierInputs() {}

namespace {

// Writable data in the shipped image. [INFERRED] names.
float s_degreesToRadians = 0.01745329238474369f;   // PE 102AC788
int   s_turnPredictionSteps = 3;                    // PE 102AC7A8

float signOf(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (!(value >= 0.0f))
        return -1.0f;
    return 0.0f;
}

// PE 100E9DE0, the conditional Vec2 normaliser.
void normaliseVec2(Vec2& value)
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

// sign(a) * sqrt(|a|), clamped to [-1, 1].
float softAxis(float a)
{
    float value = signOf(a) * std::sqrt(std::fabs(a));
    if (!(-1.0f <= value))
        value = -1.0f;
    else if (!(1.0f >= value))
        value = 1.0f;
    return value;
}

void setIfMapped(io::PlayerInput* input, io::PlayerInputMap which, float value)
{
    if (input->mappingFlags & (1ULL << which))
        input->inputs[which] = value;
}

bool isPressed(io::PlayerInput* input, io::PlayerInputMap which)
{
    return (input->mappingFlags & (1ULL << which)) && input->inputs[which] != 0.0f;
}

} // anonymous namespace

// PE 101991F0. Walking: the throttle is split between forward and strafe so the soldier keeps
// moving along the sensing agent's look direction whatever way it faces, with the axes turned
// ahead by the soldier's current yaw rate. Sprinting or swimming: the look is steered onto that
// direction instead, and the throttle and sprint follow how far away the focus is and how far
// the soldier must still turn.
bool BAPMergeSoldierInputs::execute(Bot* bot, bool& finished, bool& failed)
{
    io::PlayerInput* const input = bot->playerInput();
    SensingAgent* const agent = bot->getSensingAgent();
    const Vec2 look = agent->getLookDirection();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const Mat4& transform = equipment->getTransformation();
    float pitchError = 0.0f;
    float yawError = 0.0f;
    IPIControlInfo* const control = equipment->getControlInfo();
    const bool swimming = equipment->getSoldierPose() == SPSwimming;

    bool steer;
    bool sprinting = false;
    if (isPressed(input, io::PISprint))
    {
        steer = true;
        sprinting = true;
    }
    else
    {
        steer = swimming;
    }

    if (!steer)
    {
        Vec2 forward;
        forward.x = transform.m[8];
        forward.y = transform.m[10];
        Vec2 right;
        right.x = transform.m[0];
        right.y = transform.m[2];
        normaliseVec2(forward);
        normaliseVec2(right);
        IAIObject* const object = equipment->getAIObject();
        if (object)
        {
            world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(
                world::object_detail::LayoutAccess::physicsNode(object->getObject()));
            if (node)
            {
                const float elapsed = static_cast<float>(
                    static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
                const float ahead = static_cast<float>(s_turnPredictionSteps) * elapsed;
                const float rate = s_degreesToRadians * node->getRotationalSpeed().y;
                if (rate != 0.0f)
                {
                    const float angle = rate * ahead;
                    const float sine = std::sin(angle);
                    const float cosine = std::cos(angle);
                    // Row-major 3x3 with rows (c, -s, 0), (s, c, 0), (0, 1, 0); a Vec2 is turned
                    // by its upper-left block (PE 10199180 / 10199100).
                    const float m0 = cosine, m1 = -sine, m3 = sine, m4 = cosine;
                    const float fx = forward.x * m0 + forward.y * m3;
                    const float fy = forward.x * m1 + forward.y * m4;
                    forward.x = fx;
                    forward.y = fy;
                    const float rx = right.x * m0 + right.y * m3;
                    const float ry = right.x * m1 + right.y * m4;
                    right.x = rx;
                    right.y = ry;
                }
            }
        }
        const bool throttleMapped = (input->mappingFlags & (1ULL << io::PIThrottle)) != 0;
        const float throttle = std::fabs(throttleMapped ? input->inputs[io::PIThrottle] : 0.0f);
        const float ahead = softAxis(forward.x * look.x + forward.y * look.y) * throttle;
        const float strafe = softAxis(right.x * look.x + right.y * look.y) * throttle;
        if (throttleMapped)
            input->inputs[io::PIThrottle] = ahead;
        setIfMapped(input, io::PIYaw, strafe);
        setIfMapped(input, io::PISprint, 0.0f);
    }
    else
    {
        Vec3 direction;
        direction.x = look.x;
        direction.y = 0.0f;
        direction.z = look.y;
        steerLook(bot, direction,
                  *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation()),
                  pitchError, m_pitchError, yawError, m_yawError,
                  0.0f, control, input, 0.0f, 0.0f, 0.0f);
        const float turnLimit = s_degreesToRadians * 40.0f;
        float distanceSquared = 0.0f;
        float near = 16.0f;
        bool targetIsVehicle = false;
        if (sprinting)
        {
            const float px = transform.m[12];
            const float pz = transform.m[14];
            float tx;
            float tz;
            Information* target = 0;
            if (agent->getFocusObject() != static_cast<unsigned int>(-1))
                target = informationManager->getInformation(agent->getFocusObject());
            if (target)
            {
                const Vec3& position = target->getPosition();
                tx = position.x;
                tz = position.z;
                IAIObject* const targetObject = target->getAIObject();
                targetIsVehicle = targetObject && !IBotManager::instance->getBot(targetObject) &&
                                  target->getUnit() && target->getUnit()->isOccupied();
            }
            else
            {
                tx = agent->getFocusPoint().x;
                tz = agent->getFocusPoint().z;
            }
            const float dx = px - tx;
            const float dz = pz - tz;
            distanceSquared = dx * dx + dz * dz;
        }

        bool close = false;
        if (agent->getFocusObject() != static_cast<unsigned int>(-1) && distanceSquared < 9.0f &&
            targetIsVehicle)
        {
            close = true;
            near = 1.0f;
        }
        if (distanceSquared < near && !close && !(turnLimit * 5.0f >= std::fabs(yawError)))
        {
            setIfMapped(input, io::PIThrottle, 0.0f);
            setIfMapped(input, io::PISprint, 0.0f);
        }
        else if ((!(std::fabs(yawError) >= turnLimit) || close) && distanceSquared > near)
        {
            if (sprinting)
                setIfMapped(input, io::PISprint, 1.0f);
            setIfMapped(input, io::PIThrottle, 1.0f);
        }
        else
        {
            setIfMapped(input, io::PIThrottle, 1.0f);
            setIfMapped(input, io::PISprint, 0.0f);
        }
    }
    m_pitchError = pitchError;
    m_yawError = yawError;
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPMergeSoldierInputs::getType() { return BAPSTMergeSoldierInputs; }
bool BAPMergeSoldierInputs::isComposite() { return true; }

// ---- BAPALook ------------------------------------------------------------------------------

BAPALook::BAPALook(float deadZone, BAPCondition* until, unsigned long long inputMask, int id)
    : m_until(until)
    , m_deadZone(deadZone)
{
    m_state20 = 0.0f;
    m_state24 = 0.0f;
    m_lastForward.x = m_lastForward.y = m_lastForward.z = 0.0f;
    m_inputMask = inputMask;
    m_id = id;
}

// The condition is the derived classes' to destroy.
BAPALook::~BAPALook() {}

bool BAPALook::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPALook::getType() { return BAPSTLookAt; }
bool BAPALook::isComposite() { return false; }

// The condition is claimed without a null check.
BAPALookDirection::BAPALookDirection(const Vec3& direction, float deadZone, BAPCondition* until,
                                     unsigned long long inputMask, int id)
    : BAPALook(deadZone, until, inputMask, id)
    , m_direction(direction)
{
    m_until->m_owner = this;
}

BAPALookDirection::~BAPALookDirection()
{
    if (m_until)
        delete m_until;
}

Vec3 BAPALookDirection::getTarget(bool& valid)
{
    valid = true;
    return m_direction;
}

BAPALookAtObject::BAPALookAtObject(Bot* bot, unsigned int object, float deadZone, BAPCondition* until,
                                   unsigned long long inputMask, int id,
                                   BotScriptValue<Vec3>* position, float heightOffset)
    : BAPALook(deadZone, until, inputMask, id)
    , m_bot(bot)
    , m_object(object)
    , m_position(position)
    , m_heightOffset(heightOffset)
{
    m_until->m_owner = this;
}

BAPALookAtObject::~BAPALookAtObject()
{
    if (m_until)
        delete m_until;
}

// PE 101AD890. The direction from the camera of the bot's current equipment to the target, or
// (1, 0, 0) with `valid` cleared for an unknown object. The equipment is not null-checked, as
// shipped.
Vec3 BAPALookAtObject::getTarget(bool& valid)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    Information* const object = informationManager->getInformation(m_object);
    valid = object != 0;
    Vec3 direction;
    if (!valid)
    {
        direction.x = 1.0f;
        direction.y = 0.0f;
        direction.z = 0.0f;
        return direction;
    }
    const Mat4& camera = equipment->getControlInfo()->getCameraTransformation();
    const Vec3& from = *reinterpret_cast<const Vec3*>(&camera.m[12]);
    Vec3 to;
    if (!m_position)
    {
        const Vec3& position = object->getPosition();
        to = position;
    }
    else
    {
        to = m_position->m_value;
    }
    direction.x = to.x - from.x;
    direction.y = to.y + m_heightOffset - from.y;
    direction.z = to.z - from.z;
    normaliseVector(direction);
    return direction;
}

// ---- BAPAimAt -------------------------------------------------------------------------------

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

// The plan statements store the identity inline.
void setIdentity(Mat4& m)
{
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
}

// PE 10283F68. [UNRESOLVED] what reads the field it initialises.
const double kAimUnsetTime = -5000000000.0;

float worldGravity()
{
    return static_cast<const IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();
}

const IBFTimer* aimTimer()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
}

} // anonymous namespace

BAPAimAt::BAPAimAt(BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction)
    : m_valid(valid), m_direction(direction)
{
    m_offset.x = m_offset.y = m_offset.z = 0.0f;
    for (int i = 0; i < 4; ++i) {
        m_unresolved24[i].x = m_unresolved24[i].y = m_unresolved24[i].z = 0.0f;
        m_unresolved54[i].x = m_unresolved54[i].y = m_unresolved54[i].z = 0.0f;
    }
    m_previousYaw = 0.0f;
    for (int i = 0; i < 10; ++i)
        m_pitchHistory[i] = 0.0f;
    m_pitchIndex = 0;
    m_unresolved150[2] = 0;
}

BAPAimAt::~BAPAimAt() {}

bool BAPAimAt::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPAimAt::getType() { return BAPSTAimAt; }
bool BAPAimAt::isComposite() { return false; }

Vec3& BAPAimAt::clearFireOffset(Bot* bot)
{
    Vec3& offset = static_cast<BotMain*>(bot)->getFireCorrection()->getTargetOffset();
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = 0.0f;
    return offset;
}

// ---- BAPAimAtObject -------------------------------------------------------------------------

BAPAimAtObject::BAPAimAtObject(Bot* bot, unsigned int target, const Vec3& offset,
                               BAPCondition* condition, unsigned long long inputMask, int id,
                               bool usePosition, BotScriptValue<bool>* valid,
                               BotScriptValue<Vec3>* direction)
    : BAPAimAt(valid, direction), m_bot(bot), m_target(target), m_impactTime(0.0),
      m_solved(false), m_usePosition(usePosition)
{
    m_aimDirection.x = m_aimDirection.y = m_aimDirection.z = 0.0f;
    m_aimPoint.x = m_aimPoint.y = m_aimPoint.z = 0.0f;
    setIdentity(m_frame);

    // The shipped constructor resolves the primary objects of the bot's equipment and of the
    // target and discards them; neither lookup is checked.
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment->isPrimary())
        equipment->getPrimaryObject(bot->getSide());
    Information* const aimed = informationManager->getInformation(target);
    if (!aimed->isPrimary())
        aimed->getPrimaryObject(bot->getSide());

    m_offset = offset;
    m_condition = condition;
    condition->m_owner = this;
    m_double0A0 = kAimUnsetTime;
    m_inputMask = inputMask;
    m_unresolved150[0] = 0;
    m_unresolved150[2] = 0;
    m_id = id;
}

BAPAimAtObject::~BAPAimAtObject()
{
    if (m_condition)
        delete m_condition;
}

bool BAPAimAtObject::tracksObject() { return true; }

bool BAPAimAtObject::computeAim(const Mat4& frame, const Vec3& ownVelocity, Vec3& direction,
                                float effort, float drag, float speed, float gravityModifier,
                                bool lob, Bot* bot)
{
    if (m_valid && m_direction) {
        direction = m_direction->m_value;
        m_aimDirection = direction;
        m_solved = m_valid->m_value;
        return m_valid->m_value;
    }

    Vec3 aim = {0.0f, 0.0f, 0.0f};
    Vec3& fireOffset = clearFireOffset(bot);
    const IBFTimer* const timer = aimTimer();
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    m_bot->getCurrentEquipment();   // the result is unused, as shipped
    m_frame = frame;
    m_solver.setDrag(drag);
    m_solver.setSpeed(speed);
    m_solver.setGravity(worldGravity() * gravityModifier);

    // The bot's sensed point on the target, or its position when allowed.
    const SensedObject* const sensed =
        static_cast<const SensedObject*>(m_bot->getSensedObjectFromHandle(m_target));
    Vec3 point = {0.0f, 0.0f, 0.0f};
    if (!sensed) {
        if (!m_usePosition)
            return false;
        point = target->getPosition();
    } else {
        transformPoint(target->getAIObject()->getTransformation(), sensed->m_senseNormal, point);
    }

    const float* const origin = m_frame.m + 12;
    aim.x = point.x - origin[0] + m_offset.x;
    aim.y = point.y - origin[1] + m_offset.y;
    aim.z = point.z - origin[2] + m_offset.z;
    aim.x += fireOffset.x;
    aim.y += fireOffset.y;
    aim.z += fireOffset.z;

    Vec3 targetVelocity = {0.0f, 0.0f, 0.0f};
    IPIMobile* mobile = target->getMobile();
    if (!mobile && !target->isPrimary()) {
        Information* const primary =
            informationManager->getInformation(target->getPrimaryObject(m_bot->getSide()));
        if (primary)
            mobile = primary->getMobile();
    }
    if (mobile)
        targetVelocity = mobile->getSpeed();

    m_solver.setTarget(aim, targetVelocity, ownVelocity, &m_frame, timer->getGameTime());
    m_solved = m_solver.solve(timer->getGameTime(), effort, lob, m_aimDirection, m_aimPoint,
                              &m_impactTime);
    m_aimPoint.x += origin[0];
    m_aimPoint.y += origin[1];
    m_aimPoint.z += origin[2];
    m_aimPoint.x -= fireOffset.x;
    m_aimPoint.y -= fireOffset.y;
    m_aimPoint.z -= fireOffset.z;
    direction = m_aimDirection;
    return m_solved;
}

bool BAPAimAtObject::getRelativePosition(const Vec3& from, Vec3& out)
{
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    const Vec3& position = target->getPosition();
    out.x = position.x - from.x;
    out.y = position.y - from.y;
    out.z = position.z - from.z;
    return true;
}

const Mat4& BAPAimAtObject::getFrame() { return m_frame; }
bool BAPAimAtObject::isSolved() { return m_solved; }

bool BAPAimAtObject::getAimDirection(Vec3& out)
{
    if (!m_solved)
        return false;
    out = m_aimDirection;
    return true;
}

bool BAPAimAtObject::getAimPoint(Vec3& out)
{
    if (!m_solved)
        return false;
    out = m_aimPoint;
    return true;
}

bool BAPAimAtObject::getImpactTime(double& out)
{
    if (!m_solved)
        return false;
    out = m_impactTime;
    return true;
}

bool BAPAimAtObject::getProjectilePosition(Vec3& out, double time)
{
    const Vec3 forward = {m_frame.m[8], m_frame.m[9], m_frame.m[10]};
    const bool reached = m_solver.projectilePosition(forward, static_cast<float>(time), out);
    out.x += m_frame.m[12];
    out.y += m_frame.m[13];
    out.z += m_frame.m[14];
    return reached;
}

// ---- BAPAimAtPosition -----------------------------------------------------------------------

BAPAimAtPosition::BAPAimAtPosition(Bot* bot, BotScriptValue<Vec3>* position, const Vec3& offset,
                                   BAPCondition* condition, unsigned long long inputMask, int id,
                                   BotScriptValue<bool>* valid, BotScriptValue<Vec3>* direction)
    : BAPAimAt(valid, direction), m_position(position), m_bot(bot), m_impactTime(0.0),
      m_solved(false)
{
    m_aimDirection.x = m_aimDirection.y = m_aimDirection.z = 0.0f;
    m_aimPoint.x = m_aimPoint.y = m_aimPoint.z = 0.0f;
    setIdentity(m_frame);
    m_offset = offset;
    m_condition = condition;
    condition->m_owner = this;
    m_double0A0 = kAimUnsetTime;
    m_id = id;
    m_inputMask = inputMask;
    m_unresolved150[0] = 0;
    m_unresolved150[2] = 0;
}

BAPAimAtPosition::~BAPAimAtPosition()
{
    if (m_condition)
        delete m_condition;
}

bool BAPAimAtPosition::tracksObject() { return false; }

bool BAPAimAtPosition::computeAim(const Mat4& frame, const Vec3& ownVelocity, Vec3& direction,
                                  float effort, float drag, float speed, float gravityModifier,
                                  bool lob, Bot* bot)
{
    const IBFTimer* const timer = aimTimer();
    Vec3 aim = {0.0f, 0.0f, 0.0f};
    m_bot->getCurrentEquipment();   // the result is unused, as shipped
    Vec3& fireOffset = clearFireOffset(bot);
    m_frame = frame;
    m_solver.setDrag(drag);
    m_solver.setSpeed(speed);
    m_solver.setGravity(worldGravity() * gravityModifier);

    const Vec3& position = m_position->m_value;
    const float* const origin = m_frame.m + 12;
    aim.x = position.x - origin[0] + m_offset.x;
    aim.y = position.y - origin[1] + m_offset.y;
    aim.z = position.z - origin[2] + m_offset.z;
    aim.x += fireOffset.x;
    aim.y += fireOffset.y;
    aim.z += fireOffset.z;

    const Vec3 still = {0.0f, 0.0f, 0.0f};
    m_solver.setTarget(aim, still, ownVelocity, &frame, timer->getGameTime());
    m_solved = m_solver.solve(timer->getGameTime(), effort, lob, m_aimDirection, m_aimPoint,
                              &m_impactTime);
    m_aimPoint.x += origin[0];
    m_aimPoint.y += origin[1];
    m_aimPoint.z += origin[2];
    m_aimPoint.x -= fireOffset.x;
    m_aimPoint.y -= fireOffset.y;
    m_aimPoint.z -= fireOffset.z;
    direction = m_aimDirection;
    return m_solved;
}

bool BAPAimAtPosition::getRelativePosition(const Vec3& from, Vec3& out)
{
    const Vec3& position = m_position->m_value;
    out.x = position.x - from.x;
    out.y = position.y - from.y;
    out.z = position.z - from.z;
    return true;
}

const Mat4& BAPAimAtPosition::getFrame() { return m_frame; }
bool BAPAimAtPosition::isSolved() { return m_solved; }

bool BAPAimAtPosition::getAimDirection(Vec3& out)
{
    if (!m_solved)
        return false;
    out = m_aimDirection;
    return true;
}

bool BAPAimAtPosition::getAimPoint(Vec3& out)
{
    if (!m_solved)
        return false;
    out = m_aimPoint;
    return true;
}

bool BAPAimAtPosition::getImpactTime(double& out)
{
    if (!m_solved)
        return false;
    out = m_impactTime;
    return true;
}

bool BAPAimAtPosition::getProjectilePosition(Vec3& out, double time)
{
    const Vec3 forward = {m_frame.m[8], m_frame.m[9], m_frame.m[10]};
    const bool reached = m_solver.projectilePosition(forward, static_cast<float>(time), out);
    out.x += m_frame.m[12];
    out.y += m_frame.m[13];
    out.z += m_frame.m[14];
    return reached;
}

// ---- BAPOverrideSensingAgent ----------------------------------------------------------------

BAPOverrideSensingAgent::~BAPOverrideSensingAgent() {}

bool BAPOverrideSensingAgent::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->getSensingAgent()->setOverride(true);
    finished = true;
    failed = true;
    return false;
}

BAPStatementType BAPOverrideSensingAgent::getType() { return BAPSTOverrideSensingAgent; }
bool BAPOverrideSensingAgent::isComposite() { return false; }

// ---- BAPChangeWrapper -----------------------------------------------------------------------

BAPChangeWrapper::BAPChangeWrapper(unsigned int target, BotScript* child, int id, int playerId)
    : BAPWrapper(child, id), m_target(target), m_playerId(playerId)
{
    // An unknown target books through a null information, as shipped.
    informationManager->getInformation(target)->bookVehicle(playerId);
    m_child->m_owner = this;
}

// The bot of the player is not null-checked, as shipped.
BAPChangeWrapper::~BAPChangeWrapper()
{
    Information* target = informationManager->getInformation(m_target);
    if (target && target->isBooked())
        target->cancelBookVehicle();
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(m_playerId);
    CommunicationAgent* const communication = bot->getCommunicationAgent();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (target && equipment && bot->getCurrentEquipment() == m_target)
    {
        bool thank = false;
        Information* const targetPrimary =
            informationManager->getInformation(target->getPrimaryObject(bot->getSide()));
        Information* const equipmentPrimary =
            informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
        int player = targetPrimary->getPlayerId();
        if (player >= 0 && communication->hasMessage(static_cast<AIRadioMessageType>(1), player)
            && equipmentPrimary == targetPrimary)
            thank = true;
        bool full = !equipment->isPrimary() && targetPrimary->getUnit()->isOccupied()
                    && equipmentPrimary == targetPrimary;
        int iterator;
        Information* part = informationManager->getInformation(
            targetPrimary->getFirstSecondaryObject(iterator, bot->getSide()));
        bool broke = false;
        while (part)
        {
            player = part->getPlayerId();
            if (player >= 0 && communication->hasMessage(static_cast<AIRadioMessageType>(1), player)
                && equipmentPrimary == targetPrimary)
                thank = true;
            full = full && !part->isBooked();
            if (thank && !full)
            {
                broke = true;
                break;
            }
            part = informationManager->getInformation(
                targetPrimary->getSecondaryObject(iterator, bot->getSide()));
        }
        if (!broke && full)
            communication->enqueueRadioMessage(11, 7, 3, 1.0f, -1);
        else if (thank)
            communication->enqueueRadioMessage(8, 7, 3, 1.0f, -1);
    }
    if (m_child)
        delete m_child;
}

int BAPChangeWrapper::getWrapperIndex() { return 8; }

// ---- BAPAirMoveToObject ---------------------------------------------------------------------

BAPAirMoveToObject::BAPAirMoveToObject(unsigned int target, float range, float speed,
                                       float cruiseSpeed, float altitude, BAPCondition* until,
                                       unsigned long long inputMask, int id)
    : m_target(target), m_range(range), m_speed(speed), m_cruiseSpeed(cruiseSpeed), m_until(until),
      m_altitude(altitude), m_targetMobile(false), m_int88(0)
{
    for (int i = 0; i < 10; ++i)
        m_state2C[i] = 0.0f;
    m_id = id;
    m_inputMask = inputMask;
    if (m_until)
        m_until->m_owner = this;
    if (Information* const object = informationManager->getInformation(m_target))
        m_targetMobile = object->getMobile() != 0;
    for (int i = 0; i < 9; ++i)
        m_state60[i] = 0.0f;
    m_state60[1] = 1.0f;   // +0x64
    m_state60[5] = 1.0f;   // +0x74
    m_state60[6] = 1.0f;   // +0x78
}

BAPAirMoveToObject::~BAPAirMoveToObject()
{
    if (m_until)
        delete m_until;
}

bool BAPAirMoveToObject::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPAirMoveToObject::getType() { return BAPSTMoveToObject; }
bool BAPAirMoveToObject::isComposite() { return false; }

bool BAPAirMoveToObject::isFinished(Bot* bot)
{
    return m_until && m_until->evaluate(bot);
}

// ---- BAPRevive -------------------------------------------------------------------------------

BAPRevive::BAPRevive(int id) : m_pressed(false)
{
    m_id = id;
}

BAPRevive::~BAPRevive() {}

// The equipment information is not null-checked, as shipped.
bool BAPRevive::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->requestSoldierPose(2, true);
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment->getSoldierPose() == 2)
    {
        io::PlayerInput* const input = static_cast<BotMain*>(bot)->playerInput();
        if (!m_pressed)
        {
            if (input->mappingFlags & (1ULL << io::PIFire))
                input->inputs[io::PIFire] = 1.0f;
            m_pressed = true;
        }
        else
        {
            failed = true;
            if (input->mappingFlags & (1ULL << io::PIFire))
                input->inputs[io::PIFire] = 0.0f;
            m_pressed = false;
        }
    }
    finished = true;
    return true;
}

BAPStatementType BAPRevive::getType() { return BAPSTRevive; }
bool BAPRevive::isComposite() { return true; }

// ---- BAPPlaceC4 -----------------------------------------------------------------------------

BAPPlaceC4::BAPPlaceC4(int placementType, AIStrategicObject* object, bool place)
    : m_placementType(placementType), m_object(object), m_place(place)
{
}

BAPPlaceC4::~BAPPlaceC4() {}

// The equipment information is not null-checked, as shipped.
bool BAPPlaceC4::execute(Bot* bot, bool& finished, bool& failed)
{
    if (m_place)
    {
        if (!bot->isC4Placed())
        {
            Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
            const Vec3& position = equipment->getPosition();
            bot->placeC4(m_placementType, position, m_object);
        }
    }
    else if (bot->isC4Placed())
    {
        bot->removeC4();
    }
    finished = true;
    failed = true;
    return false;
}

BAPStatementType BAPPlaceC4::getType() { return BAPSTPlaceC4; }
bool BAPPlaceC4::isComposite() { return false; }

// ---- BAPChangeWeapon ------------------------------------------------------------------------

namespace {

// A weapon change releases the fire, alternate-fire and reload inputs.
void releaseFireInputs(io::PlayerInput* input)
{
    if (input->mappingFlags & (1ULL << io::PIFire))
        input->inputs[io::PIFire] = 0.0f;
    if (input->mappingFlags & (1ULL << io::PIAltFire))
        input->inputs[io::PIAltFire] = 0.0f;
    if (input->mappingFlags & (1ULL << io::PIReload))
        input->inputs[io::PIReload] = 0.0f;
}

} // anonymous namespace

BAPChangeWeapon::BAPChangeWeapon(Bot* bot, int weapon, int id) : m_bot(bot), m_weapon(weapon)
{
    m_id = id;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    m_weaponInfo = WeaponInfoManager::m_instance->getInformation(
        equipment->getArmament()->getWeapon(weapon));
    m_inputMask = 1ULL << m_weaponInfo->getWeaponActivate();
}

BAPChangeWeapon::~BAPChangeWeapon() {}

bool BAPChangeWeapon::execute(Bot* bot, bool& finished, bool& failed)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment) {
        finished = false;
        failed = true;
        return true;
    }
    IPIArmament* const armament = equipment->getArmament();
    io::PlayerInput* const input = bot->playerInput();
    if (!((equipment->getObjectTypeFlags() >> 21) & 1)) {
        input->setInput(static_cast<io::PlayerInputMap>(m_weaponInfo->getWeaponActivate()), 1.0f);
        finished = true;
        failed = true;
        return true;
    }
    if (armament->getActiveWeapon() != m_weapon) {
        releaseFireInputs(input);
        input->setInput(static_cast<io::PlayerInputMap>(m_weaponInfo->getWeaponActivate()), 1.0f);
        finished = true;
        failed = false;
        return true;
    }
    input->setInput(static_cast<io::PlayerInputMap>(m_weaponInfo->getWeaponActivate()), 0.0f);
    releaseFireInputs(input);
    finished = true;
    failed = true;
    return true;
}

BAPStatementType BAPChangeWeapon::getType() { return BAPSTChangeWeapon; }
bool BAPChangeWeapon::isComposite() { return true; }

// ---- BAPIdle --------------------------------------------------------------------------------

BAPIdle::BAPIdle(BAPCondition* condition, unsigned long long inputMask, int id)
    : m_condition(condition)
{
    condition->m_owner = this;
    m_inputMask = inputMask;
    m_id = id;
}

BAPIdle::~BAPIdle()
{
    if (m_condition)
        delete m_condition;
}

bool BAPIdle::execute(Bot* bot, bool& finished, bool& failed)
{
    finished = true;
    failed = m_condition->evaluate(bot);
    return true;
}

BAPStatementType BAPIdle::getType() { return BAPSTIdle; }
bool BAPIdle::isComposite() { return true; }
void BAPIdle::getDebugString(Bot*, std::string& out) { out = "Idling..."; }

// ---- BAPTriggerContinously ------------------------------------------------------------------

BAPTriggerContinously::BAPTriggerContinously(int input, BAPCondition* press, BAPCondition* hold,
                                             BAPCondition* stop, unsigned long long inputMask,
                                             int id, double delay)
    : m_input(input), m_press(press), m_hold(hold), m_stop(stop), m_pressed(false)
{
    press->m_owner = this;
    if (m_hold)
        m_hold->m_owner = this;
    if (m_stop)
        m_stop->m_owner = this;
    m_id = id;
    m_inputMask = inputMask;
    m_startTime =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + delay;
}

BAPTriggerContinously::~BAPTriggerContinously()
{
    if (m_press)
        delete m_press;
    if (m_hold)
        delete m_hold;
    if (m_stop)
        delete m_stop;
}

bool BAPTriggerContinously::execute(Bot*, bool& finished, bool& failed)
{
    finished = false;
    failed = true;
    return false;
}

BAPStatementType BAPTriggerContinously::getType() { return BAPSTTriggerContinously; }
bool BAPTriggerContinously::isComposite() { return false; }

// ---- BAPLookAtWrapper -----------------------------------------------------------------------

namespace {

const float kLeadMuzzleSpeed = 20.0f;       // PE 1027E550: assumed speed without a projectile
const float kLeadMaxTime = 2.5f;            // PE 1027B224
const float kLeadSoldierLift = 0.05f;       // PE 1027B218: per metre of distance

// PE 1006EEC0, the conditional Vec3 normaliser (file-local copies live in other TUs too).
int normaliseVector(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return 0;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f)) {
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

} // anonymous namespace

BAPLookAtWrapper::BAPLookAtWrapper(unsigned int target, BotScriptValue<Vec3>* position,
                                   bool succeed, bool lead, BotScriptValue<Vec3>* direction,
                                   BotScriptValue<bool>* found)
    : m_target(target), m_position(position), m_direction(direction), m_found(found),
      m_succeed(succeed), m_lead(lead)
{
    m_fixedPosition.x = m_fixedPosition.y = m_fixedPosition.z = 0.0f;
}

BAPLookAtWrapper::~BAPLookAtWrapper() {}

bool BAPLookAtWrapper::execute(Bot* bot, bool& finished, bool& failed)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (m_target == static_cast<unsigned int>(-1)) {
        finished = true;
        failed = !m_succeed;
        m_position->m_value = m_fixedPosition;
        return true;
    }
    Information* const target = informationManager->getInformation(m_target);
    if (!target) {
        finished = false;
        failed = true;
        return true;
    }
    finished = true;
    failed = !m_succeed;
    if (m_found)
        m_found->m_value = true;

    const SensedObject* const sensed =
        static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(m_target));
    if (!sensed)
        return true;
    Vec3 point = {0.0f, 0.0f, 0.0f};
    transformPoint(target->getTransformation(), sensed->m_senseNormal, point);

    // The equipment is not checked on either path, as shipped.
    if (!m_lead) {
        m_position->m_value = point;
        if (m_direction) {
            const Vec3& own = equipment->getPosition();
            Vec3 toward = {point.x - own.x, point.y - own.y, point.z - own.z};
            normaliseVector(toward);
            m_direction->m_value = toward;
        }
        return true;
    }

    IPIMobile* const targetMobile = target->getMobile();
    IPIArmament* const armament = equipment->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    const float* const muzzle = weapon->getProjectileOrigin().m + 12;
    Vec3 projectileSpeed = {0.0f, 0.0f, 0.0f};
    const bool hasSpeed = armament->getProjectileSpeed(projectileSpeed);
    Vec3 projectilePosition = {0.0f, 0.0f, 0.0f};
    armament->getProjectilePosition(projectilePosition);

    // Measured from the projectile when one is in flight, otherwise from the muzzle.
    const float speedLength = std::sqrt(projectileSpeed.z * projectileSpeed.z +
                                        projectileSpeed.y * projectileSpeed.y +
                                        projectileSpeed.x * projectileSpeed.x);
    Vec3 from;
    if (speedLength == 0.0f || !hasSpeed) {
        from.x = muzzle[0];
        from.y = muzzle[1];
        from.z = muzzle[2];
    } else {
        from = projectilePosition;
    }
    Vec3 relative = {point.x - from.x, point.y - from.y, point.z - from.z};

    const float* const forward = equipment->getControlInfo()->getCameraTransformation().m + 8;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;
    if (targetMobile) {
        const Vec3& velocity = targetMobile->getSpeed();
        targetX = velocity.x;
        targetY = velocity.y;
        targetZ = velocity.z;
    }
    if (speedLength == 0.0f) {
        projectileSpeed.x = forward[0] * kLeadMuzzleSpeed;
        projectileSpeed.y = forward[1] * kLeadMuzzleSpeed;
        projectileSpeed.z = forward[2] * kLeadMuzzleSpeed;
    }

    // Lead horizontally by the target's velocity over the closing time, when short enough.
    const float rx = projectileSpeed.x - targetX;
    const float ry = projectileSpeed.y - targetY;
    const float rz = projectileSpeed.z - targetZ;
    const float distance = std::sqrt(relative.z * relative.z + relative.y * relative.y +
                                     relative.x * relative.x);
    const float time = distance / std::sqrt(rz * rz + ry * ry + rx * rx);
    if (!(time >= kLeadMaxTime)) {
        relative.x += targetX * time;
        relative.y += 0.0f;
        relative.z += targetZ * time;
    }

    const float* const camera = equipment->getControlInfo()->getCameraTransformation().m + 12;
    if ((equipment->getObjectTypeFlags() >> 21) & 1)
        relative.y = distance * kLeadSoldierLift + relative.y;
    const Vec3 aimed = {relative.x + camera[0], relative.y + camera[1], relative.z + camera[2]};
    m_position->m_value = aimed;
    if (m_direction) {
        const Vec3& own = equipment->getPosition();
        Vec3 toward = {m_position->m_value.x - own.x, m_position->m_value.y - own.y,
                       m_position->m_value.z - own.z};
        normaliseVector(toward);
        m_direction->m_value = toward;
    }
    return true;
}

BAPStatementType BAPLookAtWrapper::getType() { return BAPSTLookAtWrapper; }
bool BAPLookAtWrapper::isComposite() { return true; }

// ---- BAPAimingWrapper -----------------------------------------------------------------------

namespace {

const float kShortRange = 10.0f;            // PE 1027B228
const float kHighAbove = 30.0f;             // PE 1027EAE0
const float kSteepAbove = 0.6f;             // PE 1027F83C
const float kGroundDrop = 2.0f;             // PE 1027BECC
const float kPointTolerance = 1.1920929e-07f; // PE 1027C5A0
const double kRecentSighting = 5.0;         // PE 1027E4D0
const double kLeadDelay = 1.0;              // PE 1027C6E0
const double kLeadStretch = 0.05;           // PE 102ADA10
const double kGrenadeRecent = 8.0;          // PE 102843A8
const double kLeadCapRecent = 0.5;          // PE 1027BF18
const double kLeadCapLate = 2.0;            // PE 1027BDE0

// PE 100D2910: equal within the tolerance in every component.
bool nearlyEqual(const Vec3& a, const Vec3& b, float tolerance)
{
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance
        && std::fabs(a.z - b.z) <= tolerance;
}

} // anonymous namespace

BAPAimingWrapper::BAPAimingWrapper(BotScriptValue<Vec3>* direction, BotScriptValue<bool>* valid,
                                   BotScriptValue<Vec3>* position, unsigned int equipment,
                                   unsigned int target, bool succeed)
    : m_direction(direction), m_position(position), m_valid(valid), m_equipment(equipment),
      m_target(target), m_succeed(succeed), m_groundHeight(0.0f)
{
    m_groundPoint.x = m_groundPoint.y = m_groundPoint.z = 0.0f;
}

BAPAimingWrapper::~BAPAimingWrapper() {}

bool BAPAimingWrapper::groundHeight(Bot*, const Vec3& point, float& height)
{
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    if (nearlyEqual(m_groundPoint, point, kPointTolerance)) {
        height = m_groundHeight;
        return true;
    }
    std::vector<IAIObject*> ignore;
    ignore.push_back(target->getAIObject());
    IgnoreListPredicate filter(ignore);
    const Vec3 below = {point.x, point.y - kGroundDrop, point.z};
    Vec3 hit = {0.0f, 0.0f, 0.0f};
    if (!static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
             ->collideLineWithWorld(point, below, &filter, &hit))
        return false;
    height = hit.y;
    m_groundPoint = point;
    m_groundHeight = height;
    return true;
}

bool BAPAimingWrapper::execute(Bot* bot, bool& finished, bool& failed)
{
    bot->getSensingAgent()->setOverride(true);
    Information* const own = informationManager->getInformation(m_equipment);
    if (!own || !own->getControlInfo() || !own->getArmament()) {
        finished = false;
        failed = true;
        return true;
    }
    IPIArmament* const armament = own->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    Information* const target = weapon ? informationManager->getInformation(m_target) : 0;
    if (!weapon || !target) {
        finished = false;
        failed = true;
        return true;
    }
    // Neither primary-object lookup is checked, as shipped.
    IPIMobile* const targetMobile =
        informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->getMobile();
    IPIMobile* const ownMobile =
        informationManager->getInformation(own->getPrimaryObject(bot->getSide()))->getMobile();

    m_solver.setDrag(weapon->getDrag());
    m_solver.setSpeed(weapon->getExitVelocity());
    const float gravity = static_cast<const IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();
    m_solver.setGravity(weapon->getGravityModifier() * gravity);
    m_solver.setFixedTime(weapon->getIsThrown());
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double now = timer->getGameTime();

    const bool ownSoldier = (own->getObjectTypeFlags() >> 21) & 1;
    const bool targetSoldier = (target->getObjectTypeFlags() >> 21) & 1;
    float heightBias = 0.0f;
    if (ownSoldier && targetSoldier) {
        if (weapon->getMaxRange() < kShortRange)
            heightBias = -0.2f;
        else if (target->getSoldierPose() == 1)
            heightBias = -0.3f;
    }

    const SensedObject* const sensed =
        static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(m_target));
    if (!sensed)
        return true;
    Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
    transformPoint(target->getTransformation(), sensed->m_senseNormal, aimPoint);
    const double sensedTime = timer->getGameTime();
    const Vec3& ownPosition = own->getPosition();
    Vec3 relative = {aimPoint.x - ownPosition.x, aimPoint.y - ownPosition.y,
                     aimPoint.z - ownPosition.z};
    const float rise = relative.y;
    const float length = std::sqrt(relative.z * relative.z + relative.y * relative.y +
                                   relative.x * relative.x);
    if (length != 0.0f) {
        const float inverse = 1.0f / length;
        relative.x *= inverse;
        relative.y *= inverse;
        relative.z *= inverse;
    }

    // Splash, unguided, and (for the second) not thrown.
    bool splash = false;
    bool lobbable = false;
    if (weapon->getExplosionRadius() != 0.0f && weapon->getWeaponGuidanceType() == 0) {
        splash = true;
        lobbable = !weapon->getIsThrown();
    }
    const bool vehicleOnSoldier = !ownSoldier && targetSoldier && lobbable;
    const bool high = bot->getSpawnKit() != 1 && !weapon->getIsThrown() && rise > kHighAbove;
    const unsigned int flags = sensed->m_senseFlags;
    const bool reported = (flags & 2) && ((flags & 8) || high || vehicleOnSoldier) && splash;
    const bool failedSince =
        static_cast<BotMain*>(bot)->getTimeOfLastFailedStaticSkyAttack() > sensed->m_positionTime;
    const bool simulate = reported && !failedSince && !vehicleOnSoldier
        && (!lobbable || rise > kShortRange || relative.y > kSteepAbove);
    m_solver.setNoCorrection(reported && !simulate);

    if (simulate) {
        if (targetMobile) {
            if ((flags & 2) && sensedTime - sensed->m_lastUpdate < kRecentSighting)
                aimPoint = sensed->m_position;
            else
                aimPoint = sensed->m_lastPosition;
            double elapsed = now - sensed->m_positionTime;
            if (elapsed > kLeadDelay) {
                elapsed = (elapsed - kLeadDelay) * kLeadStretch + kLeadDelay;
                const double cap = sensedTime - bot->getTimeOfGrenadeThrown(0) < kGrenadeRecent
                    ? kLeadCapRecent : kLeadCapLate;
                elapsed = elapsed >= cap ? cap : elapsed;
            }
            const Vec3& velocity = targetMobile->getSpeed();
            const float scale = static_cast<float>(elapsed);
            aimPoint.x += velocity.x * scale;
            aimPoint.y += velocity.y * scale;
            aimPoint.z += velocity.z * scale;
        } else {
            transformPoint(target->getTransformation(), sensed->m_senseNormal, aimPoint);
        }
    } else if (reported) {
        aimPoint = sensed->m_lastPosition;
        float ground;
        if (groundHeight(bot, aimPoint, ground))
            aimPoint.y = ground;
    } else {
        if ((flags & 2) && !(flags & 8))
            transformPoint(target->getTransformation(), sensed->m_senseNormal, aimPoint);
        aimPoint.y += heightBias;
    }

    if (splash && !weapon->getIsThrown() && !reported && targetSoldier)
        aimPoint.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                         ->getHeight(aimPoint.x, aimPoint.z);
    if (m_position)
        m_position->m_value = aimPoint;

    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3 toTarget = {aimPoint.x - origin.m[12], aimPoint.y - origin.m[13],
                           aimPoint.z - origin.m[14]};
    Vec3 targetVelocity = {0.0f, 0.0f, 0.0f};
    Vec3 ownVelocity = {0.0f, 0.0f, 0.0f};
    if (ownMobile)
        ownVelocity = ownMobile->getSpeed();
    if (targetMobile && !reported)
        targetVelocity = targetMobile->getSpeed();
    m_solver.setTarget(toTarget, targetVelocity, ownVelocity, &origin, now);

    Vec3 direction = {0.0f, 0.0f, 0.0f};
    Vec3 work = {0.0f, 0.0f, 0.0f};
    BotMain* const main = static_cast<BotMain*>(bot);
    bool valid;
    if (simulate) {
        main->setBotWantsToSimulateArtillery(true);
        valid = m_solver.lobDirection(direction, work);
    } else {
        main->setBotWantsToSimulateArtillery(false);
        valid = m_solver.solve(now, 0.5f, weapon->getIndirect(), direction, work, 0);
    }
    failed = !m_succeed;
    finished = true;
    m_direction->m_value = direction;
    m_valid->m_value = valid;
    return true;
}

BAPStatementType BAPAimingWrapper::getType() { return BAPSTAimingWrapper; }
bool BAPAimingWrapper::isComposite() { return true; }

// ---- BAPFireWrapper -------------------------------------------------------------------------

BAPFireWrapper::BAPFireWrapper(Bot* bot, unsigned int target, bool flag2C, bool flag2D,
                               int weapon, int int28, BotScript* child, int id)
    : BAPWrapper(child, id), m_target(target), m_weapon(weapon), m_int28(int28), m_flag2C(flag2C),
      m_flag2D(flag2D), m_int30(-1), m_bot(bot)
{
    m_child->m_owner = this;
    bot->setIsFiring(true);
    bot->setFiringTarget(target);
    m_useStaticAttack = static_cast<BotMain*>(bot)->getUseStaticAttack();
}

BAPFireWrapper::~BAPFireWrapper()
{
    if (m_child)
        delete m_child;
    BotMain* const bot = static_cast<BotMain*>(m_bot);
    bot->setIsFiring(false);
    bot->setBridgeRepairTarget(static_cast<unsigned int>(-1));
    bot->setBotIsSimulatingArtillery(false);
    bot->setRepairTarget(static_cast<unsigned int>(-1));
    bot->setIsGoingForFlag(false);
    bot->setFiringTarget(static_cast<unsigned int>(-1));
    if (m_target != static_cast<unsigned int>(-1)) {
        Information* const target = informationManager->getInformation(m_target);
        if (target) {
            Bot* const targetBot = IBotManager::instance->getBotFromPlayerId(target->getPlayerId());
            if (targetBot)
                static_cast<BotMain*>(targetBot)->setIsEngineerTarget(false);
        }
    }
}

int BAPFireWrapper::getWrapperIndex() { return 0; }

namespace {

// The horizontal part (x, z) of a position. PE 1014ADC0.
Vec2 runOverFlat(const Vec3& position)
{
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;
    return flat;
}

// The conditional Vec2 normaliser. PE 100E9DE0.
void runOverNormalise(Vec2& value)
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

// The vector a quarter turn from a flat direction: (y, -x). PE 1014AE20.
Vec2 runOverPerpendicular(const Vec2& value)
{
    Vec2 out;
    out.x = value.y;
    out.y = -value.x;
    return out;
}

// 1, -1 or 0 by the sign. PE 100DE7B0.
float runOverSign(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (value < 0.0f)
        return -1.0f;
    return 0.0f;
}

// Clamps to [low, high]; an unordered value yields low. PE 1006ED90.
float runOverClamp(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    if (value >= low)
        return value;
    return low;
}

// The signed angle of a flat target against a forward direction and its right-hand
// perpendicular, in (-pi, pi]. The target is not normalised; the sine is clamped twice, as
// shipped. PE 1014B730; [INFERRED] name.
float signedFlatAngle(const Vec2& forward, const Vec2& right, const Vec2& target)
{
    float sine = right.y * target.y + right.x * target.x;
    if (-1.0f > sine)
        sine = -1.0f;
    else if (!(1.0f >= sine))
        sine = 1.0f;
    float angle = static_cast<float>(1.5707963705062866
                                     - std::acos(static_cast<double>(runOverClamp(sine, -1.0f, 1.0f))));
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

} // anonymous namespace

// Steers and throttles a car (PE 101CD1B0; [INFERRED] name). The steering angle turns the wheel
// at three times its size, clamped to full lock. With a zero throttle the car turns in place and
// the throttle input gets that zero; otherwise the target speed (cruise times throttle) eases off
// while the car rolls or pitches faster than 0.15, is capped to maxSpeedFactor in size, and the
// throttle chases it. A nonzero cruise speed wakes the vehicle's physics on both sides. The
// equipment's flat axes and maximum speed are read and dropped, as shipped. Always true.
bool driveCar(Information* equipment, IPIControlInfo* control, IPIMobile* mobile, IPIPhysical* /*physical*/,
              io::PlayerInput* input, float cruise, float throttle, float steering, float speedWeight,
              float maxSpeedFactor)
{
    const Mat4& transformation = equipment->getTransformation();
    const Vec2 forward = runOverFlat(*reinterpret_cast<const Vec3*>(&transformation.m[8]));
    const Vec2 right = runOverFlat(*reinterpret_cast<const Vec3*>(&transformation.m[0]));
    const Vec2 origin = runOverFlat(*reinterpret_cast<const Vec3*>(&transformation.m[12]));
    (void)forward;
    (void)right;
    (void)origin;
    mobile->getMaxSpeed();
    const Vec2 velocity = runOverFlat(mobile->getSpeed());
    const float speed = std::sqrt(velocity.y * velocity.y + velocity.x * velocity.x);
    if (throttle != 0.0f && speedWeight != throttle && speed > 1.0f)
        steering = steering * -1.0f;
    if (cruise != 0.0f)
    {
        IAIObjectPhysical* const physics = equipment->getAIObject()->getPhysical();
        physics->enablePhysics(1);
        physics->enablePhysics(2);
    }
    const float direction = steering > 0.0f ? 1.0f : (steering < 0.0f ? -1.0f : 0.0f);
    if (throttle == 0.0f)
    {
        float size = std::fabs(steering);
        if (throttle == -1.0f)
            size = size - 3.1415927410125732f;
        float lock = std::fabs(size * 3.0f);
        if (!(1.0f >= lock))
            lock = 1.0f;
        input->setInput(control->getDriveTurnControl(), -(lock * direction));
        input->setInput(control->getDriveThrottleControl(), throttle);
        return true;
    }
    float wheel = steering * 3.0f;
    if (-1.0f > wheel)
        wheel = -1.0f;
    else if (!(1.0f >= wheel))
        wheel = 1.0f;
    input->setInput(control->getDriveTurnControl(), wheel);
    float target = cruise * throttle;
    const Vec3& rotation = mobile->getRotationalSpeed();
    const float rollRate = std::fabs(rotation.x);
    const float pitchRate = std::fabs(rotation.z);
    const float turning = rollRate >= pitchRate ? rollRate : pitchRate;
    if (turning > 0.15f)
    {
        float ease = (turning - 0.15f) * 1.4285714626312256f;
        if (!(1.0f >= ease))
            ease = 1.0f;
        target = (1.0f - ease * 0.8f) * target;
    }
    if (std::fabs(target) > maxSpeedFactor)
        target = runOverSign(target) * maxSpeedFactor;
    float chase = (target - speed * speedWeight) * 0.3333333432674408f;
    if (!(1.0f >= chase))
        chase = 1.0f;
    float pedal = runOverClamp(target * 0.06666667014360428f + chase, -1.0f, 1.0f);
    if (pedal != 0.0f && std::fabs(speed) < 1.5f)
        pedal = runOverSign(pedal);
    input->setInput(control->getDriveThrottleControl(), pedal);
    return true;
}


BAPRunOverObject::BAPRunOverObject(unsigned int target, unsigned long long inputMask, int id)
    : m_target(target)
    , m_driveControls(inputMask)
{
    m_id = id;
}

BAPRunOverObject::~BAPRunOverObject() {}

// PE 101C1FF0. Each tick traces a vehicle path to the target; with one, it predicts where the
// target will be (twice, each time from the time the car needs at its current speed) and steers
// at that point at cruise 50. The speed is divided without a zero check, as shipped.
bool BAPRunOverObject::execute(Bot* bot, bool& finished, bool& failed)
{
    finished = true;
    failed = false;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return false;
    IPIPhysical* const physical = equipment->getPhysical();
    IPIMobile* const mobile = equipment->getMobile();
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
    {
        failed = true;
        return true;
    }
    IPIMobile* const targetMobile = target->getMobile();
    const IPIMobile::ValidPos& targetValid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
    const IPIMobile::ValidPos& ownValid = mobile->getLastValidPosition();
    Vec3 hit;
    hit.x = hit.y = hit.z = 0.0f;
    unsigned int hitHandle;
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    if (!IAIPathfinding::instance->trace(ownValid.polygon, ownValid.quadTree, mobile->getVehicleNumber(),
                                         ownValid.position, targetValid.position, hit, hitHandle,
                                         equipmentType, -1, 0.0f, 5, 0))
    {
        failed = true;
        return true;
    }

    const Vec3& position = equipment->getPosition();
    const Vec3& targetPosition = target->getPosition();
    Vec3 toTarget;
    toTarget.x = targetPosition.x - position.x;
    toTarget.y = targetPosition.y - position.y;
    toTarget.z = targetPosition.z - position.z;
    const float distanceSquared = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
    const float inverseSpeed = 1.0f / mobile->getSpeedMagnitude();
    float time = std::sqrt(distanceSquared) * inverseSpeed;
    const Vec3& velocity = targetMobile->getSpeed();
    Vec3 predicted;
    predicted.x = time * velocity.x + targetPosition.x;
    predicted.y = time * velocity.y + targetPosition.y;
    predicted.z = time * velocity.z + targetPosition.z;
    toTarget.x = predicted.x - position.x;
    toTarget.y = predicted.y - position.y;
    toTarget.z = predicted.z - position.z;
    time = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z) * inverseSpeed;
    const Vec3& velocityAgain = targetMobile->getSpeed();
    predicted.x = time * velocityAgain.x + targetPosition.x;
    predicted.y = time * velocityAgain.y + targetPosition.y;
    predicted.z = time * velocityAgain.z + targetPosition.z;
    Vec3 aim;
    aim.x = predicted.x - position.x;
    aim.y = predicted.y - position.y;
    aim.z = predicted.z - position.z;

    const Vec2 flatAim = runOverFlat(aim);
    Vec2 facing = runOverFlat(equipment->getFaceing());
    runOverNormalise(facing);
    const Vec2 right = runOverPerpendicular(facing);
    const float steering = signedFlatAngle(facing, right, flatAim);
    finished = driveCar(equipment, equipment->getControlInfo(), mobile, physical,
                        static_cast<BotMain*>(bot)->playerInput(), 50.0f, 1.0f, steering, 1.0f, 1.0f);
    return true;
}

BAPStatementType BAPRunOverObject::getType() { return BAPSTRunOverObject; }
bool BAPRunOverObject::isComposite() { return false; }

} // namespace ai
} // namespace hfe
} // namespace dice
