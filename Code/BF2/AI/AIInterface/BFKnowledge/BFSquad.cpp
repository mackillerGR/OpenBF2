// [INFERRED] Source placement.
// [FRONTIER] Remaining methods stop by name until implemented.
// [TEMP] Temporary implementation boundaries remain.
// [CHOSEN, not recovered] Temporary implementation boundaries remain.
// [RETIRE-WHEN: aidll-01-adapters] Complete and validate the remaining BFSquad operations.

#include "BFSquad.h"
#include "AIPlayer.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods

namespace dice {
namespace hfe {

extern ClassManager* classManager;
extern IBase* squadManager;

namespace ai {

// First wins in, exact match out.
IBFSquad::IBFSquad()
{
    if (!CommonSingletonServer::m_iBFSquad)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IBFSquad"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IBFSquad::~IBFSquad()
{
    if (CommonSingletonServer::m_iBFSquad == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IBFSquad"));
        BFSingletonServer::updateStaticMethods();
    }
}

BFSquad::BFSquad()
{
}

BFSquad::~BFSquad()
{
}

// A tail call to the squad manager's first unused squad. Client PE 0x816BD0.
int BFSquad::getNextEmptySquad(int team) const
{
    return static_cast<ISquadManager*>(squadManager)->getFirstUnusedSquad(team);
}

// Preserve id-before-side virtual calls.
bool BFSquad::joinSquad(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    return static_cast<ISquadManager*>(squadManager)->addToSquad(side, squad, playerId);
}

// Removes the player from the squad without the remover's notification. Client PE 0x816C20
// reads the id before the side.
void BFSquad::exitSquad(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    static_cast<ISquadManager*>(squadManager)->removeFromSquad(side, squad, playerId, false);
}

bool BFSquad::isSquadLeader(int squad, IAIPlayer* player)
{
    // [PLATFORM] ELF 0x84109C0 reads id, side, leader; PE 0x742110 reads side, leader, id.
#if defined(_MSC_VER)
    const int side = player->getSide();
    const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(side, squad);
    const int playerId = player->getId();
#else
    const int playerId = player->getId();
    const int side = player->getSide();
    const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(side, squad);
#endif
    return playerId == leader;
}

// Makes the player the squad's leader. Client PE 0x816CA0 reads the id before the side.
void BFSquad::assignAsSquadLeader(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    static_cast<ISquadManager*>(squadManager)->setSquadLeader(side, squad, playerId, false);
}

// Clears the squad's leader only when it is this player; the side is read again for the store.
// Client PE 0x816CE0.
void BFSquad::retireAsSquadLeader(int squad, IAIPlayer* player)
{
    ISquadManager* const manager = static_cast<ISquadManager*>(squadManager);
    const int leader = manager->getSquadLeader(player->getSide(), squad);
    if (player->getId() == leader)
        static_cast<ISquadManager*>(squadManager)->setSquadLeader(player->getSide(), squad, -1, false);
}

int BFSquad::issueSquadAttackOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::issueSquadAttackOrder");
}

int BFSquad::issueSquadMoveOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::issueSquadMoveOrder");
}

int BFSquad::issueSquadDefendOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::issueSquadDefendOrder");
}

// 5 bytes - a bare ret. Empty, like its decline twin.
void BFSquad::acceptSquadOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::acceptSquadOrder");
}

// ELF, - empty.
void BFSquad::declineSquadOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::declineSquadOrder");
}

int BFSquad::acceptCommanderAttackOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::acceptCommanderAttackOrder");
}

int BFSquad::acceptCommanderDefendOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::acceptCommanderDefendOrder");
}

int BFSquad::acceptCommanderMoveOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::acceptCommanderMoveOrder");
}

int BFSquad::declineCommanderAttackOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::declineCommanderAttackOrder");
}

int BFSquad::declineCommanderDefendOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::declineCommanderDefendOrder");
}

int BFSquad::declineCommanderMoveOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::declineCommanderMoveOrder");
}

int BFSquad::requestNewCommanderOrder(int squad, IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::requestNewCommanderOrder");
}

world::IObject* BFSquad::getCommanderOrder(int a, int b, Vec3& position, IAIObject* target)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::getCommanderOrder");
}

world::IObject* BFSquad::getSquadOrder(int a, int b, Vec3& position, IAIObject* target)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFSquad::getSquadOrder");
}

} // namespace ai
} // namespace hfe
} // namespace dice
