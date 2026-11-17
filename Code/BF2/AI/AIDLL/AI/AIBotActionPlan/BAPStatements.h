// Control-flow and primitive statements used by the plan generators: the parallel container,
// if, while, trigger, reset-controls and the wrappers. Primitive statements only carry their
// arguments; what they do is supplied per vehicle by the interpreter's entries.
// [INFERRED] placement and class names: no __FILE__ string names these classes' files.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H

#include <list>

#include "BotScript.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class BAPCondition;

// A statement with a list of child statements. PE vtable 10282BA4 adds the pure eighth slot.
class BAPStatementList : public BotScript
{
public:
    BAPStatementList();
    // PE 1019A280: frees the list nodes, not the children.
    virtual ~BAPStatementList();
    // Appends a child and claims it.
    virtual void addStatement(BotScript* statement) = 0;

protected:
    /* +0x18 */ std::list<BotScript*> m_children;
    /* +0x24 */ std::list<BotScript*>::iterator m_cursor;
    // Combine child failures with "and" instead of "or". [INFERRED] name.
    /* +0x28 */ bool m_failOnlyIfAllFail;
};

// Runs every child each tick; finished when all are. Type 0. PE vtable 10282BC4.
class BAPParallel : public BAPStatementList
{
public:
    // PE 1019A4B0.
    BAPParallel(int id, bool failOnlyIfAllFail, int limit, bool neverFail);
    // PE 1019A410: fails only when every child fails.
    BAPParallel(int id, int limit, bool neverFail);
    // PE 1019A540: destroys the children.
    virtual ~BAPParallel();

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019A010
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 10199B80
    virtual void addStatement(BotScript* statement);                   // PE 1019A600

private:
    // One flag per child. Execution writes every child's failure into the first node, as shipped.
    /* +0x30 */ std::list<bool> m_childFailed;
    /* +0x3C */ int m_limit;           // [UNRESOLVED] meaning; printed when not -1
    /* +0x40 */ bool m_neverFail;
};

// Runs its children one at a time. Type 1. With the base flag set (always, from this
// constructor) a child that reports failure hands over to the next, and failure is reported only
// after the last child; without it the cursor advances every call. PE vtable 10282C8C.
// [INFERRED] name, from the "Serial combiner" debug title.
class BAPSerial : public BAPStatementList
{
public:
    BAPSerial(int id, int limit);                                      // PE 1019AE30
    virtual ~BAPSerial();                                              // PE 1019AD90: destroys the children

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 1019ACC0
    virtual unsigned long long getInputMask();                         // PE 1019AD80
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 1019AB90
    virtual void addStatement(BotScript* statement);                   // PE 1019AF40

private:
    /* +0x30 */ int m_limit;           // [UNRESOLVED] meaning
};

// Evaluates a condition once: finished, failed when the condition holds. Type 4. PE vtable
// 102839BC. [INFERRED] name; the type's script spelling is "Wait".
class BAPWait : public BotScript
{
public:
    BAPWait(BAPCondition* condition, int id);                          // PE 101AD200
    virtual ~BAPWait();                                                // PE 101AD250

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AD280
    virtual unsigned long long getInputMask();                         // PE 101AD2B0: none
    virtual BAPStatementType getType();
    virtual bool isComposite();

    /* +0x18 */ BAPCondition* m_condition;
};

// Asks the bot to change into the target vehicle or seat. Type 29. The constructor leaves the id
// unset, as shipped. PE vtable 102839A0. [INFERRED] name.
class BAPICChangeVehicle : public BotScript
{
public:
    explicit BAPICChangeVehicle(unsigned int target);                  // PE 101AD140
    virtual ~BAPICChangeVehicle();                                     // PE 101AD170

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101AD1B0
    virtual BAPStatementType getType();
    virtual bool isComposite();                                        // PE 101AD1F0: true

    /* +0x18 */ unsigned int m_target;
};

// Releases the bot's controls in up to three groups. Type 14. PE vtable 10283378.
class BAPResetControls : public BotScript
{
public:
    // PE 101A80F0.
    BAPResetControls(bool movement, bool look, bool actions, int id);
    virtual ~BAPResetControls();

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A8210
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101A8230

    /* +0x18 */ bool m_movement;
    /* +0x19 */ bool m_look;
    /* +0x1A */ bool m_actions;
    // The single input released when the statement has an input mask. PE 101A8200 reads it.
    /* +0x1C */ int m_input;
};

// Runs `then` while the condition holds and `otherwise` while it does not. Type 3.
// PE vtable 102835E8.
class BAPIf : public BotScript
{
public:
    // PE 101A94A0.
    BAPIf(BAPCondition* condition, BotScript* then, BotScript* otherwise, int id,
          bool reevaluate, bool reevaluateOnlyInElse);
    virtual ~BAPIf();                                                  // PE 101A98B0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A9570
    virtual unsigned long long getInputMask();                         // PE 101A9600
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void getDebugString(Bot* bot, std::string& out);           // PE 101A9920
    virtual void printDebug(std::ostream& out);                        // PE 101A96A0

private:
    /* +0x18 */ BAPCondition* m_condition;
    /* +0x20 */ BotScript* m_then;
    /* +0x24 */ BotScript* m_otherwise;
    /* +0x28 */ BotScript* m_current;
    /* +0x2C */ BotScript* m_lastChosen;
    // [INFERRED] names.
    /* +0x30 */ bool m_reevaluate;
    /* +0x31 */ bool m_reevaluateOnlyInElse;
};

