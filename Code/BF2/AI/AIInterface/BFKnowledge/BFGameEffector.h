// Applies AI actions to engine players and objects.
// Preserve virtual declaration order. [INFERRED] Return types require validation where implementations remain incomplete.
// [INFERRED] Source placement.

#ifndef AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H
#define AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "AIAdapterTypes.h"

namespace dice {
namespace hfe {

class IBase;

namespace ai {

class IAIGameEffector
{
public:
    IAIGameEffector();
    virtual ~IAIGameEffector();

    virtual int enterVehicle(IAIPlayer* player, IAIObject* vehicle) = 0;
    virtual int exitVehicle(IAIPlayer* player) = 0;
    // PE 81F380 / 81F3C0 leave the void IGameServer::destroyPlayer result in eax; no return
    // value is materialized, so neither overload returns one.
    virtual void terminatePlayer(int playerId) = 0;
    virtual void terminatePlayer(IAIPlayer* player) = 0;
    virtual int addBot(int& a, int& b, std::string& name) = 0;
    virtual int respawnBot(IAIPlayer* player) = 0;
    virtual void forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation) = 0;
    virtual int giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount) = 0;
    // PE 81FCF0 materializes no return value after the void IGameLogic::playEffect call.
    virtual void playEffect(const Vec3& position, int a, int b) = 0;
    virtual void playSoundEffects(double dt) = 0;
    virtual int suicide(IAIPlayer* player) = 0;
};

class BFGameEffector : public IAIGameEffector
{
public:
    BFGameEffector();
    virtual ~BFGameEffector();

    virtual int enterVehicle(IAIPlayer* player, IAIObject* vehicle);
    virtual int exitVehicle(IAIPlayer* player);
    virtual void terminatePlayer(int playerId);
    virtual void terminatePlayer(IAIPlayer* player);
    virtual int addBot(int& a, int& b, std::string& name);
    virtual int respawnBot(IAIPlayer* player);
    virtual void forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation);
    virtual int giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount);
    virtual void playEffect(const Vec3& position, int a, int b);
    virtual void playSoundEffects(double dt);
    virtual int suicide(IAIPlayer* player);

private:
    /* +0x00 */ // vptr
    // A queued material effect: position plus the material pair passed to
    // MaterialManager::getEffectTemplate. 20 bytes in the PE. [INFERRED] field names.
    struct PendingEffect
    {
        Vec3 position;
        int materialA;
        int materialB;
    };
    // Effects queued by playEffect and replayed, then cleared, by playSoundEffects.
    // PE +0x04 (MSVC 7.1 vector: allocator, first +0x08, last +0x0C, end +0x10).
    std::vector<PendingEffect> m_pendingEffects;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_BFKNOWLEDGE_BFGAMEEFFECTOR_H
