// Strategies, their prerequisites and the conditions they weigh, as built by aiStrategy scripts.
// [INFERRED] placement: the image carries no __FILE__ string for these classes.
// Offsets in the comments are the retail PE (x86, MSVC) layout.

#ifndef AI_AIDLL_AI_MAIN_STRATEGY_H
#define AI_AIDLL_AI_MAIN_STRATEGY_H

#include <istream>
#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// Per-strategy scale factors for the temperature tree. Every factor starts at 1.
// [INFERRED] class name; the PE allocates it as a separate 0x5E4-byte block.
class TemperatureTreeModifiers
{
public:
    TemperatureTreeModifiers();

    // Type 6 writes all six combat-strength entries of the node; others write one entry.
    void setCombatStrength(int node, int type, float value);
    float getCombatStrength(int node, int type) const;

    /* +0x000 */ float m_node[41];
    // Row count follows the allocation size; scripts address only the first 41 rows.
    /* +0x0A4 */ float m_combatStrength[56][6];
};

class StrategyCondition
{
public:
    enum CompareType { Crisp, Fuzzy, FuzzySqr };
    enum Compare
    {
        Equal, EqualGreater, EqualSmaller,
        Quotient, QuotientGreater, QuotientSmaller,
        Difference, DifferenceGreater, DifferenceSmaller
    };
    enum ConditionSide { Friendly, Enemy, Both };
    enum ConditionStrength
    {
        Required, RequiredPositive, RequiredNegative,
        Advisory, AdvisoryPositive, AdvisoryNegative
    };
    // Index into the SAI state arrays; 44 is the extractor's unknown value.
    enum Conditional
    {
        Carrier, HeavyNaval, LCVP, Naval, Bomber, Fighter, AntiAir, Air, Infantry, Artillery,
        AntiTank, HeavyAttack, FastAttack, Ticket, ControlPoint, Time, StartTime, Attacks,
        Defences, NumberOfFriendlyAreas, NumberOfNeutralAreas, NumberOfHostileAreas, Flank, Base,
        Close, Centre, Remote, Route, Bridge, North, West, South, East, FrontFlank, FrontBase,
        FrontClose, FrontCentre, FrontRemote, FrontNeutral, Front, Safe, EnemyObject, UnReachable,
        UnitConstant
    };

    StrategyCondition(const std::string& name, float targetValue, Compare compare,
                      ConditionSide side, CompareType compareType, Conditional conditional);
    virtual ~StrategyCondition();

    // 0 constant, 1 homogenous, 2 heterogeneous; the base reports 4.
    virtual int getType() const;
    // Evaluates against the friendly and enemy state arrays for the given side.
    virtual float evaluate(float* friendlyState, float* enemyState, int side) = 0;

    void setStrength(ConditionStrength strength) { m_strength = strength; }
    ConditionStrength getStrength() const { return m_strength; }
    bool isAbortCondition() const { return m_isAbortCondition; }
    void setIsAbortCondition(bool value) { m_isAbortCondition = value; }

protected:
    // Dispatches on the compare type; unknown types yield zero.
    float compare(float lhs, float rhs);
    // Returns 1 when the relation holds and -1 otherwise.
    float crispCompare(float lhs, float rhs);
    // Returns a signed distance from the target; division by zero saturates at +/-1000.
    float fuzzyCompare(float lhs, float rhs);
    // Returns 1, -1 or 0 for positive, negative and other values.
    static float sign(float value);

    /* +0x04 */ bool m_isAbortCondition;          // [INFERRED] name; = false
    /* +0x08 */ float m_targetValue;
    /* +0x0C */ Compare m_compare;
    /* +0x10 */ CompareType m_compareType;
    /* +0x14 */ ConditionSide m_side;
    /* +0x18 */ Conditional m_conditional;
    /* +0x1C */ ConditionStrength m_strength;     // = Required
    /* +0x20 */ std::string m_name;
};

// Compares one state entry with a constant. PE vtable 10280804.
class ConstantCondition : public StrategyCondition
{
public:
    ConstantCondition(const std::string& name, CompareType compareType, Compare compare,
                      ConditionSide side, Conditional conditional, float value,
                      float targetValue);

