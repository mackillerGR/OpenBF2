// [INFERRED] Source placement.

#include "BFSquad.h"
#include "AIPlayer.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods
#include "IAIObject.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../IO/Network/NetworkManager.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/GameEvents/RequestEvent.h"
#include "../../../Game/GameLogic/SquadManager/ISquadInfo.h"
#include "../../../Game/GameLogic/SquadManager/SquadOrder.h"

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

namespace {

// Client PE 0x817050. A squad order of the given type at the position, aimed at the target's
// networkable when it has one, handed to the squad manager (team b, squad a). The order's last
// flag is left unset, as shipped. The shipped body returns whatever the void manager call
// leaves in eax; [CHOSEN, not recovered] 0 is returned here and every AI caller discards it.
int issueSquadOrderOfType(const Vec3& position, int a, int b, IAIObject* target, int type)
{
    SquadOrder order;
    order.orderId = type;
    order.points[0] = position;
    order.points[1].x = order.points[1].y = order.points[1].z = 0.0f;
    order.points[2].x = order.points[2].y = order.points[2].z = 0.0f;
    order.targetNetId = 0;
    if (target)
    {
        if (io::INetworkable* const networkable = target->getObject()->getNetworkable())
            order.targetNetId = networkable->m_networkableId;
    }
    static_cast<ISquadManager*>(squadManager)->issueSquadOrder(b, a, order, false);
    return 0;
}

// The request event a bot's answer raises, executed at once as if sent by the bot.
void raiseBotRequest(int squad, IAIPlayer* player, int requestType)
{
    RequestEvent* const event = new RequestEvent(player->getSide(), squad,
                                                 static_cast<RequestEvent::RequestTypes>(requestType));
    event->executeServer(player->getId());
    event->release();
}

// Client PE 0x816D40 (accepted) / 0x816DD0 (declined). The commander order of the given type is
// marked answered for the player's team; a bot's answer also raises request 2 or 3. The shipped
// bodies return the last call's eax; [CHOSEN, not recovered] 0 here, discarded by every caller.
int answerCommanderOrder(int squad, IAIPlayer* player, int type, bool accepted)
{
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        player->getSide(), squad, accepted, static_cast<OrderType>(1), static_cast<SquadOrderType>(type), false);
    if (player->getIsAIPlayer())
        raiseBotRequest(squad, player, accepted ? 2 : 3);
    return 0;
}

} // namespace

// Client PE 0x8170F0: type 5.
int BFSquad::issueSquadAttackOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueSquadOrderOfType(position, a, b, target, 5);
}

// Client PE 0x817110: type 4.
int BFSquad::issueSquadMoveOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueSquadOrderOfType(position, a, b, target, 4);
}

// Client PE 0x817130: type 6.
int BFSquad::issueSquadDefendOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueSquadOrderOfType(position, a, b, target, 6);
}

// Client PE 0x5A59F0, shared bare `ret 8`. Empty, like its decline twin.
void BFSquad::acceptSquadOrder(int, IAIPlayer*)
{
}

// Client PE 0x5A59F0 - empty.
void BFSquad::declineSquadOrder(int, IAIPlayer*)
{
}

// Client PE 0x816EF0.
int BFSquad::acceptCommanderAttackOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 5, true);
}

// Client PE 0x816F10.
int BFSquad::acceptCommanderDefendOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 6, true);
}

// Client PE 0x816F30.
int BFSquad::acceptCommanderMoveOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 4, true);
}

// Client PE 0x816F50.
int BFSquad::declineCommanderAttackOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 5, false);
}

// Client PE 0x816F70.
int BFSquad::declineCommanderDefendOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 6, false);
}

// Client PE 0x816F90.
int BFSquad::declineCommanderMoveOrder(int squad, IAIPlayer* player)
{
    return answerCommanderOrder(squad, player, 4, false);
}

// Client PE 0x816E60. Only a bot raises request 1. [CHOSEN, not recovered] 0 stands in for the
// shipped eax, which no caller reads.
int BFSquad::requestNewCommanderOrder(int squad, IAIPlayer* player)
{
    if (player->getIsAIPlayer())
        raiseBotRequest(squad, player, 1);
    return 0;
}

// Client PE 0x816FB0. The squad (team a, squad b) is not null-checked; its commander order's
// first point is copied out and the order's target is looked up by network id. The target
// argument is unused.
world::IObject* BFSquad::getCommanderOrder(int a, int b, Vec3& position, IAIObject*)
{
    SquadOrder* const order = static_cast<ISquadManager*>(squadManager)->getSquadInfo(a, b)->getCommanderOrders();
    position = order->points[0];
    return io::networkManager->getObject(order->targetNetId);
}

// Client PE 0x817000. The squad-order twin of getCommanderOrder.
world::IObject* BFSquad::getSquadOrder(int a, int b, Vec3& position, IAIObject*)
{
    SquadOrder* const order = static_cast<ISquadManager*>(squadManager)->getSquadInfo(a, b)->getSquadOrders();
    position = order->points[0];
    return io::networkManager->getObject(order->targetNetId);
}

} // namespace ai
} // namespace hfe
} // namespace dice
