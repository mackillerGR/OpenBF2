// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIPlayerCourier operations.

#include "AIPlayerCourier.h"

#include <string>

#include "../../../../Core/ClassManager.h"
#include "CommonSingletonServer.h"
#include "AIDllFrontier.h"
#include "IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// IAIPlayerCourier
IAIPlayerCourier::IAIPlayerCourier()
{
    if (!classManager->getSimpleSingleton("IAIPlayerCourier"))
        classManager->registerSimpleSingleton("IAIPlayerCourier", this);
}

IAIPlayerCourier::~IAIPlayerCourier()
{
    if (classManager->getSimpleSingleton("IAIPlayerCourier") == this)
        classManager->unregisterSimpleSingleton("IAIPlayerCourier");
}

// Stateless player courier; construction requires only the base-class initialization.
AIPlayerCourier::AIPlayerCourier()
{
}

// The vptr reset and the base call, which again is what C++ emits.
AIPlayerCourier::~AIPlayerCourier()
{
}

namespace {

// The engine object of a handle's information, or null.
world::IObject* engineObjectOf(unsigned int handle)
{
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return 0;
    return information->getAIObject()->getObject();
}

} // anonymous namespace

// PE 100BF0E0. The vehicle the player's bot is changing into, or null.
world::IObject* AIPlayerCourier::getSelectedVehicle(int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    return engineObjectOf(bot->getChangeTarget());
}

// PE 100BF140. Reads the bot's bridge-repair target, as shipped.
world::IObject* AIPlayerCourier::getRepairTarget(int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    return engineObjectOf(bot->getBridgeRepairTarget());
}

// PE 100BF1A0.
world::IObject* AIPlayerCourier::getReviveTarget(int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return 0;
    return engineObjectOf(bot->getReviveTarget());
}

// PE 100BF080. The bot's mine target handle, or -1 without a bot.
unsigned int AIPlayerCourier::getMineDefuseTarget(int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (!bot)
        return static_cast<unsigned int>(-1);
    return static_cast<BotMain*>(bot)->getMineTarget();
}

} // namespace ai
} // namespace hfe
} // namespace dice
