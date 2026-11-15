#include "BAPStatements.h"

#include "BAPConditions.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "LookController.h"
#include "../AIBot/SensingAgent.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
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
// ahead by the soldier's current yaw rate. Sprinting or prone: the look is steered onto that
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
    const bool prone = equipment->getSoldierPose() == 4;

    bool steer;
    bool sprinting = false;
    if (isPressed(input, io::PISprint))
    {
        steer = true;
        sprinting = true;
    }
    else
    {
        steer = prone;
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
                                   unsigned long long inputMask, int id, int arg8, int arg9)
    : BAPALook(deadZone, until, inputMask, id)
    , m_bot(bot)
    , m_object(object)
    , m_arg40(arg8)
    , m_arg44(arg9)
{
    m_until->m_owner = this;
}

BAPALookAtObject::~BAPALookAtObject()
{
    if (m_until)
        delete m_until;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-look-at-object] Recover the body and
// validate a bot that looks at a sensed object.
Vec3 BAPALookAtObject::getTarget(bool&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BAPALookAtObject::getTarget");
}

} // namespace ai
} // namespace hfe
} // namespace dice
