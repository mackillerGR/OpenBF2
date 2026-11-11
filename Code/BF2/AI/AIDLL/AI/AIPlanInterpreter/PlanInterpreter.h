// Per-vehicle table of plan primitives, indexed by statement type and level of detail.
// [INFERRED] placement and class name: the image carries no __FILE__ string for this class.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_PLANINTERPRETER_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_PLANINTERPRETER_H

#include "../AIBot/BotMain.h"   // BotLodLevel

namespace dice {
namespace hfe {
namespace ai {

class BAPEntry;
class BotScript;
enum BAPStatementType : int;

// Each statement type holds one primitive per level of detail (full, medium, light), plus a
// per-level bit mask of the types the interpreter handles. A level with its bit set and no
// primitive falls back to the script's own default execution. PE vtable 10281C68; the base
// vtable 10281C60 has a pure second slot, so the base is an interface with execute only.
class PlanInterpreter
{
public:
    // PE 10184860. Allocates `typeCount` empty rows and three zeroed masks.
    explicit PlanInterpreter(int typeCount);
    // PE 101845F0. Releases the rows and masks; the primitives are borrowed and not deleted.
    virtual ~PlanInterpreter();

    // PE 10184940. Runs the statement through its primitive, or the script's default handler.
    virtual bool execute(BotScript* script, Bot* bot, bool& finished, bool& failed);

    // Installs `entry` for its statement type at `level` and every coarser level that has no
    // primitive yet. False when the type is out of range or `level` is already taken.
    // PE 10184670 / 10184920 (level full).
    bool addEntry(BotLodLevel level, BAPEntry* entry);
    bool addEntry(BAPEntry* entry);
    // Removes `entry` from `level` and every coarser level that holds it. PE 10184720.
    bool removeEntry(BotLodLevel level, BAPEntry* entry);
    // Marks `type` as handled by the default execution from `level` up to the first level that
    // has a primitive. PE 101847C0.
    bool activateDefault(BAPStatementType type, BotLodLevel level);

private:
    enum { LevelCount = 3 };

    /* +0x04 */ int m_typeCount;
    /* +0x08 */ unsigned long long* m_handled;   // one mask per level
    /* +0x0C */ BAPEntry*** m_entries;           // [type][level]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_PLANINTERPRETER_H
