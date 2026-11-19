// Aim-correction state initialization.

#include "BotSubObjects.h"

#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

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
    for (int i = 0; i < 6; ++i)
        m_zero8[i] = 0;
    m_aimDirection.x = m_aimDirection.y = m_aimDirection.z = 0.0f;
    m_targetOffset.x = m_targetOffset.y = m_targetOffset.z = 0.0f;
    m_aimPoint.x = m_aimPoint.y = m_aimPoint.z = 0.0f;
    // nine zeroed words, +88..+120
    m_projectilePrePoint.x = m_projectilePrePoint.y = m_projectilePrePoint.z = 0.0f;
    m_projectilePostPoint.x = m_projectilePostPoint.y = m_projectilePostPoint.z = 0.0f;
    m_projectileTargetPoint.x = m_projectileTargetPoint.y = m_projectileTargetPoint.z = 0.0f;
}

void FireCorrectionData::setTarget(unsigned int handle)
{
    m_targetTime = -1.0;
    m_flag5 = true;
    m_flag4 = true;
    m_flag6 = true;
    Information* const information = informationManager->getInformation(handle);
    m_targetOffset.x = 0.0f;
    m_targetOffset.y = 0.0f;
    m_targetOffset.z = 0.0f;
    if (information)
        m_targetObject = information->getAIObject()->getObject();
    else
        m_targetObject = nullptr;
}

} // namespace ai
} // namespace hfe
} // namespace dice
