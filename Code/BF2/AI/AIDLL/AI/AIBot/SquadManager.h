// Owns squad coordinators and side-specific squad state.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_SQUADMANAGER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADMANAGER_H

#include <list>
#include <map>
#include <vector>
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class SquadCoordinator;   // (`operator new(0xE0)`); held only by pointer here
class SquadMember;
class AIStrategicObject;
class IAIPlayer;
// Attested enum name in both squad-strength ELF manglings. Enumerator names are not recovered.
enum StrategicStrengthType : int;

class SquadManager
{
public:
    SquadManager();

    // Direct AIMain scheduling call, double by value; neither image materializes a result.
    void update(double time);
    void updateArtilleryStatus(double time);
    void updateCommanderMessages();
    void updateHumanPlayers(double time);   // bounded
    void setPlayerOnValidPosition(IAIPlayer* player, bool valid, double time);

    // player id then one-based side. All two callers in
    // each image discard the result; neither body materializes one independently of its last
    // updateSquadLeader call. void is the recovered no-result contract, not decompiler EAX.
    // A full squad hands its last member to the next, empty squad before the new player joins.
    void addSquadMember(int playerId, int side);

    // Engine notifications use one-based side and squad ids. These bodies materialize no
    // result; the engine/courier callers discard it on both ports.
    void newSquadLeader(int side, int squadId);
    void playerJoinsSquad(int playerId, int squadId, int side);
    void playerLeavesSquad(int playerId, int squadId, int side); // [INFERRED] name; PE 1012B230
    void resetSquads();                     // [INFERRED] name; PE 1012B810
    void unAssignSquads(int side);          // [INFERRED] name; PE 1012AC70, one-based side
    int releaseSquadsFromArea(int areaIndex, int side); // [INFERRED] name; PE 1012AC00
    float getArtilleryRadius() const;       // [INFERRED] name; PE 101284C0 returns m_float152
    // The member's squad demand for support of the type at the target: 1 when the member already
    // requests it, 0 without a squad. PE 1012B370; [INFERRED] name.
    float getSupportDemand(SquadMember* member, unsigned int target, int type, int side);
    // Whether a human player is on the side. PE 1012C130; [INFERRED] name.
    bool hasHumanPlayerOnSide(int side);
    // Any side member's qualifying vehicle within radius of the position. [INFERRED] name and
    // argument split: PE 1012BB20 takes the four floats by value. PE 1012BB20.
    bool isFriendlyNearPosition(const Vec3& position, float radius, int side);
    // Distance to the nearest squad leader with a valid position; sqrt(FLT_MAX) when none.
    // PE 1012AD80; [INFERRED] name.
    float getDistanceToClosestSquad(const Vec3& position, int side);
    // Nearest idle squad with members and a positive doubled strength for the role (defence
    // strength when not attacking); a squad already pending for the object needs a bot-backed
    // leader. Writes the doubled strength. PE 1012AF50; [INFERRED] name.
    SquadCoordinator* getClosestAvailableSquad(AIStrategicObject* object, const Vec3& position,
                                               int side, bool attack, int* strength);
    void incomingArtillery(const Vec3& position, int side); // PE 10128C80, one-based side
    bool getArtilleryTarget(int side, Vec2& target);        // [INFERRED] name; PE 10129BE0
    // True when the position lies strictly inside an active artillery zone: the side's own at
    // once, the other side's only once its delay has passed. Reports the time since the fire
    // and the zone's one-based side. PE 10128D20; [INFERRED] name and parameter split.
    bool isInArtilleryDangerZone(Vec3 position, int side, double& elapsed, int& zoneSide);
    Vec3 getArtilleryPosition(int side) const;              // [INFERRED] name; PE 10128E50
    // The flee position recorded for the member's side and vehicle kind (type bit 21 or not).
    // PE 10128E80 / 10128F40; the setter name is attested by this header's earlier note.
    bool getValidArtilleryFleePos(SquadMember* member, Vec3& position);
    void setValidArtilleryFleePos(SquadMember* member, Vec3 position);
    SquadMember* getSquadMember(int playerId, int side);
    SquadCoordinator* getSquadFromPlayer(int playerId, int side);
    // Hands a target a bot engages to the rest of the player's squad on the one-based side; a
    // player in no squad is ignored. PE 1012B430; [INFERRED] name.
    void shareTarget(int playerId, int side, unsigned int target);
    int getSquadStrengths(int side, int areaIndex, StrategicStrengthType type);