// Presses one input until a condition holds. Type 11. PE vtable 10283728.
class BAPTrigger : public BotScript
{
public:
    // PE 101A9E00.
    BAPTrigger(int input, BAPCondition* until, BAPCondition* release,
               unsigned long long inputMask, int id, double delay, bool flag);
    virtual ~BAPTrigger();                                             // PE 101AA0E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A9EF0
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void getDebugString(Bot* bot, std::string& out);           // PE 101A9F10
    virtual void printDebug(std::ostream& out);                        // PE 101A9FA0

    /* +0x18 */ int m_input;
    /* +0x1C */ BAPCondition* m_until;    // [INFERRED] names of both conditions
    /* +0x20 */ BAPCondition* m_release;
    /* +0x24 */ bool m_pressed;           // = false
    /* +0x28 */ double m_startTime;       // game time at construction plus the delay
    /* +0x30 */ bool m_flag30;            // [UNRESOLVED]
};

// Runs its body while the condition holds. Type 5. PE vtable 102833E4.
class BAPWhile : public BotScript
{
public:
    // PE 101A83A0.
    BAPWhile(BAPCondition* condition, BotScript* body, int id);
    virtual ~BAPWhile();                                               // PE 101A86C0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101A84A0
    virtual unsigned long long getInputMask();                         // PE 101A8570
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101A85D0

private:
    /* +0x18 */ BAPCondition* m_condition;
    /* +0x20 */ BotScript* m_body;
    // [INFERRED] names.
    /* +0x24 */ bool m_abort;             // = false
    /* +0x25 */ bool m_testCondition;     // = true
    /* +0x26 */ bool m_testAfterFailure;  // = true
};

// A statement that runs one child. Type 15. PE vtable 10285420.
class BAPWrapper : public BotScript
{
public:
    // PE 101DF240.
    BAPWrapper(BotScript* child, int id);
    virtual ~BAPWrapper();                                             // PE 101DF2E0

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101DF2B0
    virtual unsigned long long getInputMask();                         // PE 101A5D50
    virtual BAPStatementType getType();
    virtual bool isComposite();
    virtual void printDebug(std::ostream& out);                        // PE 101DF320
    // Index into the wrapper name table. [INFERRED] name.
    virtual int getWrapperIndex();

    /* +0x18 */ BotScript* m_child;
};

// The root of a generated plan: remembers where it was made and marks the bot's squad member
// as running a plan while it lives. PE vtable 10283550.
class BAPInfoWrapper : public BAPWrapper
{
public:
    // PE 101A9060.
    BAPInfoWrapper(const Vec3& position, BotScript* child, int id, Bot* bot);
    virtual ~BAPInfoWrapper();                                         // PE 101A90D0
    virtual int getWrapperIndex();

    /* +0x20 */ Vec3 m_position;
    /* +0x2C */ Bot* m_bot;
};

// Steers the soldier's movement relative to where it looks. Type 36. PE vtable 10282B10.
class BAPMergeSoldierInputs : public BotScript
{
public:
    explicit BAPMergeSoldierInputs(int id);                            // PE 10199030
    virtual ~BAPMergeSoldierInputs();                                  // PE 10199140

    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101991F0
    virtual BAPStatementType getType();
    virtual bool isComposite();

    // Last tick's look-controller errors, fed back as the derivative's history.
    /* +0x18 */ float m_yawError;
    /* +0x1C */ float m_pitchError;
};

// Turns the bot's look toward a target. Type 23. PE vtable 102854C8; the target comes from the
// eighth slot. The name is attested by SensingAgentSoldierPattern::endPlan's signature.
class BAPALook : public BotScript
{
public:
    // PE 101DF6E0.
    BAPALook(float deadZone, BAPCondition* until, unsigned long long inputMask, int id);
    virtual ~BAPALook();                                               // PE 101DF740

    // The fallback when no entry is installed: unfinished and failed.
    virtual bool execute(Bot* bot, bool& finished, bool& failed);      // PE 101DF6C0
    virtual BAPStatementType getType();
    virtual bool isComposite();
    // Where to look, and whether that is known. [INFERRED] name.
    virtual Vec3 getTarget(bool& valid) = 0;

    /* +0x18 */ BAPCondition* m_until;
    /* +0x1C */ float m_deadZone;     // the look controller's dead zone
    // Controller state the look entry carries between ticks: two values it feeds back to the
    // controller (+0x20 is reset to pi when the look ends) and the last camera forward.
    /* +0x20 */ float m_state20;
    /* +0x24 */ float m_state24;
    /* +0x28 */ Vec3  m_lastForward;
};

// Looks along a fixed direction. PE vtable 10284990. [INFERRED] name.
class BAPALookDirection : public BAPALook
{
public:
    // PE 101C17B0.
    BAPALookDirection(const Vec3& direction, float deadZone, BAPCondition* until,
                      unsigned long long inputMask, int id);
    virtual ~BAPALookDirection();                                      // PE 101C1800
    virtual Vec3 getTarget(bool& valid);                               // PE 101C1780

    /* +0x38 */ Vec3 m_direction;
};

// Looks at an object. PE vtable 10283AA8. [INFERRED] name.
class BAPALookAtObject : public BAPALook
{
public:
    // PE 101AD7D0.
    BAPALookAtObject(Bot* bot, unsigned int object, float deadZone, BAPCondition* until,
                     unsigned long long inputMask, int id, int arg8, int arg9);
    virtual ~BAPALookAtObject();                                       // PE 101AD850
    virtual Vec3 getTarget(bool& valid);                               // PE 101AD890

    /* +0x38 */ Bot* m_bot;
    /* +0x3C */ unsigned int m_object;
    /* +0x40 */ int m_arg40;         // [UNRESOLVED]
    /* +0x44 */ int m_arg44;         // [UNRESOLVED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPSTATEMENTS_H
