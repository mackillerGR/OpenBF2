// Base statement of a bot action plan. Plans are trees of statements; the plan interpreter runs
// each node through a per-vehicle primitive or, failing that, the statement's own execute.
// [INFERRED] placement: the attested action-plan sources live in AIBotActionPlan, but no
// __FILE__ string names this class's own file.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BOTSCRIPT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BOTSCRIPT_H

#include <iosfwd>
#include <string>

#include "../AIPlanInterpreter/BAPStatementType.h"

namespace dice {
namespace hfe {
namespace ai {

class Bot;

// PE vtable 10282AD8, seven slots. An outer interface (vtable 10282AD0: a pure first slot and the
// destructor) carries no data and is folded into this class. [UNRESOLVED] its name.
class BotScript
{
public:
    BotScript();

    // Runs the statement. `finished` and `failed` report the statement's state to its parent;
    // the result is whether the call itself was handled.
    virtual bool execute(Bot* bot, bool& finished, bool& failed) = 0;
    virtual ~BotScript();
    // The player inputs this statement drives, one bit per io::PlayerInputMap value.
    virtual unsigned long long getInputMask();
    virtual BAPStatementType getType() = 0;
    // True for statements that hold other statements. [INFERRED] name.
    virtual bool isComposite() = 0;
    // [INFERRED] names for the two debug slots.
    virtual void getDebugString(Bot* bot, std::string& out);
    virtual void printDebug(std::ostream& out);

    // PE 101A81E0, ordinary member.
    bool hasInputMask() const { return m_inputMask != 0; }
    int getId() const { return m_id; }

    // The parent statement, or for a plan's root the generator or behaviour that made it.
    // [UNRESOLVED] declared type: callers store three unrelated pointer types here.
    /* +0x04 */ void* m_owner;
    /* +0x08 */ unsigned long long m_inputMask;
    // The id of the behaviour whose generator built the statement.
    /* +0x10 */ int m_id;

    // Construction and destruction counts. [INFERRED] names.
    static int s_created;
    static int s_destroyed;
};

std::ostream& operator<<(std::ostream& out, BAPStatementType type);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BOTSCRIPT_H
