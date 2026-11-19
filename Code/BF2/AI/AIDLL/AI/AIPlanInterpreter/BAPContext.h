// Variable scope for a bot action plan. [INFERRED] Source placement.
// Call clear to delete the owned variables before destruction; the destructor frees only list nodes.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H

#include <list>

#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

// Base for owned script variables; deletion uses the virtual destructor.
class BotScriptVariable
{
public:
    virtual ~BotScriptVariable();       // - the vptr store, nothing more
};

class BAPContext;

// A typed value shared by the statements and conditions of one plan; owned by the context that
// made it. One vtable per value type in the PE: WeaponInfo* 1027EA90, int 1027EA94, unsigned int
// 1027EB68, bool 1027EB6C, Vec3 1027EB70. [INFERRED] names; the meaning of m_flag is [UNRESOLVED]
// (every audited caller passes 0).
template <class T>
class BotScriptValue : public BotScriptVariable
{
public:
    BotScriptValue(BAPContext* context, const T& value, int flag)
        : m_context(context), m_value(value), m_flag(flag)
    {
    }

    /* +0x04 */ BAPContext* m_context;
    /* +0x08 */ T m_value;
    int m_flag;
};

class BAPContext
{
public:
    BAPContext();
    ~BAPContext();                      // - , and it leaks; see above
    void clear();

    // Makes a value owned by this context and appends it to the variable list. PE 100FF690
    // (WeaponInfo*), 100FF700 (int), 10106080 (unsigned int), 101060F0 (bool), 10106160 (Vec3).
    template <class T>
    BotScriptValue<T>* createVariable(const T& value, int flag);

private:
    /* 0 */ std::list<BotScriptVariable*> m_variables;   // the whole object
};

template <class T>
BotScriptValue<T>* BAPContext::createVariable(const T& value, int flag)
{
    BotScriptValue<T>* const variable = checkPointer(new BotScriptValue<T>(this, value, flag));
    m_variables.push_back(variable);
    return variable;
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H
