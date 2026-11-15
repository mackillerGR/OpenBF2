// Attested placement: SensingAgent.cpp (PE __FILE__ string).

#include "SensingAgent.h"
#include "BotMain.h"
#include "../AIBotActionPlan/BotScript.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"

#include "BotSenses.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "SensingAgentPatterns.h"           // ISensingAgentPattern::reset - the shared patterns
#include "../Main/AIRandom.h"               // the per-life reaction time
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"   // IBFTimer::getGameTime
#include <map>
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Core/Debug.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// Creates sensing state and an eight-sector attention vector for the bot.
SensingAgent::SensingAgent(BotMain* owner, unsigned char /*behaviourCount*/)
    : m_flag134(false)
    , m_plan(0)
{
    m_focusPosition.x = m_focusPosition.y = m_focusPosition.z = 0.0f;
    m_planTargetDirection.x = m_planTargetDirection.y = m_planTargetDirection.z = 0.0f;
    m_lookDirection.x = m_lookDirection.y = 0.0f;
    for (int i = 0; i < 3; ++i)
        m_zero88[i] = 0;                   // +88..+96

    // the mechanism this agent steers. , and it takes the bot rather than the agent
    // so `BotSenses` answers to the bot directly and this object is a policy layer over it.
    m_senses = checkPointer(new BotSenses(owner));
    m_bot    = owner;

    m_bapContext = checkPointer(new BAPContext());

    // eight quadrants, zeroed. A live resize; see the block above.
    m_quadrantWeights.resize(8, 0.0f);

    // the last statement, and it is of its own. A sensing agent is only half built
    // until `reset` has run - the same constructor/reset split `BotMain` uses.
    reset();
}

// [FRONTIER] Remaining sensing-plan operations require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

SensingAgent::~SensingAgent()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SensingAgent::~SensingAgent");
}

// Deletes the owned plan and marks planning state for refresh.
void SensingAgent::deletePlan()
{
    if (m_plan)
    {
        delete m_plan;
        m_plan = 0;
    }
    m_flag140 = true;
}

// Clears the current plan's target information.
void SensingAgent::clearPlanTargetInformation()
{
    m_planTarget60 = 0;
    m_planTargetWeight = 0.0f;
}

// Resets sensing state and draws a new reaction delay in [0.5, 1.0).
void SensingAgent::reset()
{
    m_flag120 = false;
    m_flag121 = false;
    m_double112 = 0.0;
    m_flag122 = false;
    m_flag132 = false;
    m_flag140 = true;
    m_flag133 = false;

    // the shared patterns, RE-indexed. See the block above.
    IAISettings* const settings =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const std::map<int, ISensingAgentPattern*>& patterns = settings->getSensingAgentPatterns();

    int index = 0;
    for (std::map<int, ISensingAgentPattern*>::const_iterator it = patterns.begin();
         it != patterns.end(); ++it)
    {
        it->second->reset(index++);
    }

    deletePlan();

    m_int24  = -1;
    m_focusWeight = 0.0f;
    m_flag44 = false;
    m_flag148 = true;
    m_lookDirection.x = 0.0f;              // +80
    m_lookDirection.y = 0.0f;              // +84

    // through the buffer, and the size is not touched. See the block above.
    for (unsigned q = 0; q <= 7; ++q)
        m_quadrantWeights[q] = 0.0f;

    clearPlanTargetInformation();

    // game time, not wall-clock.
    m_double112 =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_double124 = 0.0;

    // Chooses a reaction time in [0.5, 1.0), preserving the arithmetic order.
    m_reactionTime = 0.5f + AIRandom::m_instance->nextFloat() * (1.0f - 0.5f);

    m_senses->reset();
}

// Ignores the override while m_flag132 is set. Otherwise stores the requested value
// and the current game time. reset clears the refusal flag.
void SensingAgent::setOverride(bool value)
{
    // The refusal. See the block above.
    if (!m_flag132)
    {
        m_flag122 = value;
        m_double124 =
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    }
}

// PE 101AE240.
void SensingAgent::setOverrideLock(bool locked)
{
    m_flag132 = locked;
    m_flag122 = false;
}

