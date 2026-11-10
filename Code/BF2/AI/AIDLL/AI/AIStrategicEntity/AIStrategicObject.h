// Base interface and shared state for strategic objects. Preserve virtual declaration order.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H

#include <list>
#include <set>
#include <istream>

// Vec2 and Vec3 cannot be forward-declared in this tree: both are typedefs to
// `dice::hfe::Math::Vec2/Vec3`, so a local `struct Vec3;` is a redefinition with a different basic
// type (C2371). The header has to come in.
#include "../../../../Math/Vec3.h"
#include "../AIStrategic/StatusMap.h"

namespace dice {
namespace hfe {
namespace ai {

class EnemyStatistics;
class ISAI;
class Strategy;

class AIStrategicObject
{
public:
    // Strategic object classifications used by console scripts. Unknown values have no printed name.
    enum StrategicObjectType
    {
        Flank          = 1,
        Centre         = 2,
        Base           = 3,
        Close          = 4,
        Remote         = 5,
        AirSpawner     = 6,
        LandSpawner    = 7,
        NavalSpawner   = 8,
        SoldierSpawner = 9,
        StrongPoint    = 10,
        ChokePoint     = 11,
        Bridge         = 12,
        AirField       = 13,
        SupplyPoint    = 14,
        Route          = 15,
        ControlPoint   = 16,
        North          = 17,
        West           = 18,
        South          = 19,
        East           = 20,
        // 21 has no name in the writer
        Front          = 22,
        Safe           = 23,
        Enemy          = 24,
        // 25 has no name in the writer
        NeutralType    = 26     // spelled "Neutral" by the writer; renamed here only because
                                // `StatusMap::Status::Neutral` would collide at this scope
    };

    AIStrategicObject(float radius, float f148);

    // the vtable, , thirty-three slots
    virtual ~AIStrategicObject();                                      // slots 0/1,

    virtual int getType() const;                                       // slot 2,
    virtual const Vec2& getPosition(int side) const;                   // slot 3,
    virtual void setPosition(int side, const Vec2& position);          // slot 4,

    // Slots 5..17, 21..25, 28, 31 and 32 are `__cxa_pure_virtual` in this class's table. Every name
    // and parameter list below is taken from the matching slot of `AIStrategicArea`'s vtable; the
    // return types are [INFERRED] from whether the override leaves its result in eax or in st0.
    virtual Vec3 getOrderPos(int a, int b) const = 0;                  // slot 5
    virtual float updateTemperature(const Strategy* strategy, int side) = 0;               // slot 6
    virtual void update(int side, ISAI* sai, bool& changed) = 0;                          // slot 7; paired Area body has no return
    virtual int  getChangedAttackDefence(int side) = 0;                                   // slot 8
    virtual float getControlPointTakeOverCounter() = 0;                                  // slot 9; ST0 float chain
    virtual bool isEnemyTakingOverBase(int side) = 0;                                     // slot 10
    virtual float calculateDefenseNeed(int side) = 0;                                     // slot 11; x87 float
    virtual float calculateAttackValue(int side) = 0;                                     // slot 12; x87 float
    virtual float calculateAttackCost(int side) = 0;                                      // slot 13; x87 float
    virtual bool isNeighbour(AIStrategicObject* object) const = 0;                        // slot 14
    virtual bool hasNeighbourWithStatus(StatusMap::Status status, int side) const = 0;    // slot 15
    virtual bool isNeighbouringObject(AIStrategicObject* object) const = 0;               // slot 16
    virtual bool hasNeighbouringObjectWithStatus(StatusMap::Status status,
                                                 int side) const = 0;                     // slot 17

    virtual int  getObjectTypeFlags() const;                           // slot 18,
    virtual bool hasObjectTypeFlag(StrategicObjectType type) const;    // slot 19,
    virtual bool hasObjectTypeFlags(int flags) const;                  // slot 20,

    virtual int  getTacticalStatus(int side) const = 0;                                   // slot 21
    virtual bool isAreaTakeable() = 0;                                                    // slot 22
    virtual bool collectSquadResources(int side, ISAI* sai) = 0;                          // slot 23
    // PE body reads the first argument as the squad count and the second as the side.
    virtual int  collectAdditionalSquadResources(int amount, int side, ISAI* sai) = 0;    // slot 24
    virtual int  releaseSurplus(int side) = 0;                                            // slot 25

    virtual float gravity(const Vec3& position, int side);             // slot 26,
    virtual float gravity(const Vec2& position, int side);             // slot 27,

    virtual bool retainBot(int side, int a, ISAI* sai) = 0;                               // slot 28

    virtual float getXExtent(int side) const;                          // slot 29,
    virtual float getZExtent(int side) const;                          // slot 30,

    virtual void setAsDirty(int side) = 0;                                                // slot 31
    virtual int  orderBot(int side, const std::list<AIStrategicObject*>& objects,
                          int a, ISAI* sai) = 0;                                          // slot 32

