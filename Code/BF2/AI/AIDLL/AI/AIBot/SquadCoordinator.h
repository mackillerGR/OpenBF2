// Coordinates squad membership, leadership and its owned formation.
// [FRONTIER] Leader-change movement remains incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H

#include <list>
#include <vector>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
enum KitType : int;
namespace ai {

class SquadFormation;
class SquadMember;
class IAIPlayer;
class AIStrategicArea;
class AIStrategicObject;
enum StrategicStrengthType : int; // Attested type name; enum labels remain unrecovered.

class SquadCoordinator
{
public:
    // attested nested type name: `std::list<SquadCoordinator::SquadMemberFire*>` insert/erase/clear
    // are instantiated. on the ELF (`shotsFired` allocates 0x44), four Vec3s zeroed by
    // the allocation; the scalar members are written field by field. Roles come from shotsFired
    // (, ), removeSquadMemberFire and updateShotsFired; names are [INFERRED].
    // Nothing in this tree allocates one yet, so the list stays empty on every exercised path.
    struct SquadMemberFire
    {
        /* +0x00 */ double m_time;           // expiry: game time + 10 s, restamped on each shot
        /* +0x08 */ Vec3 m_v8;               // the shotsFired Vec3 argument [UNRESOLVED role]
        /* +0x14 */ Vec3 m_v20;              // seeded from it too; refreshed conditionally [UNRESOLVED]
        /* +0x20 */ int m_playerId;          // the shotsFired int; the search/removal key
        /* +0x24 */ SquadMember* m_member;   // getSquadMember(playerId, index), borrowed
        /* +0x28 */ Vec3 m_v40;              // shooter position at the latest shot [UNRESOLVED role]
        /* +0x34 */ Vec3 m_position;         // shooter position at the first shot; the 5 m anchor
        /* +0x40 */ int m_i64;               // 1, or the shooting bot's slot +0x80 answer [UNRESOLVED]
    };

    SquadCoordinator(int side, int squadIndex);

    void update(double time);                    // complete order
    void updateSquadPosition();                  // complete
    void updateShotsFired();                     // complete
    void updateRepairRequest(double time);       // PE inlined at ; complete
    void updateNeedOrdersMessage(double time);   // radio-enqueue seam
    void updateIncomingMessages();               // radio-enqueue seam
    bool isSquadTakingFlag();                    // complete
    void updateSquadManeuver();                  // selected maneuver seams
    int chooseSquadManeuver();                   // integer payload, enum spelling unresolved
    void updateSquadPaths();                     // complete invalidation loop
    int getSide();
    int getMissionStatus();                       // integer payload, enum spelling unresolved
    bool isSquadAssignedToArea(int areaIndex);
    int getStrategicStrength(StrategicStrengthType type); // nonnull vehicle seam
    void getSquadPosition(Vec3& position);       // inferred void
    // The position getSquadPosition last stored. [INFERRED] name. PE 101A1880.
    void getLastSquadPosition(Vec3& position) const { position = m_v108; }
    int getSquadLeaderIndex() const { return m_i96; }   // [INFERRED] name. PE 101A1170
    // The squad's secondary goal, set alongside the order. [INFERRED] name. PE 101A12E0.
    const Vec3& getSecondaryGotoPos() const { return m_v200; }
    // Whether the next path should be announced over the radio. [INFERRED] names.
    bool shouldAnnounceOrder() const { return m_b220; }               // PE 101A10C0
    void setAnnounceOrder(bool announce) { m_b220 = announce; }       // PE 101A10B0
    // A member is close to the goal of an area the side holds. [INFERRED] name. PE 101A2FA0.
    bool isAnyMemberNearGoal();
    // The order position of `object` for the member's vehicle. [INFERRED] name. PE 101A1A10.
    bool getObjectOrderPos(SquadMember* member, AIStrategicObject* object, Vec3& position);
    SquadMember* getSquadLeader();
    int getNumberOfSquadMembers();
    int getSquadId();
    bool squadHasPosition();
    SquadFormation* getSquadFormation();         // borrowed
    const Vec3& getSquadGotoPos();               // borrowed, return spelling inferred
    void addSquadMember(IAIPlayer* player);
    void removeSquadMember(int index);          // PE 101A45A0
    void reset();                               // [INFERRED] name; PE 101A2470
    void removeSquadMember(SquadMember* member); // PE 101A4730: by player id
    void removeSquadMemberFire(SquadMember* member); // PE 101A2D30
    bool getMemberOrderPos(SquadMember* member, Vec3& position); // [INFERRED] name; PE 101A1960
    void updateSquadLeader();                   // change seam
    void unAssignSquad();                       // PE inlined at
    SquadMember* getSquadMember(int index);
    SquadMember* getSquadMember(int playerId, int& index);
    bool hasMemberWithKit(KitType type);
    bool isSquadAssignedToAnyArea();
    bool hasNoMission();                         // mission status 0; [INFERRED] name
    bool isSquadPendingAssignedToArea(int areaIndex); // [INFERRED] name
    AIStrategicArea* getSquadStrategicAreaAssignment(); // borrowed
    void resetNeedOrderMessageTimer(double now);
    // Whether the member's vehicle, or on foot its soldier, can stand at the position on the
    // navigation mesh. [INFERRED] name.
    bool isPositionValidForMember(SquadMember* member, const Vec3& position);
    // Up to eight random tries in a square of the given half-width around the position, at
    // terrain height, for a position valid for the member; the position itself first.
    // [INFERRED] name. PE 101A1F40.
    bool findValidPositionNear(SquadMember* member, Vec3& position, float halfWidth);
    // Takes the pending mission: onto the closest strategic object when the position lies in
    // it or within its reach and the side holds it, otherwise onto a valid nearby position.
    // Announces and confirms it through the squad courier, or declines it. [INFERRED] name.
    // PE 101A3120.
    bool acceptMission(SquadMember* member);
    // Records an order as the pending mission. A commander order is refused while a bot
    // commander's squad is busy (near its goal or on a mission); a squad order from a human
    // leader becomes the secondary goal. [INFERRED] name. PE 101A3000.
    void setMission(int type, const Vec3& position, int value, bool commander);
    // Starts a repair request at the position, due in 60 seconds. [INFERRED] name. PE 101A1750.
    void requestRepair(const Vec3& position);
    // A copy of the pending mission block. [INFERRED] name. PE 101A1D60.
    struct PendingMission
    {
        AIStrategicArea* area;     // PE +0x00
        Vec3 position;             // PE +0x04
        int type;                  // PE +0x10
        int value;                 // PE +0x14
        double time;               // PE +0x18 [UNRESOLVED]
        bool commander;            // PE +0x20
    };
    void getPendingMission(PendingMission& mission) const;
    // Clears every member's path. PE 101A2BF0; [INFERRED] name.
    void clearMemberPaths();
    // Whether the member's vehicle finds material 4 at the position; true without a vehicle.
    // [INFERRED] name.
    bool isMaterialFourAt(SquadMember* member, const Vec3& position);
    bool isMissionStatusTwo();                   // [INFERRED] name
    bool getSquadGotoPos(Vec3& position);        // only while m_i44 is zero; [INFERRED] spelling