// PE 101AE340. A failed or unfinished run marks the plan stale and clears its targets.
void SensingAgent::executePlan()
{
    bool finished = false;
    bool failed = false;
    m_bot->getPlanInterpreter()->execute(m_plan, m_bot, finished, failed);
    m_flag140 = failed || !finished;
    if (m_flag140)
        clearPlanTargetInformation();
}

namespace {

// PE 1006ED90: clamps to [low, high]; an unordered value yields low.
float clampToRange(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// The inlined pre-clamp before acos: below -1 (or unordered) gives -1, above 1 gives 1.
float clampCosine(float cosine)
{
    if (!(-1.0f <= cosine))
        return -1.0f;
    if (!(1.0f >= cosine))
        return 1.0f;
    return cosine;
}

// PE 100E9DE0, the Vec2 normaliser (see SquadFormation.cpp for the same file-local copy).
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

// PE 1006EEC0, the Vec3 normaliser of the same shape.
int normalise(Vec3& value)
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

// PE 1012F8C0: rotates a row vector by the upper 3x3 of the matrix, in place.
void rotateVector(const Mat4& m, Vec3& v)
{
    const float x = v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8];
    const float y = v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9];
    const float z = v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10];
    v.x = x;
    v.y = y;
    v.z = z;
}

float lengthSquared(const Vec2& v)
{
    return v.x * v.x + v.y * v.y;
}

const float kHalfSqrt2 = 0.7071067690849304f;

} // anonymous namespace

// PE 101AE0A0. The eight sector directions: the four axes, then the four diagonals.
void SensingAgent::getQuadrantDirection(int quadrant, Vec2& direction)
{
    switch (quadrant)
    {
    case 0: direction.x = 0.0f;         direction.y = 1.0f;         return;
    case 1: direction.x = 1.0f;         direction.y = 0.0f;         return;
    case 2: direction.x = 0.0f;         direction.y = -1.0f;        return;
    case 3: direction.x = -1.0f;        direction.y = 0.0f;         return;
    case 4: direction.x = kHalfSqrt2;   direction.y = kHalfSqrt2;   return;
    case 5: direction.x = kHalfSqrt2;   direction.y = -kHalfSqrt2;  return;
    case 6: direction.x = -kHalfSqrt2;  direction.y = -kHalfSqrt2;  return;
    case 7: direction.x = -kHalfSqrt2;  direction.y = kHalfSqrt2;   return;
    default:
        Debug(DebugTypeError, __FILE__, 770, "") << "Bad quadrant value " << quadrant;
        return;
    }
}

// PE 101AE7E0. Whether a direction lies within 20 degrees of the sector's centre line. The
// direction is not normalised first.
bool SensingAgent::isInQuadrant(int quadrant, const Vec2& direction)
{
    if (!(lengthSquared(direction) >= 0.01f) || quadrant < 0)
        return false;
    Vec2 centre = {};
    getQuadrantDirection(quadrant, centre);
    const float cosine = clampCosine(centre.x * direction.x + centre.y * direction.y);
    return std::acos(clampToRange(cosine, -1.0f, 1.0f)) < 0.01745329238474369f * 20.0f;
}

// PE 101AE8C0. As above within 90 degrees.
bool SensingAgent::isInQuadrantHalf(int quadrant, const Vec2& direction)
{
    if (!(lengthSquared(direction) >= 0.01f) || quadrant < 0)
        return false;
    Vec2 centre = {};
    getQuadrantDirection(quadrant, centre);
    const float cosine = clampCosine(centre.x * direction.x + centre.y * direction.y);
    return std::acos(clampToRange(cosine, -1.0f, 1.0f)) < 0.01745329238474369f * 90.0f;
}