    int getNumberOfAssignedUnits(int side) const;
    bool isAttack(int side) const;
    // Borrowed mutable statistics at m_statistics[side - 1]; reference spelling is [INFERRED].
    EnemyStatistics& getBattleStatistics(int side) const;
    int releaseAssignedResources(int side);
    // Bot-id sets per one-based side; SAI::setActiveStrategy drains both. [INFERRED] names.
    std::set<int>& getBotSet20(int side) { return m_set20[side - 1]; }    // PE 100E8BA0
    std::set<int>& getBotSet44(int side) { return m_set44[side - 1]; }    // PE 100E8BC0
    // Erase the element at `it`, advancing `it`; false when `it` is already at the end.
    bool eraseFromBotSet20(std::set<int>::iterator& it, int side);
    bool eraseFromBotSet44(std::set<int>::iterator& it, int side);
    std::set<int>& getBotSet68(int side) { return m_set68[side - 1]; }    // PE 100E8BE0
    std::set<int>& getBotSet92(int side) { return m_set92[side - 1]; }    // PE 100E8C00
    // PE 100E9310.
    bool isInBotSet92(int id, int side) { return m_set92[side - 1].find(id) != m_set92[side - 1].end(); }
    // PE 100E9820.
    std::pair<std::set<int>::iterator, bool> insertBotSet92(int id, int side)
    {
        return m_set92[side - 1].insert(id);
    }
    // PE 100E97C0 / 100E97F0; both always report true.
    bool insertBotSet20(int id, int side) { m_set20[side - 1].insert(id); return true; }
    bool insertBotSet44(int id, int side) { m_set44[side - 1].insert(id); return true; }
    // Remove one id; false when absent. PE 100E98B0 / 100E9900.
    bool eraseIdFromBotSet20(int id, int side);
    bool eraseIdFromBotSet44(int id, int side);
    // Erase at `it` after advancing it; no end check. PE 100E9350.
    void eraseFromBotSet92(std::set<int>::iterator& it, int side);
    // Bit tests on the two masks constructed as -1 and 0. The shift count is masked to five
    // bits as the x86 instruction does. PE 100E8D20 / 100E8DB0; [INFERRED] names.
    bool allowsVehicleGroup(int group) const { return (m_i244 & (1 << (group & 31))) != 0; }
    void setAttack(bool attack, int side) { m_attack[side - 1] = attack; }   // PE 100E8AE0
    // Assigned squad strength per unit of need; 1 when nothing is needed. PE 100E8C40;
    // [INFERRED] name. The quotient is integer division.
    float getAssignedRatio(int side) const
    {
        if (m_resourceNeed[side - 1] == 0)
            return 1.0f;
        return static_cast<float>(m_assignedSquadStrength[side - 1] / m_resourceNeed[side - 1]);
    }
    bool expelsUnitType(int type) const { return (m_i248 & (1 << (type & 31))) != 0; }

    // [INFERRED] accessor names/forms. SAI::updateStates and the area's neighbour scans inline
    // these loads: status is at +0x70 + 4*side and index at +0xF0 in both shipped x86 ports.
    StatusMap::Status getStatus(int side) const { return m_status[side - 1]; }
    int getIndex() const { return m_index; }
    // Restores both live side statuses from the two stored after them. PE 100E8DE0; the name and
    // the "initial status" role of m_status[2..3] are [INFERRED].
    void resetStatus() { m_status[0] = m_status[2]; m_status[1] = m_status[3]; }

    // [CHOSEN, not recovered] Public fields allow manager access; the original may have
    // used friendship instead. Offsets document ELF layout and do not constrain modern STL sizes.
    /*   +4 */ EnemyStatistics* m_statistics[4];   // new EnemyStatistics(6), (6), (2), (2) in order
    // Per-side strategic-object sets. [UNRESOLVED] Exact roles of these four pairs remain
    // unidentified; at least two pairs represent neighboring areas and objects.
    /*  +20 */ std::set<int> m_set20[2];
    /*  +44 */ std::set<int> m_set44[2];
    /*  +68 */ std::set<int> m_set68[2]; // assigned units, one-based side accessor
    /*  +92 */ std::set<int> m_set92[2];

    // All four set to 2. `StatusMap::Status(2)` is `Neutral`, which is the only reason this member
    // carries that name - [INFERRED], on the strength of one coincidence.
    /* +116 */ StatusMap::Status m_status[4];

    // one-based: `getPosition(side)` is `m_position[side - 1]`. See the header note.
    /* +132 */ Vec2 m_position[2];

    /* +148 */ float m_f148;        // the constructor's second argument; -666.0f from every caller
    /* +152 */ float m_size[3];     // [0] = the radius argument, [1] and [2] zeroed; read by side
    /* +164 */ float m_f164[4];     // all four set to the radius argument [UNRESOLVED]
    // Paired Area::update reads/writes these as floats, not the formerly guessed ints.
    // [INFERRED] names; one-based side pairs, all zeroed by the base constructor.
    /* +180 */ float m_needCurveInput[2];
    /* +188 */ float m_friendlyRatio[2];
    /* +196 */ float m_friendlyBattleStrength[2];
    /* +204 */ float m_enemyBattleStrength[2];
    /* +212 */ int m_unresolved212[2]; // zeroed; these two preceding counts remain unidentified
    /* +220 */ int m_resourceNeed[2];
    /* +228 */ int m_assignedSquadStrength[2];
    /* +236 */ bool  m_attack[2];   // both true; isAttack(side) reads [side - 1]
    /* +240 */ int   m_index;       // not initialised here - addStrategicObject assigns it
    /* +244 */ int   m_i244;        // 1 [UNRESOLVED]
    /* +248 */ int   m_i248;        // 0 [UNRESOLVED]
    /* +252 */ int   m_category;    // 1 until createFromControlPoint or create sets it
    /* +256 */ std::set<int> m_set256;
};

// Reads one whitespace-delimited token and maps the exact spelling to its type; any other token
// yields 27, the value addObjectTypeFlag refuses. PE 100DA9E0; [INFERRED] placement.
std::istream& operator>>(std::istream& stream, AIStrategicObject::StrategicObjectType& type);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H
