// [INFERRED] Source placement.
// Bodies are read from BF2.exe 1.50. [FRONTIER] Only the dedicated image's playEffect and
// playSoundEffects remain unwritten; see the marker beside them.

#include "BFGameEffector.h"

#include "AIFrontier.h"
#include "AIPlayer.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"   // IAISettings::getBotName
#include "../../../Core/ClassManager.h"
#include "../../../Core/Ids.h"                          // IID_IGameServer
#include "../../../Game/IGame.h"                        // the `game` global
#include "../../../Game/IGameServer.h"                  // createPlayer, slot +0x20
#include "../../../Game/IGameLogic.h"                   // the `gameLogic` global
#include "../../../Scene/Object/PlayerManager.h"        // the `playerManager` global
#include "../../../Core/SettingsRepostitory.h"           // settings->boolGet("GSIsServer")
#include "../../../Game/GameLogic/ServerGameLogic.h"     // spawnPlayer, called directly
#include "../../../Physics/MaterialManager.h"            // getEffectTemplate (PE +0x5C)
#include "../../../Game/Objects/IObjectComponent.h"      // IAIObject::getObject
#include "IAIObject.h"
#include "BFTimer.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace world { extern IBase* materialManager; }   // Core/Globals.cpp

namespace {
// The by-value vectors the retail bodies build on the stack.
Vec3 zeroVector() { Vec3 v = { 0.0f, 0.0f, 0.0f }; return v; }
Vec3 upVector() { Vec3 v = { 0.0f, 1.0f, 0.0f }; return v; }
} // namespace

namespace ai {

// First wins in, exact match out.
IAIGameEffector::IAIGameEffector()
{
    if (!CommonSingletonServer::m_iaiGameEffector)
        classManager->registerSimpleSingleton(std::string("IAIGameEffector"), this);
}

IAIGameEffector::~IAIGameEffector()
{
    if (CommonSingletonServer::m_iaiGameEffector == this)
        classManager->unregisterSimpleSingleton(std::string("IAIGameEffector"));
}

// PE 81FB40. Only the effect queue is initialized.
BFGameEffector::BFGameEffector()
{
}

BFGameEffector::~BFGameEffector()
{
}

// PE 81F240. Puts the player into the vehicle's engine object through IGame without
// moving it there; `game` is not null-checked.
int BFGameEffector::enterVehicle(IAIPlayer* player, IAIObject* vehicle)
{
    return game->enterVehicle(player->getPlayer(), vehicle->getObject(), false);
}

// PE 81F270. Takes the player out of its vehicle through IGame; `game` is not null-checked.
int BFGameEffector::exitVehicle(IAIPlayer* player)
{
    return game->exitVehicle(player->getPlayer(), true, true);
}

// PE 81F380. Destroys the engine player with this id through the game server. The query
// result is not checked, so a missing game crashes as it does in retail.
void BFGameEffector::terminatePlayer(int playerId)
{
    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    server->destroyPlayer(world::playerManager->getPlayerFromId(playerId));
}

// PE 81F3C0. As above, for the AI player's engine player.
void BFGameEffector::terminatePlayer(IAIPlayer* player)
{
    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    server->destroyPlayer(player->getPlayer());
}

// Creates an AI player through the game server and returns its player ID.
int BFGameEffector::addBot(int& a, int& b, std::string& name)
{
    if (name.empty())
    {
        const IAISettings* const settings =
            static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

        name.assign(settings->getBotName());
    }

    // equal weights - the engine balances, not the AI.
    b = world::playerManager->getTeamToAddPlayer(1.0f, 1.0f);

    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;

    if (server)
    {
        // asked A second time. See the block above; the binary does not reuse `server`.
        IGameServer* const target =
            game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;

        a = target->createPlayer(name, true);
        return a;
    }

    a = -1;
    return 0;
}

// PE 81F350. Asks the game logic to respawn the bot's player; the player is not null-checked.
int BFGameEffector::respawnBot(IAIPlayer* player)
{
    return gameLogic->reSpawnPlayer(player->getPlayer());
}

// PE 81F520. Only a server spawns: the bot's player is spawned at the given position and
// rotation through ServerGameLogic::spawnPlayer.
void BFGameEffector::forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation)
{
    if (!hfe::settings->boolGet(std::string("GSIsServer"), false))
        return;
    std::pair<Vec3, Vec3> at(position, rotation);
    static_cast<ServerGameLogic*>(gameLogic)->spawnPlayer(player->getPlayer(), &at);
}

// PE 81F440. Applies direct damage to the target's engine object: no damage list, no
// projectile or weapon objects, zero position, an up normal, and no extra flags but the
// second. Returns the game logic's result.
int BFGameEffector::giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount)
{
    return gameLogic->giveDamage(target->getObject(), amount, from, 0, 0, type, 0, 0,
                                 zeroVector(), upVector(), -1,
                                 false, true, false);
}

#if !DEDICATED
// PE 81FCF0. Queues the effect for playSoundEffects, then plays the material pair's effect
// template at the position with an up normal.
void BFGameEffector::playEffect(const Vec3& position, int a, int b)
{
    PendingEffect effect;
    effect.position = position;
    effect.materialA = a;
    effect.materialB = b;
    m_pendingEffects.push_back(effect);
    gameLogic->playEffect(position,
                          static_cast<const world::IObjectTemplate*>(
                              static_cast<world::MaterialManager*>(world::materialManager)->getEffectTemplate(
                                  static_cast<unsigned int>(a), static_cast<unsigned int>(b), 0)),
                          upVector());
}

// PE 81FC40. Replays queued effects, in order, until the update clock passes `dt` seconds
// beyond its value at entry, then drops the whole queue including effects not replayed.
void BFGameEffector::playSoundEffects(double dt)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + dt;
    for (std::vector<PendingEffect>::iterator it = m_pendingEffects.begin();
         it != m_pendingEffects.end(); ++it)
    {
        gameLogic->playEffect(it->position,
                              static_cast<const world::IObjectTemplate*>(
                                  static_cast<world::MaterialManager*>(world::materialManager)->getEffectTemplate(
                                  static_cast<unsigned int>(it->materialA), static_cast<unsigned int>(it->materialB), 0)),
                              upVector());
        if (timer->getUpdateTime() > deadline)
            break;
    }
    m_pendingEffects.resize(0);
}
#else
// The dedicated image has not been checked for these two bodies.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-01-adapters] Read the
// dedicated server's BFGameEffector::playEffect and playSoundEffects and write them here.
void BFGameEffector::playEffect(const Vec3& position, int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFGameEffector::playEffect");
}

void BFGameEffector::playSoundEffects(double dt)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFGameEffector::playSoundEffects");
}
#endif

// PE 81F400. Kills the bot by giving its soldier 1e8 damage from itself, through the
// virtual giveDamage.
int BFGameEffector::suicide(IAIPlayer* player)
{
    return giveDamage(player->getSoldier(), player->getPlayer(), player->getSide(), 1.0e8f);
}

} // namespace ai
} // namespace hfe
} // namespace dice