// PE 101AE9A0. Whether the seat's camera may turn toward a direction: the forward of the bot's
// vehicle, turned by the seat's rotation offset, against the seat's minimum and maximum rotation.
// Seats without limits allow every direction.
bool SensingAgent::isDirectionAllowed(const Vec2& direction)
{
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    IPIControlInfo* const control = equipment->getControlInfo();
    const float offset = control->getCameraRelativeDofRotationOffset();
    const Mat4 transform(m_bot->getPlayer()->getCurrentVehicle()->getTransformation());
    Vec2 forward;
    if (offset > 0.0f)
    {
        Mat4 rotation = Mat4::getIdentity();
        setRotateYDeg(rotation, offset * 57.2957763671875f);
        const float y = transform.m[9] * rotation.m[6] + transform.m[8] * rotation.m[2] +
                        transform.m[10] * rotation.m[10];
        forward.x = transform.m[9] * rotation.m[4] + transform.m[10] * rotation.m[8] +
                    transform.m[8] * rotation.m[0];
        forward.y = y;
    }
    else
    {
        forward.x = transform.m[8];
        forward.y = transform.m[10];
    }
    normalise(forward);
    const float cosine = clampCosine(forward.y * direction.y + forward.x * direction.x);
    const float angle = std::acos(clampToRange(cosine, -1.0f, 1.0f));
    const Vec3& maximum = control->getCameraRelativeMaxRotation();
    const Vec3& minimum = control->getCameraRelativeMinRotation();
    if (maximum.x == 0.0f && minimum.x == 0.0f)
        return true;
    if (forward.y * direction.x - forward.x * direction.y > 0.0f)
        return angle <= maximum.x;
    return !(-angle < minimum.x);
}

// PE 101AEBD0. The two sectors a position falls in seen from the bot's camera: one axis sector
// and one diagonal sector. False when the position is on top of the camera.
bool SensingAgent::getQuadrantsOf(const Vec3& position, int quadrants[2])
{
    const Mat4& camera =
        *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    Vec2 offset;
    offset.x = position.x - camera.m[12];
    offset.y = position.z - camera.m[14];
    if (normalise(offset) != 0)
        return false;

    const float scaled = offset.x * kHalfSqrt2;
    const float across = offset.y * -kHalfSqrt2 + scaled;
    const float along = offset.y * kHalfSqrt2 + scaled;
    if (!(along < 0.0f))
        quadrants[0] = !(across < 0.0f) ? 1 : 0;
    else
        quadrants[0] = !(across < 0.0f) ? 2 : 3;

    const float x = offset.y * 0.0f + offset.x;
    const float y = offset.x * 0.0f + offset.y;
    if (!(y < 0.0f))
        quadrants[1] = !(x < 0.0f) ? 4 : 7;
    else
        quadrants[1] = !(x < 0.0f) ? 5 : 6;
    return true;
}

// PE 101AF380. A sector, or with a negative index the plan target's position taken as a
// direction (its x and z, as shipped), checked against the seat's turning limits.
bool SensingAgent::isQuadrantAllowed(int quadrant)
{
    Vec2 direction = {};
    if (quadrant >= 0)
    {
        getQuadrantDirection(quadrant, direction);
    }
    else
    {
        Information* const target = informationManager->getInformation(m_planTarget60);
        if (!target)
            return false;
        const Vec3& position = target->getPosition();
        direction.x = position.x;
        direction.y = position.z;
    }
    return isDirectionAllowed(direction);
}

