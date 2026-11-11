#include "PlanGenerator.h"

#include "../AIBot/BotMain.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

PlanGenerator::PlanGenerator()
{
    const int bots =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_hasPlan = checkPointer(new bool[bots]);
    for (int i = 0; i < bots; ++i)
        m_hasPlan[i] = false;
}

PlanGenerator::~PlanGenerator()
{
    delete[] m_hasPlan;
}

bool PlanGenerator::hasPlan(Bot* bot)
{
    return m_hasPlan[bot->getBotId()];
}

void PlanGenerator::reset(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = false;
}

void PlanGenerator::setHasPlan(Bot* bot)
{
    m_hasPlan[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