    virtual int getType() const;
    virtual float evaluate(float* friendlyState, float* enemyState, int side);

private:
    /* +0x3C */ float m_value;
};

// Weighted list of conditions; no vptr.
class StrategyPrerequisite
{
public:
    explicit StrategyPrerequisite(const std::string& name);

    // Ignores a null condition.
    void addCondition(StrategyCondition* condition, float weight);
    // Sums weighted condition values. A failed required condition makes the whole result zero;
    // abort conditions are skipped unless the owning strategy is the active one.
    float evaluate(float* friendlyState, float* enemyState, bool isActive, int side);

    /* +0x00 */ std::list<std::pair<StrategyCondition*, float> > m_conditions;
    /* +0x0C */ std::string m_name;
};

class Strategy
{
public:
    explicit Strategy(const std::string& name);
    ~Strategy();

    // Heat is zero when the active strategy breaks the preceding-strategy rules; otherwise the
    // prerequisite's weighted value. The prerequisite must have been set.
    float getHeat(float* friendlyState, float* enemyState, Strategy* activeStrategy, int side);

    // Writes the modifier for all three statuses.
    void setStrategicObjectsModifier(int type, float value);
    void setStrategicObjectsModifier(int type, float value, int status);
    // The 27 per-type modifiers for one status. PE 100DD110.
    const float* getStrategicObjectsModifiers(int status) const { return m_objectsModifier[status]; }
    // Modifier for one strategic-object index; 1 outside the list. PE 100DD8B0.
    float getSpecificObjectModifier(int index) const;
    // Resets every strategic-object modifier to 1.
    void clearAllStrategicObjectsModifiers();

    float getAggression() const { return m_aggression; }
    void setAggression(float value) { m_aggression = value; }
    int getNumberOfAttacks() const { return m_numberOfAttacks; }
    void setNumberOfAttacks(int value) { m_numberOfAttacks = value; }
    int getNumberOfDefences() const { return m_numberOfDefences; }
    void setNumberOfDefences(int value) { m_numberOfDefences = value; }
    float getTimeLimit() const { return m_timeLimit; }
    void setTimeLimit(float value) { m_timeLimit = value; }
    void setPrerequisite(StrategyPrerequisite* prerequisite) { m_prerequisite = prerequisite; }

    /* +0x000 */ TemperatureTreeModifiers* m_treeModifiers;
    /* +0x004 */ float m_aggression;                 // = 0.5
    /* +0x008 */ int m_numberOfAttacks;              // not initialized by the constructor
    /* +0x00C */ int m_numberOfDefences;             // not initialized by the constructor
    /* +0x010 */ int m_int10;                        // [UNRESOLVED] accessor pair, uninitialized
    /* +0x014 */ int m_int14;                        // [UNRESOLVED] accessor pair, uninitialized
    /* +0x018 */ float m_float18;                    // [UNRESOLVED] = 0; abort-value base
    /* +0x01C */ float m_prerequisiteWeight;         // = 1
    // Scales the minimum time before the SAI reconsiders an unchanged choice.
    /* +0x020 */ float m_timeLimit;                  // = 1e9
    /* +0x024 */ StrategyPrerequisite* m_prerequisite;
    /* +0x028 */ StrategyPrerequisite* m_abortPrerequisite;   // [INFERRED] name
    /* +0x030 */ std::set<Strategy*> m_requiredPreceding;
    /* +0x03C */ std::set<Strategy*> m_prohibitedPreceding;
    // Indexed [status][type] with 27 object types and three statuses.
    /* +0x048 */ float m_objectsModifier[3][27];
    /* +0x18C */ std::vector<float> m_specificObjectModifiers;  // [UNRESOLVED] element type
    /* +0x19C */ std::string m_name;
};

// Token extractors; an unrecognised token yields the enumeration's count.
std::istream& operator>>(std::istream& stream, StrategyCondition::CompareType& value);
std::istream& operator>>(std::istream& stream, StrategyCondition::Compare& value);
std::istream& operator>>(std::istream& stream, StrategyCondition::ConditionSide& value);
std::istream& operator>>(std::istream& stream, StrategyCondition::ConditionStrength& value);
std::istream& operator>>(std::istream& stream, StrategyCondition::Conditional& value);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_STRATEGY_H