    // Clears messages and position records by erasing each container node individually.
    void clearCommanderMessages();
    // Queues a commander message for the side's squad unless one is already queued for it; it is
    // due `delay` seconds after the current game time. [INFERRED] name. PE 1012B920.
    // An order the engine reports for a player's squad: the order position is read back from the
    // squad courier (commander or squad order) and becomes the squad's pending mission, or its
    // repair request for type 3. Players in no squad are ignored. [INFERRED] name. PE 1012ACB0.
    void receiveOrder(int type, int playerId, bool commander);
    // A human squad leader asks for orders: with a bot commander and a squad on a mission whose
    // pending order came from a commander, the order is given again (attack for type 0, defend
    // otherwise) and a commander message is queued for two seconds. The squad id is not
    // range-checked. [INFERRED] name. PE 1012BA30.
    void requestNewOrders(int side, int squadId);
    void queueCommanderMessage(int side, int squadIndex, int type, double delay);
    void clearPlayerValidPositions();

    static SquadManager* m_instance;

    // Queued commander message. Field and enumerator names are inferred.
    // Native dueTime offset/record size: ELF +0xC/20 bytes, PE +0x10/24 bytes.
    enum CommanderMessageType : int;
    class CommanderMessage
    {
    public:
        CommanderMessageType m_type;
        int m_squadIndex;
        int m_sideIndex;
        double m_dueTime;
    };

    // Three Vec3 values, not containers: incomingArtillery
    // and setValidArtilleryFleePos store their scalar components.
    // [INFERRED] field names. ELF offsets below; PE offsets are 0,8,16,32,40,41,44,56.
    // Only the three positions are zeroed here. The manager tail clears m_b0; both doubles
    // and the two validity flags remain unwritten by retail construction.
    class SideBlock
    {
    public:
        /*  +0 */ bool m_b0;
        /*  +4 */ double m_artilleryStartTime;
        /* +12 */ Vec3 m_artilleryPosition{};
        /* +24 */ double m_artilleryDelay;
        /* +32 */ bool m_validFlee0;
        /* +33 */ bool m_validFlee1;
        /* +36 */ Vec3 m_fleePos0{};
        /* +48 */ Vec3 m_fleePos1{};
    };

private:
    // no vtable. The constructor stores no vptr on any path, so this class has no virtuals at
    // all - checked before writing, because a missing vptr shifts every offset below by four.
    /*   0 */ std::vector<SquadCoordinator*> m_coordinators[2]; // ELF stride 12 / PE stride 16
    /*  24 */ int  m_squadCursor;         // = 0; [INFERRED] name, update uses unsigned comparison
    /*  28 */ int  m_sideCursor;          // = 1; [INFERRED] name, update alternates zero/nonzero
    // Per-side state blocks, each 60 bytes in the original layout. Construction clears
    // the leading flag and the same nine words in each block.
    /*  32 */ SideBlock m_sideBlock0;     // +32..+91
    /*  92 */ SideBlock m_sideBlock1;     // +92..+151
    /* 152 */ float  m_float152;          // = 40.0f (bit pattern )
    /* 156 */ double m_double156;         // = 25.0
    // with a bare `operator delete` one at a time.
    /* 164 */ std::list<CommanderMessage*> m_commanderMessages;
    // Maps player ids to their first invalid-position timestamp; a valid position removes the entry.
    /* 168 */ std::map<int, double> m_playerValidPositions;
    // The original ELF map includes its empty comparator at +176 and ends at +180.
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADMANAGER_H