// PE 101AF410. Draws the delay before the next look: short for borrowed or special equipment
// and while the aim is still, longer while the bot moves or turns, longest when what it looks at
// lies in the sector it already faces.
void SensingAgent::updateReactionTime(const FocusChoice& focus, Information* equipment,
                                      const Vec2& lookDirection)
{
    io::PlayerInput* const input = m_bot->m_playerInput;
    if (!equipment->getUnit())
        return;
    const unsigned int flags = equipment->getObjectTypeFlags();
    if (equipment->getUnit()->mustBorrowArmament() || ((flags >> 5) & 1) || ((flags >> 4) & 1))
    {
        m_reactionTime = static_cast<float>(
            static_cast<double>(AIRandom::m_instance->next()) * 1.1641532182693481e-10 + 1.0);
        return;
    }
    if (!(lengthSquared(lookDirection) >= 0.01f))
    {
        m_reactionTime = AIRandom::m_instance->nextRange(1.5f, 2.0f);
        return;
    }
    const unsigned long long mapped = input->mappingFlags;
    const bool moving = ((mapped & (1ULL << io::PIThrottle)) && !(input->inputs[io::PIThrottle] < 0.01f)) ||
                        ((mapped & (1ULL << io::PIYaw)) && !(input->inputs[io::PIYaw] < 0.01f));
    if (!moving)
    {
        if ((flags >> 21) & 1)
            m_reactionTime = AIRandom::m_instance->nextRange(3.0f, 4.0f);
        else
            m_reactionTime = AIRandom::m_instance->nextRange(7.0f, 10.0f);
        return;
    }
    Vec2 back;
    back.x = -lookDirection.x;
    back.y = -lookDirection.y;
    const bool facing = isInQuadrant(focus.m_quadrant, back);
    if ((flags >> 21) & 1)
    {
        if (!facing)
        {
            m_reactionTime = AIRandom::m_instance->nextRange(1.5f, 2.0f);
            return;
        }
    }
    else if (!facing)
    {
        m_reactionTime = AIRandom::m_instance->nextRange(5.0f, 7.0f);
        return;
    }
    m_reactionTime = AIRandom::m_instance->nextRange(7.0f, 10.0f);
}

// PE 101AE290.
void SensingAgent::createPlanTowards(const Vec3& direction, ISensingAgentPattern* pattern)
{
    if (m_plan)
        m_plan->m_owner = pattern;
    m_plan = pattern->createPlan(this, m_bot, direction);
    if (m_plan)
        m_plan->m_owner = this;
}

// PE 101AF5E0. Builds the look plan. On foot it follows the plan target while that outweighs the
// focus, otherwise the chosen sector or position. In or on borrowed or special equipment it looks
// along the vehicle's camera forward, dipped for special equipment.
void SensingAgent::createPlanFromFocus(ISensingAgentPattern* pattern, const FocusChoice& focus)
{
    if (m_plan)
        m_plan->m_owner = pattern;
    BotMain* const bot = m_bot;
    Information* const equipment = informationManager->getInformation(bot->m_currentEquipment);
    const unsigned int flags = equipment->getObjectTypeFlags();
    const bool special = ((flags >> 5) & 1) || ((flags >> 4) & 1);

    if (!equipment->getUnit()->mustBorrowArmament() && !special)
    {
        if (!(lengthSquared(m_lookDirection) >= 0.01f) && m_planTargetWeight > focus.m_weight)
        {
            if (m_planTarget60 == bot->getCurrentEquipment())
            {
                Vec3 direction = m_planTargetDirection;
                normalise(direction);
                m_plan = pattern->createPlan(this, bot, direction);
            }
            else
            {
                m_plan = pattern->createPlan(this, bot, m_planTarget60);
                m_int24 = -1;
                m_focusWeight = 0.0f;
            }
            m_flag148 = false;
            m_flag133 = false;
        }
        else if (focus.m_quadrant >= 0)
        {
            Vec3 direction = {};
            const Mat4& transform = equipment->getTransformation();
            if (focus.m_position.y == 0.0f)
            {
                Vec2 centre = {};
                getQuadrantDirection(focus.m_quadrant, centre);
                direction.x = centre.x;
                direction.y = transform.m[9];
                direction.z = centre.y;
            }
            else
            {
                direction.x = focus.m_position.x - transform.m[12];
                direction.y = focus.m_position.y - transform.m[13];
                direction.z = focus.m_position.z - transform.m[14];
            }
            normalise(direction);
            m_int24 = focus.m_quadrant;
            m_focusWeight = focus.m_weight;
            m_plan = pattern->createPlan(this, bot, direction);
            m_flag148 = true;
            m_int24 = focus.m_quadrant;
            m_focusWeight = focus.m_weight;
            m_focusPosition = focus.m_position;
            m_flag133 = isInQuadrant(m_int24, m_lookDirection);
        }
        else
        {
            Debug(DebugTypeError, __FILE__, 1066, "") << "Sensing agent plan has nothing to look at!";
        }
        if (m_plan)
            m_plan->m_owner = this;
        return;
    }

    IAIObject* const vehicle = bot->getPlayer()->getCurrentVehicle();
    const float offset = equipment->getControlInfo()->getCameraRelativeDofRotationOffset();
    const Mat4 transform(vehicle->getTransformation());
    Vec3 forward;
    forward.x = transform.m[8];
    forward.y = transform.m[9];
    forward.z = transform.m[10];
    if (offset > 0.0f)
    {
        Mat4 rotation = Mat4::getIdentity();
        setRotateYDeg(rotation, offset * 57.2957763671875f);
        rotateVector(rotation, forward);
    }
    if (!special)
    {
        if (m_plan)
            m_plan->m_owner = pattern;
        m_plan = pattern->createPlan(this, bot, forward);
        if (m_plan)
            m_plan->m_owner = this;
        m_flag133 = false;
        return;
    }
    Vec3 direction;
    direction.x = forward.x;
    direction.y = ((flags >> 4) & 1) ? -0.4f : -0.7f;
    direction.z = forward.z;
    createPlanTowards(direction, pattern);
    m_flag133 = false;
}

