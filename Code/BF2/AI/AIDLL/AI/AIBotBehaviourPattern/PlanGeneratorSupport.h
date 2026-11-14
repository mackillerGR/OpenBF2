// Allocation helpers shared by the plan generator and behaviour constructors.
// [CHOSEN, not recovered] Retail inlines each loop at its call site.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORSUPPORT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORSUPPORT_H

#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

inline const IAISettings* aiSettings()
{
    return static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

inline int configuredBotCount()
{
    return aiSettings()->getMaxNBots();
}

inline int configuredBattleStrengthCount()
{
    return const_cast<IAISettings*>(aiSettings())->getNBattleStrengths();
}

// A per-bot array filled with `value`.
template <typename T>
T* newFilledArray(int count, T value)
{
    T* const array = new T[count];
    for (int i = 0; i < count; ++i)
        array[i] = value;
    return array;
}

// A zeroed array of vectors.
inline Vec3* newZeroedVectors(int count)
{
    Vec3* const array = new Vec3[count];
    for (int i = 0; i < count; ++i)
    {
        array[i].x = 0.0f;
        array[i].y = 0.0f;
        array[i].z = 0.0f;
    }
    return array;
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_PLANGENERATORSUPPORT_H
