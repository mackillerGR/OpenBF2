// Strategy, prerequisite and condition bodies recovered from AIDLL_w32ded.dll.
// [INFERRED] placement; see Strategy.h.

#include "Strategy.h"

#include <cmath>
#include <string>

#include "AIDllFrontier.h"
#include "StrategyManager.h"
#include "../AIUtils/checkPointer.h"
#include "../AIStrategic/StatusMap.h"

namespace dice {
namespace hfe {
namespace ai {

// PE 10194580.
TemperatureTreeModifiers::TemperatureTreeModifiers()
{
    for (int i = 0; i < 41; ++i)
        m_node[i] = 1.0f;
    for (int i = 0; i < 56; ++i)
        for (int j = 0; j < 6; ++j)
            m_combatStrength[i][j] = 1.0f;
}

// PE 100DCDE0.
void TemperatureTreeModifiers::setCombatStrength(int node, int type, float value)
{
    if (type == 6)
    {
        for (int j = 0; j < 6; ++j)
            m_combatStrength[node][j] = value;
    }
    else
    {
        m_combatStrength[node][type] = value;
    }
}

// PE 100DCE50.
float TemperatureTreeModifiers::getCombatStrength(int node, int type) const
{
    return m_combatStrength[node][type];
}

// PE 100DE470. The abort flag and strength start cleared.
StrategyCondition::StrategyCondition(const std::string& name, float targetValue, Compare compare,
                                     ConditionSide side, CompareType compareType,
                                     Conditional conditional)
    : m_isAbortCondition(false)
    , m_targetValue(targetValue)
    , m_compare(compare)
    , m_compareType(compareType)
    , m_side(side)
    , m_conditional(conditional)
    , m_strength(Required)
    , m_name(name)
{
}

// PE 100DE4D0.
StrategyCondition::~StrategyCondition()
{
}

// PE 100DE4C0.
int StrategyCondition::getType() const
{
    return 4;
}

// PE 100DE540. Difference* reuse the Equal* arms.
float StrategyCondition::crispCompare(float lhs, float rhs)
{
    switch (m_compare)
    {
    case Equal:
    case Difference:
        return (lhs - rhs == m_targetValue) ? 1.0f : -1.0f;
    case EqualGreater:
    case DifferenceGreater:
        return (lhs - rhs < m_targetValue) ? -1.0f : 1.0f;
    case EqualSmaller:
    case DifferenceSmaller:
        return (lhs - rhs > m_targetValue) ? -1.0f : 1.0f;
    case Quotient:
        if (rhs == 0.0f)
            return -1.0f;
        return (lhs / rhs == m_targetValue) ? 1.0f : -1.0f;
    case QuotientGreater:
        if (rhs == 0.0f)
            return 1.0f;
        return (lhs / rhs < m_targetValue) ? -1.0f : 1.0f;
    case QuotientSmaller:
        if (rhs == 0.0f)
            return -1.0f;
        return (lhs / rhs > m_targetValue) ? -1.0f : 1.0f;
    default:
        return 0.0f;
    }
}

// PE 100DE680.
float StrategyCondition::fuzzyCompare(float lhs, float rhs)
{
    switch (m_compare)
    {
    case Equal:
    case Difference:
        return std::fabs(lhs - rhs - m_targetValue) * -1.0f;
    case EqualGreater:
    case DifferenceGreater:
        return lhs - rhs - m_targetValue;
    case EqualSmaller:
    case DifferenceSmaller:
        return (lhs - rhs - m_targetValue) * -1.0f;
    case Quotient:
        if (rhs == 0.0f)
            return -1000.0f;
        return std::fabs(lhs / rhs - m_targetValue) * -1.0f;
    case QuotientGreater:
        if (rhs == 0.0f)
            return 1000.0f;
        return lhs / rhs - m_targetValue;
    case QuotientSmaller:
        if (rhs == 0.0f)
            return -1000.0f;
        return (lhs / rhs - m_targetValue) * -1.0f;
    default:
        return 0.0f;
    }
}

// PE 100DE7B0.
float StrategyCondition::sign(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (value < 0.0f)
        return -1.0f;
    return 0.0f;
}

// PE 100DE800.
float StrategyCondition::compare(float lhs, float rhs)
{
    switch (m_compareType)
    {
    case Crisp:
        return crispCompare(lhs, rhs);
    case Fuzzy:
        return fuzzyCompare(lhs, rhs);
    case FuzzySqr:
    {
        const float value = fuzzyCompare(lhs, rhs);
        return sign(value) * value * value;
    }
    default:
        return 0.0f;
    }
}

// PE 1014CBD0.
ConstantCondition::ConstantCondition(const std::string& name, CompareType compareType,
                                     Compare compare, ConditionSide side, Conditional conditional,
                                     float value, float targetValue)
    : StrategyCondition(name, targetValue, compare, side, compareType, conditional)
    , m_value(value)
{
}

// PE 1014CC10.
int ConstantCondition::getType() const
{
    return 0;
}

// PE 1014CC60. Any side other than Friendly reads the enemy array; the side argument is unused.
float ConstantCondition::evaluate(float* friendlyState, float* enemyState, int /*side*/)
{
    float* const state = (m_side != Friendly) ? enemyState : friendlyState;
    const float result = compare(state[m_conditional], m_value);
    StrategyManager::getInstance()->isDebugEnabled();
    return result;
}

// PE 1014C570. The constructor body also clears the already-empty list.
StrategyPrerequisite::StrategyPrerequisite(const std::string& name)
    : m_conditions()
    , m_name(name)
{
    m_conditions.clear();
}

// PE 1014CA00.
void StrategyPrerequisite::addCondition(StrategyCondition* condition, float weight)
{
    if (condition)
        m_conditions.push_back(std::make_pair(condition, weight));
}

// PE 1014C860.
float StrategyPrerequisite::evaluate(float* friendlyState, float* enemyState, bool isActive,
                                     int side)
{
    float sum = 0.0f;
    for (std::list<std::pair<StrategyCondition*, float> >::iterator it = m_conditions.begin();
         it != m_conditions.end(); ++it)
    {
        StrategyCondition* const condition = it->first;
        if (!isActive && condition->isAbortCondition())
            continue;

        float value = condition->evaluate(friendlyState, enemyState, side) * it->second;
        switch (condition->getStrength())
        {
        case StrategyCondition::Required:
        case StrategyCondition::RequiredPositive:
            if (value < 0.0f)
                return 0.0f;
            break;
        case StrategyCondition::RequiredNegative:
            if (value > 0.0f)
                return 0.0f;
            break;
        case StrategyCondition::AdvisoryPositive:
            if (value < 0.0f)
                value = 0.0f;
            break;
        case StrategyCondition::AdvisoryNegative:
            if (value > 0.0f)
                value = 0.0f;
            break;
        default:
            break;
        }
        sum = value + sum;
    }

    StrategyManager::getInstance()->isDebugEnabled();
    return sum;
}

// PE 100DE3A0. The attack, defence and two unresolved counters are left uninitialized.
Strategy::Strategy(const std::string& name)
    : m_treeModifiers(new TemperatureTreeModifiers())
    , m_aggression(0.5f)
    , m_float18(0.0f)
    , m_prerequisiteWeight(1.0f)
    , m_timeLimit(1e9f)
    , m_prerequisite(0)
    , m_abortPrerequisite(0)
    , m_requiredPreceding()
    , m_prohibitedPreceding()
    , m_specificObjectModifiers()
    , m_name(name)
{
    clearAllStrategicObjectsModifiers();
    m_specificObjectModifiers.resize(0, 0.0f);
}

// [TEMP] [CHOSEN, not recovered] Strategies are destroyed only by StrategyManager::reset, which
// remains a frontier. [RETIRE-WHEN: strategy-dtor] Replace with the PE destructor body once the
// manager teardown is recovered and a level reload exercises it.
Strategy::~Strategy()
{
    BF2_AI_FRONTIER("dice::hfe::ai::Strategy::~Strategy");
}

// PE 100DDE70.
float Strategy::getHeat(float* friendlyState, float* enemyState, Strategy* activeStrategy,
                        int side)
{
    StrategyManager::getInstance()->isDebugEnabled();

    if (!m_requiredPreceding.empty()
        && m_requiredPreceding.find(activeStrategy) == m_requiredPreceding.end())
        return 0.0f;

    if (!m_prohibitedPreceding.empty()
        && m_prohibitedPreceding.find(activeStrategy) != m_prohibitedPreceding.end())
        return 0.0f;

    return m_prerequisite->evaluate(friendlyState, enemyState, activeStrategy == this, side)
         * m_prerequisiteWeight;
}

// PE 100DD090.
void Strategy::setStrategicObjectsModifier(int type, float value)
{
    m_objectsModifier[0][type] = value;
    m_objectsModifier[1][type] = value;
    m_objectsModifier[2][type] = value;
}

// PE 100DD680.
void Strategy::setStrategicObjectsModifier(int type, float value, int status)
{
    m_objectsModifier[status][type] = value;
}

// PE 100DD130.
void Strategy::clearAllStrategicObjectsModifiers()
{
    for (int status = 0; status < 3; ++status)
        for (int type = 0; type < 27; ++type)
            m_objectsModifier[status][type] = 1.0f;
}

// Extractors. Each reads one token and compares exact spellings in the shipped order.

// PE 100DA8A0.
std::istream& operator>>(std::istream& stream, StrategyCondition::CompareType& value)
{
    std::string token;
    stream >> token;
    if      (token == "Crisp") value = StrategyCondition::Crisp;
    else if (token == "Fuzzy") value = StrategyCondition::Fuzzy;
    else value = static_cast<StrategyCondition::CompareType>(token == "FuzzySqr" ? 2 : 3);
    return stream;
}

// PE 100DACE0. Each value also accepts its operator spelling.
std::istream& operator>>(std::istream& stream, StrategyCondition::Compare& value)
{
    std::string token;
    stream >> token;
    if      (token == "Equal"             || token == "=")   value = StrategyCondition::Equal;
    else if (token == "EqualGreater"      || token == ">=")  value = StrategyCondition::EqualGreater;
    else if (token == "EqualSmaller"      || token == "<=")  value = StrategyCondition::EqualSmaller;
    else if (token == "Quotient"          || token == "/")   value = StrategyCondition::Quotient;
    else if (token == "QuotientGreater"   || token == ">/")  value = StrategyCondition::QuotientGreater;
    else if (token == "QuotientSmaller"   || token == "</")  value = StrategyCondition::QuotientSmaller;
    else if (token == "Difference"        || token == "-")   value = StrategyCondition::Difference;
    else if (token == "DifferenceGreater" || token == ">-")  value = StrategyCondition::DifferenceGreater;
    else if (token == "DifferenceSmaller" || token == "<-")  value = StrategyCondition::DifferenceSmaller;
    else value = static_cast<StrategyCondition::Compare>(9);
    return stream;
}

// PE 100DA940.
std::istream& operator>>(std::istream& stream, StrategyCondition::ConditionSide& value)
{
    std::string token;
    stream >> token;
    if      (token == "Friendly") value = StrategyCondition::Friendly;
    else if (token == "Enemy")    value = StrategyCondition::Enemy;
    else value = static_cast<StrategyCondition::ConditionSide>(token == "Both" ? 2 : 3);
    return stream;
}

// PE 100DA7B0.
std::istream& operator>>(std::istream& stream, StrategyCondition::ConditionStrength& value)
{
    std::string token;
    stream >> token;
    if      (token == "Required")         value = StrategyCondition::Required;
    else if (token == "RequiredPositive") value = StrategyCondition::RequiredPositive;
    else if (token == "RequiredNegative") value = StrategyCondition::RequiredNegative;
    else if (token == "Advisory")         value = StrategyCondition::Advisory;
    else if (token == "AdvisoryPositive") value = StrategyCondition::AdvisoryPositive;
    else value = static_cast<StrategyCondition::ConditionStrength>(
        token == "AdvisoryNegative" ? 5 : 6);
    return stream;
}

// PE 100DAEF0. "Infantery" is accepted as a second spelling of Infantry.
std::istream& operator>>(std::istream& stream, StrategyCondition::Conditional& value)
{
    typedef StrategyCondition C;
    std::string token;
    stream >> token;
    if      (token == "Ticket")                value = C::Ticket;
    else if (token == "ControlPoint")          value = C::ControlPoint;
    else if (token == "Time")                  value = C::Time;
    else if (token == "StartTime")             value = C::StartTime;
    else if (token == "Attacks")               value = C::Attacks;
    else if (token == "Defences")              value = C::Defences;
    else if (token == "Carrier")               value = C::Carrier;
    else if (token == "HeavyNaval")            value = C::HeavyNaval;
    else if (token == "Naval")                 value = C::Naval;
    else if (token == "Bomber")                value = C::Bomber;
    else if (token == "Fighter")               value = C::Fighter;
    else if (token == "AntiAir")               value = C::AntiAir;
    else if (token == "Air")                   value = C::Air;
    else if (token == "LCVP")                  value = C::LCVP;
    else if (token == "Infantry" || token == "Infantery") value = C::Infantry;
    else if (token == "Artillery")             value = C::Artillery;
    else if (token == "AntiTank")              value = C::AntiTank;
    else if (token == "HeavyAttack")           value = C::HeavyAttack;
    else if (token == "FastAttack")            value = C::FastAttack;
    else if (token == "NumberOfFriendlyAreas") value = C::NumberOfFriendlyAreas;
    else if (token == "NumberOfNeutralAreas")  value = C::NumberOfNeutralAreas;
    else if (token == "NumberOfHostileAreas")  value = C::NumberOfHostileAreas;
    else if (token == "Flank")                 value = C::Flank;
    else if (token == "Base")                  value = C::Base;
    else if (token == "Close")                 value = C::Close;
    else if (token == "Centre")                value = C::Centre;
    else if (token == "Remote")                value = C::Remote;
    else if (token == "North")                 value = C::North;
    else if (token == "East")                  value = C::East;
    else if (token == "South")                 value = C::South;
    else if (token == "West")                  value = C::West;
    else if (token == "Route")                 value = C::Route;
    else if (token == "Bridge")                value = C::Bridge;
    else if (token == "FrontFlank")            value = C::FrontFlank;
    else if (token == "FrontBase")             value = C::FrontBase;
    else if (token == "FrontClose")            value = C::FrontClose;
    else if (token == "FrontCentre")           value = C::FrontCentre;
    else if (token == "FrontRemote")           value = C::FrontRemote;
    else if (token == "FrontNeutral")          value = C::FrontNeutral;
    else if (token == "Front")                 value = C::Front;
    else if (token == "Safe")                  value = C::Safe;
    else if (token == "EnemyObject")           value = C::EnemyObject;
    else if (token == "UnReachable")           value = C::UnReachable;
    else value = static_cast<C::Conditional>(token == "UnitConstant" ? 43 : 44);
    return stream;
}

// PE 100DC4B0. The final "Unknown" comparison is made but its result is discarded.
std::istream& operator>>(std::istream& stream, StatusMap::Status& status)
{
    std::string token;
    stream >> token;
    if      (token == "Owned")   status = StatusMap::Owned;
    else if (token == "Hostile") status = StatusMap::Hostile;
    else if (token == "Neutral") status = StatusMap::Neutral;
    else
    {
        (void)(token == "Unknown");
        status = StatusMap::Unknown;
    }
    return stream;
}

} // namespace ai
} // namespace hfe
} // namespace dice