// PE 101AFCE0. The heaviest allowed sector; failing that a random sector, redrawn up to six
// times until allowed, at a token weight.
void SensingAgent::chooseQuadrant(const std::vector<QuadrantInformation>& quadrants,
                                  FocusChoice& focus)
{
    focus.m_weight = 0.0f;
    float best = 0.0f;
    focus.m_quadrant = -1;
    int index = 0;
    for (std::vector<QuadrantInformation>::const_iterator it = quadrants.begin();
         it != quadrants.end(); ++it, ++index)
    {
        Vec2 direction;
        getQuadrantDirection(index, direction);
        if (it->m_weight > best && isQuadrantAllowed(index))
        {
            best = it->m_weight;
            focus.m_weight = it->m_weight;
            focus.m_quadrant = index;
            focus.m_position = it->m_position;
        }
    }
    if (focus.m_quadrant >= 0)
        return;

    focus.m_quadrant = static_cast<int>(std::floor(
        static_cast<float>(AIRandom::m_instance->next()) * 1.6298145055770874e-09f + 0.5f));
    unsigned int tries = 0;
    if (!isQuadrantAllowed(focus.m_quadrant))
    {
        while (tries <= 5)
        {
            focus.m_quadrant = static_cast<int>(std::floor(
                static_cast<float>(AIRandom::m_instance->next()) * 1.6298145055770874e-09f + 0.5f));
            Vec2 direction;
            getQuadrantDirection(focus.m_quadrant, direction);
            ++tries;
            if (isQuadrantAllowed(focus.m_quadrant))
                break;
        }
    }
    focus.m_weight = 0.1f;
    focus.m_position.x = 0.0f;
    focus.m_position.y = 0.0f;
    focus.m_position.z = 0.0f;
}

// PE 101AFF00. Drops the look plan when the equipment no longer suits it, the sector became
// disallowed or changed, the plan target is sensed, the bot reverses lying down, or on request.
void SensingAgent::checkPlanValidity(const FocusChoice& focus, bool force)
{
    bool invalid = false;
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    const unsigned int flags = equipment->getObjectTypeFlags();
    const bool special = ((flags >> 5) & 1) || ((flags >> 4) & 1);
    if (equipment->getUnit()->mustBorrowArmament() || special)
    {
        deletePlan();
        return;
    }
    if (!isQuadrantAllowed(m_int24))
        invalid = true;
    if (m_flag148)
    {
        if (focus.m_quadrant != m_int24)
        {
            m_flag44 = false;
            invalid = true;
        }
    }
    else if (m_bot->getSensedObjectFromHandle(m_planTarget60))
    {
        invalid = true;
    }
    if (equipment->getSoldierPose() == 3)
    {
        io::PlayerInput* const input = m_bot->m_playerInput;
        if ((input->mappingFlags & (1ULL << io::PIThrottle)) && input->inputs[io::PIThrottle] < 0.0f)
            invalid = true;
    }
    if (force)
        invalid = true;
    if (m_plan && invalid)
        deletePlan();
}

// PE 101B0050. The focus buffers keep their capacity.
void SensingAgent::clearFocus()
{
    m_planTarget60 = 0;
    m_planTargetWeight = 0.0f;
    m_sensedFocus.clear();
    m_fireFocus.clear();
}

