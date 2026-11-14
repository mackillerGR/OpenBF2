#include "BAPStatements.h"

#include "BAPConditions.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIPlanInterpreter/PlanInterpreter.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

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

} // namespace ai
} // namespace hfe
} // namespace dice