    // layout, on the ELF, read store by store out of the constructor
    // The [UNRESOLVED] gaps hold position and claim nothing; the constructor never writes them.
    /*   +0 */ std::vector<SquadMember*> m_squadMembers;   // `getSquadMember` indexes it, unchecked
    /*  +12 */ std::list<SquadMemberFire*> m_list12;       // owned fire records; see SquadMemberFire
    /*  +16 */ int m_squadIndex;               // = the constructor's second argument
    /*  +20 */ int m_side;                     // = the first argument; `getSide` returns it
    /*  +24 */ AIStrategicArea* m_strategicAreaAssignment; // [INFERRED] name; ELF +0x18 / PE +0x28, borrowed; initially null
    /*  +28 */ Vec3 m_v28;                     // zeroed [UNRESOLVED]
    /*  +40 */ int m_i40;                      // = 4 [UNRESOLVED]
    /*  +44 */ int m_i44;                      // = 0 [UNRESOLVED]
    /*  +48 */ double m_d48;                   // = -20.0 a "never happened" sentinel, not a time
    // Copied from m_b92 when a mission is taken; the constructor never writes either flag.
    /*  +56 */ bool m_b56;                     // [UNRESOLVED] PE +0x48
    /*  +57 */ char m_pad57[3];
    // Twin of m_strategicAreaAssignment read at PE +0x50 by 101A1280. [INFERRED] name.
    /*  +60 */ AIStrategicArea* m_pendingArea;  // = 0, borrowed
    /*  +64 */ Vec3 m_v64;                     // zeroed [UNRESOLVED]
    /*  +76 */ int m_i76;                      // = 4 [UNRESOLVED] - the twin of m_i40
    /*  +80 */ int m_i80;                      // = 0 [UNRESOLVED]
    // The pending mission's double and flag; the constructor never writes them.
    /*  +84 */ double m_d84;                   // [UNRESOLVED] PE +0x68
    /*  +92 */ bool m_b92;                     // [UNRESOLVED] PE +0x70
    /*  +93 */ char m_pad93[3];
    /*  +96 */ int m_i96;                      // leader member index, initially -1
    /* +100 */ SquadFormation* m_formation;    // new SquadFormation(this)
    /* +104 */ std::list<int> m_list104;       // header only; 4-byte element [UNRESOLVED]
    /* +108 */ Vec3 m_v108;                    // zeroed twice - once before the formation, once after
    /* +120 */ int m_i120;                     // = 0 [UNRESOLVED]
    /* +124 */ double m_d124;                  // = 0.0 [UNRESOLVED]
    /* +132 */ double m_d132;                  // = 0.0 [UNRESOLVED]
    /* +140 */ double m_d140;                  // = 0.0 [UNRESOLVED]
    /* +148 */ int m_i148;                     // selected maneuver payload; initially 0
    /* +152 */ int m_i152;                     // = 0 [UNRESOLVED]
    /* +156 */ bool m_b156;                    // = false [UNRESOLVED]
    /* +157 */ bool m_b157;                    // = false [UNRESOLVED]
    /* +160 */ std::vector<Vec3> m_positions;  // seeded with two zero vectors - see the header
    // Repair request state: position, a 60-second game-time deadline, and confirmation flag.
    // Expiration clears both request and confirmation.
    /* +172 */ bool m_b172;                    // = false; isRepairRequested
    /* +176 */ Vec3 m_v176;                    // zeroed; getRepairRequestPos returns its address
    /* +188 */ double m_d188;                  // request deadline; the constructor never writes it
    /* +196 */ bool m_b196;                    // = false; haveConfirmedRepairRequest
    /* +200 */ Vec3 m_v200;                    // zeroed twice, like m_v108
    /* +212 */ double m_needOrderMessageTimer; // = IBFTimer::getGameTime at construction
    /* +220 */ bool m_b220;                    // = false [UNRESOLVED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H