// PE 101B01B0. Writes where the bot looks (the stored look direction reversed for special
// equipment, else the primary object's backward axis) and grows each sector's weight: by a
// little, or more when the bot looks into it (more again when reconsidering). A mobile primary
// seat zeroes the sectors behind it instead.
void SensingAgent::updateQuadrantWeights(Vec2& lookDirection, bool reconsider)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if ((equipment->getObjectTypeFlags() >> 21) & 1)
    {
        if (!(lengthSquared(m_lookDirection) >= 0.01f))
            return;
        lookDirection.x = -m_lookDirection.x;
        lookDirection.y = -m_lookDirection.y;
    }
    else
    {
        Information* const primary = informationManager->getInformation(
            equipment->getPrimaryObject(m_bot->getSide()));
        const Mat4& transform = primary->getTransformation();
        lookDirection.x = -transform.m[8];
        lookDirection.y = -transform.m[10];
    }

    const bool mobileSeat = !((equipment->getObjectTypeFlags() >> 21) & 1) &&
                            equipment->getMobile() && equipment->isPrimary();
    for (int quadrant = 0; quadrant < 8; ++quadrant)
    {
        if (mobileSeat && isInQuadrantHalf(quadrant, lookDirection))
        {
            m_quadrantWeights[quadrant] = 0.0f;
            continue;
        }
        Vec2 ahead;
        ahead.x = -lookDirection.x;
        ahead.y = -lookDirection.y;
        if (isInQuadrant(quadrant, ahead))
            m_quadrantWeights[quadrant] += reconsider ? 0.25f : 0.05f;
        else
            m_quadrantWeights[quadrant] += 0.0125f;
    }
}

// PE 101B0330. Adds every focus entry into both of its sectors, then each sector's standing
// weight, and averages each sector's positions.
void SensingAgent::sumQuadrants(std::vector<QuadrantInformation>& quadrants)
{
    const std::vector<FocusInformation>* const lists[2] = { &m_sensedFocus, &m_fireFocus };
    for (int list = 0; list < 2; ++list)
    {
        for (std::vector<FocusInformation>::const_iterator it = lists[list]->begin();
             it != lists[list]->end(); ++it)
        {
            int pair[2];
            if (!getQuadrantsOf(it->m_position, pair))
                continue;
            for (int k = 0; k < 2; ++k)
            {
                QuadrantInformation& q = quadrants[pair[k]];
                q.m_weight += it->m_weight;
                q.m_position.x += it->m_position.x;
                q.m_position.y += it->m_position.y;
                q.m_position.z += it->m_position.z;
                ++q.m_count;
            }
        }
    }
    for (int k = 0; k < 8; ++k)
    {
        QuadrantInformation& q = quadrants[k];
        q.m_weight += m_quadrantWeights[k];
        if (q.m_count > 0)
        {
            const float scale = 1.0f / static_cast<float>(q.m_count);
            q.m_position.x *= scale;
            q.m_position.y *= scale;
            q.m_position.z *= scale;
        }
    }
}

// PE 101B10D0. Every live sensed object, weighted by how it is sensed and its security.
void SensingAgent::collectSensedFocus()
{
    m_sensedFocus.resize(0);
    const std::map<unsigned int, SensedObject*>& sensed = m_senses->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin();
         it != sensed.end(); ++it)
    {
        SensedObject* const object = it->second;
        Information* const information = informationManager->getInformation(object->getHandle());
        if (!information)
            continue;
        if (information->getPhysical() && !(information->getPhysical()->getHealth() > 0.0f))
            continue;
        const unsigned int flags = object->getSenseFlags();
        float weight = 0.0f;
        if (flags & 2)
        {
            if (!(flags & 8))
                weight = 0.5f;
            else if (flags & 0x20)
                weight = 0.25f;
            else if (flags & 1)
                weight = 0.25f;
        }
        else
        {
            if (flags & 1)
                weight = 0.3f;
            if (flags & 0x20)
                weight += 0.25f;
        }
        FocusInformation entry;
        entry.m_weight = information->getSecurity() * weight;
        entry.m_position = information->getPosition();
        m_sensedFocus.push_back(entry);
    }
}

