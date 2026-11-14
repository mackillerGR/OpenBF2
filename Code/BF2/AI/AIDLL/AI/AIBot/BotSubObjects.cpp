// Aim-correction state initialization.

#include "BotSubObjects.h"

namespace dice {
namespace hfe {
namespace ai {

// Initializes aim-correction state while leaving byte ranges +0..+3 and +68..+83
// untouched. [UNRESOLVED] The contents of those ranges remain unknown.
FireCorrectionData::FireCorrectionData()
    : m_flag4(true)
    , m_flag5(true)
    , m_flag84(false)
{
    // +0..+3 and +68..+83 are deliberately untouched - see above.
    for (int i = 0; i < 15; ++i)
        m_zero8[i] = 0;
    // nine zeroed words, +88..+120
    m_projectilePrePoint.x = m_projectilePrePoint.y = m_projectilePrePoint.z = 0.0f;
    m_projectilePostPoint.x = m_projectilePostPoint.y = m_projectilePostPoint.z = 0.0f;
    m_projectileTargetPoint.x = m_projectileTargetPoint.y = m_projectileTargetPoint.z = 0.0f;
}


} // namespace ai
} // namespace hfe
} // namespace dice
