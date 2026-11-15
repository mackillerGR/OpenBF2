// [INFERRED] Source placement.
// [FRONTIER] Remaining methods stop by name until implemented.
// [TEMP] Temporary implementation boundaries remain.
// [CHOSEN, not recovered] Temporary implementation boundaries remain.
// [RETIRE-WHEN: aidll-01-adapters] Complete and validate the remaining BFCommander operations.

#include "BFCommander.h"

#include "AIFrontier.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/GameLogic/SquadManager/SquadOrder.h"
#include "IAIObject.h"

#include "../../../Core/EventManager.h"                          // fireArtillery's queued request
#include "../../../Game/IEventListener.h"                        // ECGameLogic
#include "../../../Game/GameEvents/ArtilleryEvent.h"
#include "../../../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../../../Game/Objects/Game/ArtilleryHandler.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods

namespace dice {
namespace hfe {

extern ClassManager* classManager;

extern IBase* squadManager;

namespace ai {

// First wins in, exact match out.
IBFCommander::IBFCommander()
{
    if (!CommonSingletonServer::m_iBFCommander)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IBFCommander"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IBFCommander::~IBFCommander()
{
    if (CommonSingletonServer::m_iBFCommander == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IBFCommander"));
        BFSingletonServer::updateStaticMethods();
    }
}

BFCommander::BFCommander()
{
}

namespace {

// BF2.exe 8179D0. Builds a commander order at the position, aimed at the target's networked
// object when there is one, and issues it for (b, a). The retail result is the stale EAX left
// by the void issue call; no caller reads it, so this returns 0. [CHOSEN] the return value.
int issueOrder(const Vec3& position, int a, int b, IAIObject* target, int type)
{
    SquadOrder order;
    order.orderId = type;
    order.points[0] = position;
    order.points[1].x = order.points[1].y = order.points[1].z = 0.0f;
    order.points[2].x = order.points[2].y = order.points[2].z = 0.0f;
    order.flag2C = false;
    order.targetNetId = 0;
    if (target)
    {
        if (io::INetworkable* const networkable = target->getObject()->getNetworkable())
            order.targetNetId = networkable->m_networkableId;
    }
    static_cast<ISquadManager*>(squadManager)->issueCommanderOrder(b, a, order, false);
    return 0;
}

} // anonymous namespace

BFCommander::~BFCommander()
{
}

int BFCommander::applyAsCommander(IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::applyAsCommander");
}

int BFCommander::retireAsCommander(IAIPlayer* player)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::retireAsCommander");
}

// BF2.exe 817A70: order type 5.
int BFCommander::giveAttackOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 5);
}

// BF2.exe 817A90: order type 4.
int BFCommander::giveMoveOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 4);
}

// BF2.exe 817AB0: order type 6.
int BFCommander::giveDefendOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 6);
}

void BFCommander::sendAttackMessage(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::sendAttackMessage");
}

void BFCommander::sendDefendMessage(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::sendDefendMessage");
}

// Queues the team's artillery request for the next game-logic update and, for a squad's request,
// marks that squad's order unaccepted. Client PE 0x8177E0 materializes no result.
void BFCommander::fireArtillery(int team, int squad, Vec2 position)
{
    ArtilleryEvent* const event = new ArtilleryEvent(team, position, 0u);
    eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    if (squad != -1)
        static_cast<ISquadManager*>(squadManager)->setIsAccepted(
            team, squad, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(0), false);
}

int BFCommander::sendUav(int a, int b, Vec2 position)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::sendUav");
}

int BFCommander::dropSupply(int a, int b, Vec2 position)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::dropSupply");
}

int BFCommander::dropVehicle(int a, int b, Vec2 position)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::dropVehicle");
}

int BFCommander::denyArtilleryRequest(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::denyArtilleryRequest");
}

// Whether the team's handler found any artillery piece. Client PE 0x8179B0 reads the handler's
// list size without a manager guard.
bool BFCommander::gameHasArtillery(int team)
{
    return world::artilleryManager->getHandler(static_cast<unsigned char>(team))->hasArtilleryPieces();
}

// Whether any of the team's pieces is still intact. Client PE 0x817410.
bool BFCommander::anyArtilleryPiecesLeft(int team)
{
    return world::artilleryManager->getHandler(static_cast<unsigned char>(team))->haveAnyArtillery();
}

// dice's capital A. Frozen - it is the mangled name.
int BFCommander::ArtilleryUnavailable(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::ArtilleryUnavailable");
}

int BFCommander::denySupplyRequest(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::denySupplyRequest");
}

int BFCommander::denyVehicleDropRequest(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::denyVehicleDropRequest");
}

int BFCommander::denyUavRequest(int a, int b)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFCommander::denyUavRequest");
}

} // namespace ai
} // namespace hfe
} // namespace dice