// PE 101B1220. Every incoming shot: from an enemy, at the shooter (0.7 for five seconds, then
// 0.5), raising the alert flags on the first; from a friend, at where the shot was, at 0.5.
void SensingAgent::collectFireFocus()
{
    m_fireFocus.resize(0);
    const std::list<std::pair<FireObject*, unsigned int> >& fire = m_senses->getFireEntries();
    const double time = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (std::list<std::pair<FireObject*, unsigned int> >::const_iterator it = fire.begin();
         it != fire.end(); ++it)
    {
        Information* const shooter = informationManager->getInformation(it->second);
        if (!shooter)
            continue;
        FireObject* const shot = it->first;
        FocusInformation entry;
        float weight;
        if (shooter->getSide() != m_bot->getSide())
        {
            if (!m_flag121)
            {
                m_flag120 = true;
                m_flag121 = true;
            }
            weight = 0.7f;
            if (!(time - shot->getTime() < 5.0))
                weight = 0.5f;
            entry.m_position = shooter->getPosition();
        }
        else
        {
            entry.m_position = shot->getPosition();
            weight = 0.5f;
        }
        entry.m_weight = shooter->getSecurity() * weight;
        m_fireFocus.push_back(entry);
    }
}

// PE 101B1470. The time is passed and not read.
void SensingAgent::decideFocus(FocusChoice& focus, const double&)
{
    collectSensedFocus();
    collectFireFocus();
    QuadrantInformation empty = {};
    std::vector<QuadrantInformation> quadrants(8, empty);
    sumQuadrants(quadrants);
    chooseQuadrant(quadrants, focus);
}

// PE 101B14F0. Once per reaction time (sooner when alerted or without a plan) the agent weighs
// what it senses into eight sectors around the bot and rebuilds its look plan.
void SensingAgent::updatePlan()
{
    Information* const equipment = informationManager->getInformation(m_bot->m_currentEquipment);
    if (!equipment)
        return;
    if (m_bot->getManDown())
        return;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    ISensingAgentPattern* const pattern =
        settings->getSensingAgentPattern(equipment->getUnit()->getEquipmentType());
    if (!pattern)
    {
        deletePlan();
        return;
    }
    if (m_flag122)
        deletePlan();
    const double time = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (m_flag122)
    {
        if (time - m_double124 > 2.0)
            m_flag122 = false;
        return;
    }
    if (time < m_double112)
        m_double112 = time;
    const double elapsed = time - m_double112;
    Information* const current = informationManager->getInformation(m_bot->getCurrentEquipment());
    const bool reconsider = ((current->getObjectTypeFlags() >> 21) & 1) && m_flag133 &&
                            !isInQuadrant(m_int24, m_lookDirection);
    if (m_reactionTime > elapsed && !m_flag120 && !m_flag140)
    {
        if (!reconsider)
            return;
        m_flag133 = false;
    }
    else if (reconsider)
    {
        m_flag133 = false;
    }

    const int pose = equipment->getSoldierPose();
    const bool skipFocus = pose == 3 ? !m_flag120 : pose == 4;
    if (!m_bot->isFireAllowed() || skipFocus)
    {
        Vec3 direction;
        direction.x = m_lookDirection.x;
        direction.y = current->getTransformation().m[9];
        direction.z = m_lookDirection.y;
        createPlanTowards(direction, pattern);
        return;
    }

    Vec2 look = {};
    if (!m_flag120)
        updateQuadrantWeights(look, reconsider);
    FocusChoice focus;
    focus.m_position.x = focus.m_position.y = focus.m_position.z = 0.0f;
    decideFocus(focus, time);
    updateReactionTime(focus, current, look);
    if (!m_flag120)
        m_flag121 = false;
    else
        deletePlan();
    m_double112 = time;
    m_flag120 = false;
    checkPlanValidity(focus, reconsider);
    if (m_flag140)
        createPlanFromFocus(pattern, focus);
    clearFocus();
}

} // namespace ai
} // namespace hfe
} // namespace dice
