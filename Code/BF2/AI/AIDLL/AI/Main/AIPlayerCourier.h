// Handles player orders in the AI module.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H
#define AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace world { class IObject; }
namespace ai {

// Registers the first IAIPlayerCourier in the simple-singleton registry.
// It has no roster static, so teardown resolves it by name.
class IAIPlayerCourier
{
public:
    IAIPlayerCourier();
    virtual ~IAIPlayerCourier();
    // The bodies return the target's engine object, or the mine target's handle; the engine-side
    // declaration (AIInterface/IAIPlayerCourier.h) must keep the same return types.
    virtual world::IObject* getSelectedVehicle(int playerId) = 0;
    virtual world::IObject* getRepairTarget(int playerId) = 0;
    virtual world::IObject* getReviveTarget(int playerId) = 0;
    virtual unsigned int getMineDefuseTarget(int playerId) = 0;
};

// AIPlayerCourier - sizeof == 4, from BF2AI::init step 12.
class AIPlayerCourier : public IAIPlayerCourier
{
public:
    AIPlayerCourier();
    virtual ~AIPlayerCourier();

    // IAIPlayerCourier
    virtual world::IObject* getSelectedVehicle(int playerId);   // PE 100BF0E0
    virtual world::IObject* getRepairTarget(int playerId);      // PE 100BF140
    virtual world::IObject* getReviveTarget(int playerId);      // PE 100BF1A0
    virtual unsigned int getMineDefuseTarget(int playerId);     // PE 100BF080

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPLAYERCOURIER_H
