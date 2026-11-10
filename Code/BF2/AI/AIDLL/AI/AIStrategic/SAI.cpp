// Strategic simulation and supporting statistics.
// [FRONTIER] Remaining strategic phases and object-query paths stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Implement and validate the remaining strategic operations.

#include "SAI.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <list>
#include <string>

#include "../Main/AIDllFrontier.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../Main/AIRandom.h"
#include "../Main/IBotManager.h"                          // updateBotPositions
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../Main/StrategyManager.h"                       // chooseStrategy
#include "../Main/AISettingsInterfaces.h"                  // IAISettings::getMaxNBots
#include "../Main/BotBehaviourManager.h"                   // getBehaviourId, by name
#include "../Main/CommonSingletonServer.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/UnitPredicate.h"
#include "../AIBot/BotCallBack.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSquad.h"
#include "../AIBot/SquadManager.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicMap/BotSpawner.h"
#include "../AIUtils/checkPointer.h"
#include "../../../AIInterface/BFKnowledge/BFCommander.h"  // request-denial callbacks
#include "../../../AIInterface/BFKnowledge/BFGame.h"       // IAIGame counters
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"     // IAIPlayer::setSpawnGroup
#include "../../../AIInterface/BFKnowledge/BFTimer.h"      // IBFTimer::getGameTime
#include "../../../../Core/Debug.h"                      // update's shipped invalid-state diagnostic

namespace dice {
namespace hfe {
namespace ai {

// The concrete callback names and signatures are ELF-attested. Their source-file placement here
// is [INFERRED]; only SAI.cpp is attested for this part of the strategic subsystem.
class SAIBotDCB : public IBotDeathCallBack
{
public:
    // The SAI pointer is non-owning, at +8 on both ports.
    SAIBotDCB(Bot* bot, ISAI* sai) : IBotDeathCallBack(bot), m_sai(sai) {}
    // PE deleting destructor : only base destruction.
    virtual ~SAIBotDCB() {}

    // Neither pointer is guarded.
    virtual void reportDeath()
    {
        m_sai->setBotIsDead(m_bot->getBotId());
    }

private:
    ISAI* m_sai;
};

class SAIBotSCB : public IBotSpawnCallBack
{
public:
    SAIBotSCB(Bot* bot, ISAI* sai) : IBotSpawnCallBack(bot), m_sai(sai) {}
    // PE deleting destructor : only base destruction.
    virtual ~SAIBotSCB() {}

    // Preserve the repeated virtual squad lookups and priority.
    // setBotIsAlive and the three recieve* callees retain their own named frontiers.
    virtual void reportSpawn()
    {
        m_sai->setBotIsAlive(m_bot->getBotId());
        if (m_bot->getBotSquad()->hasAttackOrder())
            m_bot->getBotSquad()->recieveAttackSquadOrder();
        else if (m_bot->getBotSquad()->hasDefendOrder())
            m_bot->getBotSquad()->recieveDefendSquadOrder();
        else if (m_bot->getBotSquad()->hasMoveOrderOrder())
            m_bot->getBotSquad()->recieveMoveSquadOrder();
    }

private:
    ISAI* m_sai;
};

static_assert(sizeof(void*) != 4 || sizeof(SAIBotDCB) == 12, "SAIBotDCB x86 layout");
static_assert(sizeof(void*) != 4 || sizeof(SAIBotSCB) == 12, "SAIBotSCB x86 layout");

// Running weighted statistics across size buckets. Its two array allocations are unchecked.
EnemyStatistics::EnemyStatistics(unsigned int size)
    : m_size(size)
    , m_weight(1.0f)
    , m_samples(0)
{
    m_strength = new float[size];
    m_count    = new float[m_size];

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }
}

EnemyStatistics::EnemyStatistics(unsigned int size, float weight)
    : m_size(size)
    , m_weight(weight)
    , m_samples(0)
{
    m_strength = new float[size];
    m_count    = new float[m_size];

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }
}

EnemyStatistics::~EnemyStatistics()
{
    if (m_strength)
        delete[] m_strength;

    if (m_count)
        delete[] m_count;
}

// Updates each bucket with (n * old + weight * observation) / (n + weight), using the
// entry sample count for every bucket. Integer counts are accumulated as floats.
void EnemyStatistics::sample(float* strength, int* count)
{
    const float samples = static_cast<float>(m_samples);
    const float total   = samples + m_weight;

    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = samples / total * m_strength[i];
        m_strength[i] = m_weight / total * strength[i] + m_strength[i];

        m_count[i] = static_cast<float>(m_samples) / total * m_count[i];
        m_count[i] = m_weight / total * static_cast<float>(count[i]) + m_count[i];
    }

    ++m_samples;
}

// both of these walk this object'S size, not the other's. Copying from a differently-sized
// statistics object reads past its arrays. Shipped; no check here either.
void EnemyStatistics::copy(EnemyStatistics& other)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = other.m_strength[i];
        m_count[i]    = other.m_count[i];
    }

    m_samples = other.m_samples;
}

void EnemyStatistics::add(EnemyStatistics& other)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] += other.m_strength[i];
        m_count[i]    += other.m_count[i];
    }

    m_samples += other.m_samples;
}

void EnemyStatistics::clear()
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        m_strength[i] = 0.0f;
        m_count[i]    = 0.0f;
    }

    m_samples = 0;
}

// CommanderInterface - one byte, no state, six orders. The constructor and destructor are five
// bytes each: a return.
CommanderInterface::CommanderInterface()
{
}

CommanderInterface::~CommanderInterface()
{
}


void CommanderInterface::giveAttackOrder(const Vec3&, SquadCoordinator*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveAttackOrder (SquadCoordinator)");
}

void CommanderInterface::giveMoveOrder(const Vec3&, SquadCoordinator*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveMoveOrder (SquadCoordinator)");
}

void CommanderInterface::giveDefendOrder(const Vec3&, SquadCoordinator*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveDefendOrder (SquadCoordinator)");
}

void CommanderInterface::giveAttackOrder(const Vec3&, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveAttackOrder (Bot)");
}

void CommanderInterface::giveMoveOrder(const Vec3&, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveMoveOrder (Bot)");
}

void CommanderInterface::giveDefendOrder(const Vec3&, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::CommanderInterface::giveDefendOrder (Bot)");
}


// ISAI - the interface. Its destructor is : a vptr store and a return, like every other
// bare interface destructor in this subsystem. No registration and no roster - `ISAI` is not a
// singleton, it is a per-side object `AIMain` owns by pointer.
ISAI::~ISAI()
{
}

// the unwind path the
// constructor's possible throw needs is the other hundred bytes.
SAI* ISAI::create(int side, int gridWidth, int gridHeight, Vec2 min, Vec2 max)
{
    return new SAI(side, gridWidth, gridHeight, min, max);
}

// Starts the three integer states at -1 and the float accumulator at zero.
SAI::BotInfo::BotInfo(Bot* bot)
    : m_bot(bot)
    , m_int04(-1)
    , m_int08(-1)
    , m_int0C(-1)
    , m_float10(0.0f)
    , m_double14(0.0)
{
}

// Clears both 44-element state arrays, then sets each final normalization slot to one.
void SAI::clearStates(float* a, float* b)
{
    for (int i = 0; i <= 43; ++i)
    {
        a[i] = 0.0f;
        b[i] = 0.0f;
    }

    a[43] = 1.0f;
    b[43] = 1.0f;
}

// Creates strategic state for side. Grid dimensions and bounds are retained in the signature but unused.
SAI::SAI(int side, int, int, Vec2, Vec2)
    : m_flagAC(true)
    , m_behaviourIdle(0)
    , m_behaviourMoveTo(0)
    , m_behaviourAvoid(0)
    , m_behaviourFire(0)
    , m_intD8(0)
    , m_intDC(0)
    , m_intE0(0)
    // The strategy cursor starts at `m_strategies.end`, which on a freshly built empty vector is
    // the null `finish` pointer. The binary copies the vector's finish word into it.
    , m_activeStrategy(0)
    , m_enemyStats(0)
    , m_friendlyStats(0)
    , m_int18C(19)
    , m_float190(-992.0f)
    , m_side(side)
    , m_sideMask(1 << side)
    // the other side'S BIT, and the expression only has two answers - this is a two-sided game.
    , m_enemyMask((side == 1) ? 4 : 2)
    , m_flag1C0(false)
    , m_flag1C1(false)
    , m_flag1CC(false)
    , m_flag1CD(false)
    , m_flag1CE(true)
    , m_flag1CF(false)
    , m_flag1D0(true)
    , m_flag1D1(false)
    , m_strategicObjectIterator()
    , m_stateA(0)
    , m_stateB(0)
    , m_stateC(0)
    , m_stateD(0)
    , m_int1F4(0)
    , m_commander(0)
{
    m_commander = checkPointer(new CommanderInterface());

    m_behaviourIdle   = BotBehaviourManager::m_instance->getBehaviourId(std::string("Idle"));
    m_behaviourMoveTo = BotBehaviourManager::m_instance->getBehaviourId(std::string("MoveTo"));
    m_behaviourAvoid  = BotBehaviourManager::m_instance->getBehaviourId(std::string("Avoid"));
    m_behaviourFire   = BotBehaviourManager::m_instance->getBehaviourId(std::string("Fire"));

    // four deadlines off one clock read. Three at a minute out and one at ten seconds, with the
    // fifth left at zero - so three slow reviews and one fast one, all stamped at construction.
    const double now =
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    m_time80 = now + 10.0;
    m_time68 = now + 60.0;
    m_time70 = now + 60.0;
    m_time78 = now + 60.0;
    m_time88 = 0.0;

    m_stateA = checkPointer(new float[44]);
    m_stateB = checkPointer(new float[44]);
    m_stateC = checkPointer(new float[44]);
    m_stateD = checkPointer(new float[44]);

    // the pairing is (A, C) and (B, D), not (A) and (C, D).
    clearStates(m_stateA, m_stateC);
    clearStates(m_stateB, m_stateD);

    // Not checkPointer'ed, unlike everything above.
    m_enemyStats    = new EnemyStatistics(6);
    m_friendlyStats = new EnemyStatistics(6);

    for (int i = 0; i <= 5; ++i)
    {
        m_enemyStrengths[i] = 0.0f;
        m_enemyCounts[i] = 0.0f;
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    m_botInfo.resize(settings->getMaxNBots());

    for (int i = 0; i < settings->getMaxNBots(); ++i)
        m_botInfo[i] = 0;
}

// [FRONTIER] Strategy and distribution operations remain incomplete.
// [FRONTIER] Destruction stops until ordered teardown is implemented.
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// Replace each stop when its corresponding behavior is recovered.
SAI::~SAI()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::~SAI");
}


// Map-dimension setters intentionally ignore their arguments, matching the shipped bodies.
// Level reset: clears each strategy's two heats, the strength and count tables and the
// pending deny requests, rewinds the resumable update to state 19 at -1000, restarts the timers
// (three at now + 60, one at now + 10, one at zero) and resets the squads. Always true. PE 1013DFC0.
bool SAI::reset()
{
    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin(); it != m_strategies.end(); ++it)
    {
        it->m_heat = 0.0f;
        it->m_newHeat = 0.0f;
    }
    for (int i = 0; i < 6; ++i)
    {
        m_enemyStrengths[i] = 0.0f;
        m_enemyCounts[i] = 0.0f;
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }
    m_flag1C0 = false;
    m_flag1C1 = false;
    m_intD8 = 0;
    m_intDC = 0;
    m_float190 = -1000.0f;
    m_int18C = 19;
    m_flag1CE = true;
    for (std::list<DenyRequest*>::iterator it = m_denyRequests.begin(); it != m_denyRequests.end(); )
    {
        delete *it;
        it = m_denyRequests.erase(it);
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_time68 = m_time70 = m_time78 = now + 60.0;
    m_time80 = now + 10.0;
    m_time88 = 0.0;
    SquadManager::m_instance->resetSquads();
    m_flagAC = true;
    return true;
}

void SAI::setMapXDimension(int)
{
}

void SAI::setMapYDimension(int)
{
}


// PE 10143E70. Appending may reallocate, so the active pointer is reset to the first container.
void SAI::addStrategy(Strategy* strategy)
{
    StrategyContainer container;
    container.m_heat = 0.0f;
    container.m_newHeat = 0.0f;
    container.m_startTime = 0.0f;
    container.m_count = 1;
    container.m_strategy = strategy;
    m_strategies.push_back(container);
    m_activeStrategy = &m_strategies.front();
}

// One resumable pass, with a budget check after each
// advancing stage. Equality and NaN pass the budget check; state 4's frequency gate instead
// requires an ordered strict greater-than. The prefix always runs, even for exhausted budgets.
// State 5 and 16..18 are holes. The scheduled helpers retain their own named frontiers.
void SAI::update(double budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    updateRequestDenyTimers();

    for (std::vector<BotInfo*>::iterator it = m_botInfo.begin(); it != m_botInfo.end(); ++it)
    {
        if (*it)
        {
#if defined(_MSC_VER)
            // [PLATFORM] PE reloads the entry after the timer callback and does not round
            // the interval to float before adding it to the stored float.
            const double increase = timer->getTimeIncrease();
            BotInfo* const info = *it;
            info->m_float10 = static_cast<float>(info->m_float10 + increase);
#else
            BotInfo* const info = *it;
            const float increase = static_cast<float>(timer->getTimeIncrease());
            info->m_float10 += increase;
#endif
        }
    }

    switch (m_int18C)
    {
    case 19:
        m_int18C = 0;
        // fall through
    case 0:
        m_int18C = 1;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 1:
        updateStrengths();
        m_int18C = 2;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 2:
    {
        const bool next = !m_int1F4;
        clearStates(next ? m_stateB : m_stateA, next ? m_stateD : m_stateC);
        m_int18C = 3;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 3:
    {
        const bool next = !m_int1F4;
        updateStates(next ? m_stateB : m_stateA, next ? m_stateD : m_stateC);
        m_int18C = 4;
        m_int1F4 = next;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 4:
    {
#if defined(_MSC_VER)
        // [PLATFORM] PE retains settings across getGameTime; ELF reloads it afterward.
        const IAISettings* const settings =
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
        const double gameTime = timer->getGameTime();
        const double elapsed = gameTime - m_float190;
#else
        const double gameTime = timer->getGameTime();
        const double elapsed = gameTime - m_float190;
        const IAISettings* const settings =
            static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
#endif
        if (!(elapsed > settings->getSAIUpdateFrequency()))
            return;
        prepareUpdate();
        m_int18C = 6;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    }
    case 6:
        if (!m_flag1D1)
        {
            if (!updateStrategicObjects())
                return;
            checkCommanderUseArtillery();
        }
        m_int18C = 7;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 7:
        if (!m_flag1D1)
            chooseStrategy();
        m_int18C = 8;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 8:
        modifyStrategicObjectsTemperatures();
        m_int18C = 9;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 9:
        updateBotPositions();
        m_int18C = 10;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 10:
        if (!m_flag1D1)
            updateDistribution();
        m_int18C = 11;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 11:
        if (!m_flag1D1)
            distributeResources();
        if (!haveAnyOwnedCps(m_side))
        {
            m_int18C = 15;
            return;
        }
        m_int18C = 12;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 12:
        if (!m_flag1D1)
            distributeDefence();
        m_int18C = 13;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 13:
        if (!m_flag1D1)
            distributeAttack();
        m_int18C = 14;
        if (timer->getUpdateTime() - start > budget * 0.95)
            return;
        // fall through
    case 14:
        // The ELF calls this empty direct helper; PE eliminates both the test and call.
        if (!m_flag1D1)
            distributeArtillery();
        m_int18C = 19;
        timer->getUpdateTime(); // Shipped final sample, discarded without another budget check.
        return;
    case 15:
        if (!m_flag1D1)
            distributeWithNoAreas();
        m_int18C = 19;
        timer->getUpdateTime();
        return;
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIStrategic/SAI.cpp", 1044, "")
            << "Unhadled case in SAI::Update()."; // Preserve the shipped spelling and source line.
        return;
    }
}

// Null bots do nothing; dead bots first receive (-1,-1,13).
// Squad registration is now a typed call; its narrower relocation/formation frontiers remain
// visible in the owning bodies. Only a completed registration reaches the publication tail.
void SAI::addBot(Bot* bot)
{
    if (!bot)
        return;

    if (bot->isDead())
        bot->setSpawnTarget(-1, -1, 13);

    SquadManager::m_instance->addSquadMember(bot->getPlayerId(), m_side);

    // Recovered tail, reached only after squad registration returns.
    // The vector is ELF +0x94 / PE +0xAC; port-specific STL layouts must not be copied as offsets.
    m_vector94.push_back(bot);
    bot->setDeathCallBack(checkPointer(new SAIBotDCB(bot, this)));
    bot->setSpawnCallBack(checkPointer(new SAIBotSCB(bot, this)));
}

void SAI::setBotIsDead(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::setBotIsDead");
}

void SAI::setBotIsAlive(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::setBotIsAlive");
}

void SAI::removeBot(Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::removeBot");
}

// Both test the requested bit; x86 masks shift count.
bool SAI::isEnemy(int side) const
{
    return (static_cast<unsigned int>(m_enemyMask) & (1u << (side & 31))) != 0;
}

// the same bit test on the friendly mask.
bool SAI::isFriend(int side) const
{
    return (static_cast<unsigned int>(m_sideMask) & (1u << (side & 31))) != 0;
}

float* SAI::getEnemyStrengths(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getEnemyStrengths");
}

int* SAI::getEnemyTypes(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getEnemyTypes");
}

float* SAI::getFriendlyStrengths(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getFriendlyStrengths");
}

int* SAI::getFriendlyTypes(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getFriendlyTypes");
}

void SAI::setBotHasChangedEquipment(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::setBotHasChangedEquipment");
}

// find, update the second member of the existing pair, or do nothing.
// Missing bot IDs are not inserted. The first member of the pair is left untouched.
void SAI::setNewBotKit(int botId, int kit)
{
    std::map<int, std::pair<int, int> >::iterator it = m_map5C.find(botId);
    if (it != m_map5C.end())
        it->second.second = kit;
}

// return the member map's address
// at +0x5C / +0x68. The caller copies the map; this getter neither copies nor mutates it.
// [INFERRED] const-reference spelling, as documented on the shared declaration.
const std::map<int, std::pair<int, int> >& SAI::getDeadBots() const
{
    return m_map5C;
}

void SAI::setPlayerIsCommander(bool)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::setPlayerIsCommander");
}

// byte getter at +0x1D1 / +0x221.
bool SAI::isPlayerCommander()
{
    return m_flag1D1;
}

bool SAI::playerRequestsArtillery(int, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::playerRequestsArtillery");
}

bool SAI::playerRequestsUav(int, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::playerRequestsUav");
}

bool SAI::playerRequestsSupply(int, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::playerRequestsSupply");
}

bool SAI::playerRequestsVehicleDrop(int, Vec3)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::playerRequestsVehicleDrop");
}

void SAI::updateRequestDenyTimers()
{
    // Cancellation deliberately retains the shipped
    // invalid-iterator defect: erases, then reads the erased
    // node; frees it, then reads [edi]. A nonempty queue
    // therefore makes the following ++it unsafe. Do not turn this into erase(it++).
    if (m_flag1D1)
    {
        for (auto it = m_denyRequests.begin(); it != m_denyRequests.end(); ++it)
        {
            ::operator delete(*it);
            m_denyRequests.erase(it);
        }
        return;
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (auto it = m_denyRequests.begin(); it != m_denyRequests.end(); )
    {
        // The comparison is ordered and strict: equality and NaN retain the request.
        if (now > (*it)->m_deadline)
        {
            switch ((*it)->m_type)
            {
            case 0:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denySupplyRequest(m_side, (*it)->m_playerId);
                break;
            case 1:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyArtilleryRequest(m_side, (*it)->m_playerId);
                break;
            case 2:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    ArtilleryUnavailable(m_side, (*it)->m_playerId);
                break;
            case 3:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyUavRequest(m_side, (*it)->m_playerId);
                break;
            case 4:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)->
                    denyVehicleDropRequest(m_side, (*it)->m_playerId);
                break;
            }
            // Both ports reload the payload after the callback, then advance before erase.
            ::operator delete(*it);
            m_denyRequests.erase(it++);
        }
        else
            ++it;
    }
}

// The AI commander's artillery decision, at most every ten seconds and sixty after the last
// strike. A hostile takeover in progress is shelled with probability 0.9 unless friends are within
// the artillery radius of the point; otherwise the first enemy long out of a valid position is.
// Skipped while a human commands or when the bot limit is one or less. PE 1013B8D0.
void SAI::checkCommanderUseArtillery()
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const bool tooFewBots = settings && settings->getMaxNBots() - 1 <= 0;
    if (m_flag1D1 || tooFewBots)
        return;

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    IBFCommander* const commander = static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander);
    if (!commander->gameHasArtillery(m_side) || !commander->anyArtilleryPiecesLeft(m_side))
        return;
    if (!(now > m_time68) || !(now > m_time88))
        return;

    SquadManager* const squads = SquadManager::m_instance;
    m_time88 = now + 10.0;
    AIStrategicObject* object;
    Vec2 target;
    target.x = 0.0f;
    target.y = 0.0f;
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    if (hostileTakeoveInProgress(object, target) && AIRandom::m_instance->nextFloat() > 0.1f)
    {
        const float height = terrain->getHeight(target.x, target.y);
        Vec3 point;
        point.x = target.x;
        point.y = height;
        point.z = target.y;
        if (!squads->isFriendlyNearPosition(point, squads->getArtilleryRadius(), m_side))
        {
            m_time68 = now + 60.0;
            commander->fireArtillery(m_side, -1, target);
            squads->incomingArtillery(point, m_side);
            return;
        }
    }

    if (squads->getArtilleryTarget(m_side, target))
    {
        const float height = terrain->getHeight(target.x, target.y);
        m_time68 = now + 60.0;
        commander->fireArtillery(m_side, -1, target);
        Vec3 point;
        point.x = target.x;
        point.y = height;
        point.z = target.y;
        squads->incomingArtillery(point, m_side);
    }
}

// The first area being taken over by the enemy with less than 0.75 of its counter left; the
// position is a random point between the area centre and twice its offset to this side's position.
// PE 1013C8E0.
bool SAI::hostileTakeoveInProgress(AIStrategicObject*& object, Vec2& position)
{
    const std::vector<AIStrategicObject*>& objects = AIStrategicObjectManager::getInstance()->getStrategicObjects();
    AIStrategicObject* candidate = 0;
    bool found = false;
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        candidate = *it;
        if (candidate && candidate->hasObjectTypeFlag(0x10) && candidate->isEnemyTakingOverBase(m_side) &&
            candidate->getControlPointTakeOverCounter() < 0.75f)
        {
            object = candidate;
            found = true;
            break;
        }
    }
    if (candidate && found)
    {
        const Vec2 centre = static_cast<AIStrategicArea*>(candidate)->getCentre();
        const Vec2 own = candidate->getPosition(m_side);
        const float rx = static_cast<float>(AIRandom::m_instance->next());
        position.x = static_cast<float>(rx * ((own.x - centre.x) * 2.0f) * 2.3283064e-10f + centre.x);
        const float rz = static_cast<float>(AIRandom::m_instance->next());
        position.y = static_cast<float>(rz * ((own.y - centre.y) * 2.0f) * 2.3283064e-10f + centre.y);
    }
    return found;
}

bool SAI::isPosInsideOwnedControlPoint(AIStrategicObject*&, Vec3, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::isPosInsideOwnedControlPoint");
}

// PE 1013CA20. Null entries are skipped.
bool SAI::haveAnyOwnedCps(int side)
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end();
         ++it)
    {
        if (*it && (*it)->getStatus(side) == StatusMap::Owned)
            return true;
    }
    return false;
}

bool SAI::haveAnyOwnedAndTakeableCps(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::haveAnyOwnedAndTakeableCps");
}

bool SAI::ownAllTakeableCps(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::ownAllTakeableCps");
}

// preserve unAssignSquads' full integer return.
// The caller's full-EAX additions establish the count contract; the body remains stopped.
// Unassigns the side's squads from the object at the given index, sparing squads taking its flag;
// answers how many were released. PE 1013A0F0.
int SAI::releaseSquadsFromObject(int objectIndex, int side)
{
    AIStrategicObject* const object = AIStrategicObjectManager::getInstance()->getStrategicObject(objectIndex);
    return SquadManager::m_instance->releaseSquadsFromArea(object->getIndex(), side);
}

void SAI::releaseUnitFromObject(int, std::map<int, int>::iterator&, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::releaseUnitFromObject");
}

void SAI::foundDeadBot(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::foundDeadBot");
}

bool SAI::hasChangedEquipment(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::hasChangedEquipment");
}

void SAI::equipmentChangedFinished(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::equipmentChangedFinished");
}

void SAI::assignBot(std::map<int, int>::iterator&, int, int, bool)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::assignBot");
}

bool SAI::strategicObjectsHasChanged()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::strategicObjectsHasChanged");
}

void SAI::registerTeleporter(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::registerTeleporter");
}

float SAI::getStrategyHeat(unsigned int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getStrategyHeat");
}

void SAI::listStrategies()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::listStrategies");
}

int SAI::getActiveStrategy()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getActiveStrategy");
}

// insert (point,1), increment only on duplicate.
// The native map lives at retail ELF +0x1D8 / PE +0x228; no key validation.
void SAI::addSpawnToPoint(int point)
{
    const std::pair<std::map<int, int>::iterator, bool> inserted =
        m_map1D8.insert(std::make_pair(point, 1));
    if (!inserted.second)
        ++inserted.first->second;
}

int SAI::getSpawnsToPoint(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getSpawnsToPoint");
}

Vec3 SAI::getBotCurrentPos(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getBotCurrentPos");
}

Vec3 SAI::getBotOrderedPos(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getBotOrderedPos");
}

Vec3 SAI::getBotLastOrderedPos(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getBotLastOrderedPos");
}

// Accumulate into the previous totals, then decay; no initial clear.
// Nonempty information can still reach the named Information/IPIUnit forwarding frontiers.
void SAI::updateStrengths()
{
    std::vector<unsigned int> handles;
    UnitPredicate* predicate = checkPointer(new UnitPredicate);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)->
        getAllInformation(handles, m_side, true, predicate, nullptr);

#if defined(_MSC_VER)
    // [PLATFORM] PE retains the last secondary lookup's manager for the next root lookup.
    // ELF reloads the global at the start of each root, and before each secondary callback.
    InformationManager* manager = handles.empty() ? nullptr : informationManager;
#endif
    for (auto it = handles.begin(); it != handles.end(); ++it)
    {
#if !defined(_MSC_VER)
        InformationManager* manager = informationManager;
#endif
        Information* root = manager->getInformation(*it);
        int secondary = root->getFirstSecondaryObjectIterator();
        const int rootSide = root->getSide();
        const bool friendly = rootSide == m_side;
        float* strengths = friendly ? m_friendlyStrengths : m_enemyStrengths;
        float* counts = friendly ? m_friendlyCounts : m_enemyCounts;
        Information* current = root;
        do
        {
            // Keep the selected unit across isOccupied/getSecurity, even if a callback replaces it.
            IPIUnit* unit = current->getUnit();
            if (unit->isOccupied())
            {
                const float security = current->getSecurity();
                for (int category = 0; category != 6; ++category)
                {
                    // Both shipped x87 paths retain the product through the add before storing
                    // float. Widen explicitly so modern SSE does not round the product early.
                    strengths[category] = static_cast<float>(
                        static_cast<double>(security) * unit->getBattleStrengths()[category]
                        + strengths[category]);
                }
                const int category = current->getPhysical()->getStrengthType();
                counts[category] += security;
            }

#if !defined(_MSC_VER)
            manager = informationManager;
#endif
            // Iteration always belongs to the original record, with the current SAI side.
            const unsigned int handle = root->getSecondaryObject(secondary, m_side);
#if defined(_MSC_VER)
            manager = informationManager;
#endif
            current = manager->getInformation(handle);
        } while (current);
    }

    for (int category = 0; category != 6; ++category)
    {
        if (m_friendlyStrengths[category] > 0.001f)
            m_friendlyStrengths[category] *= 0.5f;
        if (m_friendlyCounts[category] > 0.001f)
            m_friendlyCounts[category] *= 0.5f;
        if (m_enemyStrengths[category] > 0.001f)
            m_enemyStrengths[category] *= 0.5f;
        if (m_enemyCounts[category] > 0.001f)
            m_enemyCounts[category] *= 0.5f;
    }
    delete predicate;
}

// Updates strategic state in callback order with live vector iteration and repeated
// Route/Bridge tests. Non-area objects reuse the preceding area's output and border flag.
void SAI::updateStates(float* a, float* b)
{
    float* selected = 0;
    bool neighbour = false;
    const int enemySide = m_side == 1 ? 2 : 1;
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    a[13] = static_cast<float>(game->getTickets(m_side));
    b[13] = static_cast<float>(game->getTickets(enemySide));
    a[14] = static_cast<float>(game->getNumberOfControlPoints(m_side));
    b[14] = static_cast<float>(game->getNumberOfControlPoints(enemySide));

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    a[15] = static_cast<float>(now);
    b[15] = static_cast<float>(now);
    StrategyContainer* const end = m_strategies.empty()
        ? m_strategies.data() : m_strategies.data() + m_strategies.size();
    a[16] = m_activeStrategy == end ? 0.0f : static_cast<float>(now - m_activeStrategy->m_startTime);
    b[16] = 0.0f;
    a[17] = static_cast<float>(m_list1BC.size());
    a[18] = static_cast<float>(m_list1B8.size());
    a[19] = 0.0f; b[19] = 0.0f;
    a[20] = 0.0f; b[20] = 0.0f;
    a[21] = 0.0f; b[21] = 0.0f;
    a[39] = 0.0f; b[39] = 0.0f;
    a[38] = 0.0f; b[38] = 0.0f;
    b[41] = 0.0f; a[41] = 0.0f;
    a[40] = 0.0f; b[40] = 0.0f;
    a[42] = 0.0f; b[42] = 0.0f;

    std::set<int> attack;
    std::set<int> defence;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if ((*it)->getType() == 0)
        {
            switch ((*it)->getStatus(m_side))
            {
            case StatusMap::Owned:
                a[19] += 1.0f;
                b[21] += 1.0f;
                selected = a;
                neighbour = (*it)->hasNeighbourWithStatus(StatusMap::Hostile, m_side);
                if (neighbour) { a[39] += 1.0f; b[41] += 1.0f; }
                else           { a[40] += 1.0f; b[42] += 1.0f; }
                break;
            case StatusMap::Hostile:
                a[21] += 1.0f;
                b[19] += 1.0f;
                selected = b;
                neighbour = (*it)->hasNeighbourWithStatus(StatusMap::Owned, m_side);
                if (neighbour) { b[39] += 1.0f; a[41] += 1.0f; }
                else           { b[40] += 1.0f; a[42] += 1.0f; }
                break;
            case StatusMap::Neutral:
                a[20] += 1.0f;
                b[20] += 1.0f;
                if ((*it)->hasNeighbouringObjectWithStatus(StatusMap::Owned, m_side))
                    a[38] += 1.0f;
                if ((*it)->hasNeighbouringObjectWithStatus(StatusMap::Hostile, m_side))
                    b[38] += 1.0f;
                selected = 0;
                break;
            default:
                selected = 0;
                neighbour = false;
                break;
            }
        }

        if ((*it)->getNumberOfAssignedUnits(m_side) > 0)
        {
            if ((*it)->isAttack(m_side))
                attack.insert((*it)->getIndex());
            else
                defence.insert((*it)->getIndex());
        }

        if (selected)
        {
            // Each test reloads *it, including after a preceding virtual callback replaced it.
            static const AIStrategicObject::StrategicObjectType areaFlags[] = {
                AIStrategicObject::Flank, AIStrategicObject::Base, AIStrategicObject::Close,
                AIStrategicObject::Centre, AIStrategicObject::Remote
            };
            for (int i = 0; i != 5; ++i)
            {
                if ((*it)->hasObjectTypeFlag(areaFlags[i]))
                {
                    selected[22 + i] += 1.0f;
                    if (neighbour)
                        selected[33 + i] += 1.0f;
                }
            }
            static const AIStrategicObject::StrategicObjectType otherFlags[] = {
                AIStrategicObject::Route, AIStrategicObject::Bridge,
                AIStrategicObject::Route, AIStrategicObject::Bridge,
                AIStrategicObject::North, AIStrategicObject::West,
                AIStrategicObject::South, AIStrategicObject::East
            };
            static const int indices[] = { 27, 28, 27, 28, 29, 30, 31, 32 };
            for (int i = 0; i != 8; ++i)
                if ((*it)->hasObjectTypeFlag(otherFlags[i]))
                    selected[indices[i]] += 1.0f;
        }
    }
    b[17] = static_cast<float>(attack.size());
    b[18] = static_cast<float>(defence.size());
}

// Prepares admitted bots and resource strengths. Admission retains its Bot pointer
// across callbacks; strength/removal traversals reload the element. Preserve all four
// id queries during removal.
void SAI::prepareUpdate()
{
    for (std::vector<Bot*>::iterator it = m_vector94.begin(); it != m_vector94.end(); ++it)
    {
        Bot* const bot = *it;
        if (bot)
        {
            BotInfo* const info = checkPointer(new BotInfo(bot));
            m_botInfo[bot->getBotId()] = info;
            if (bot->isDead())
            {
                m_map5C.insert(std::make_pair(static_cast<int>(bot->getBotId()), std::make_pair(-1, 13)));
                bot->getPlayer()->setSpawnGroup(0);
                bot->setIsInTransitionPhase(true);
            }
            else
                setBotIsAlive(bot->getBotId());
            (void)m_setFC.find(bot->getBotId()); // shipped discarded lookup
        }
    }

    // [PLATFORM] PE frees/nulls all three vector pointers; ELF erase retains capacity.
#if defined(_MSC_VER)
    std::vector<Bot*>().swap(m_vector94);
#else
    m_vector94.erase(m_vector94.begin(), m_vector94.end());
#endif
    clearSpawnsToPoints();
    for (std::list<int>::iterator it = m_list110.begin(); it != m_list110.end(); ++it)
        handleTeleportingBot(*it);
    m_list110.clear();

    for (int i = 0; i != 6; ++i)
    {
        m_friendlyStrengths[i] = 0.0f;
        m_friendlyCounts[i] = 0.0f;
    }
    for (std::vector<BotInfo*>::iterator it = m_botInfo.begin(); it != m_botInfo.end(); ++it)
    {
        if (*it && !(*it)->m_bot->isDead())
        {
            // [PLATFORM] ELF captures the manager before the equipment callback. PE does so
            // only afterward, and does not read it at all when the handle's low half is zero.
#if !defined(_MSC_VER)
            InformationManager* const manager = informationManager;
#endif
            const unsigned int handle = (*it)->m_bot->getCurrentEquipment();
#if defined(_MSC_VER)
            if ((handle & 0xffffu) == 0)
                continue;
            InformationManager* const manager = informationManager;
#endif
            Information* const information = manager->getInformation(handle);
            if (information && information->getUnit())
            {
                IPIUnit* const unit = information->getUnit();
                // Assignment, not summation: the last live unit supplies all six strengths.
                for (int i = 0; i != 6; ++i)
                    m_friendlyStrengths[i] = unit->getBattleStrengths()[i];
                const int category = information->getPhysical()->getStrengthType();
                m_friendlyCounts[category] += 1.0f;
            }
        }
    }

    for (std::map<int, std::pair<int, int> >::iterator it = m_map5C.begin(); it != m_map5C.end();)
    {
        IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
        BotInfo* const info = m_botInfo[it->first];
        if (!info)
        {
            std::map<int, std::pair<int, int> >::iterator removed = it++;
            m_map5C.erase(removed);
        }
        else
        {
            Bot* const bot = info->m_bot;
            if (settings->isRespawnAllowed())
                BotSpawner::findSuitableSpawnGroup(m_enemyStrengths, m_enemyCounts,
                    m_friendlyStrengths, m_friendlyCounts, it->second.first, bot);
            ++it;
        }
    }

    for (std::vector<Bot*>::iterator it = m_vectorA0.begin(); it != m_vectorA0.end(); ++it)
    {
        if (m_botInfo[(*it)->getBotId()])
        {
            m_botInfo[(*it)->getBotId()]->remove(this);
            delete m_botInfo[(*it)->getBotId()];
            m_botInfo[(*it)->getBotId()] = 0;
        }
    }
#if defined(_MSC_VER)
    std::vector<Bot*>().swap(m_vectorA0);
#else
    m_vectorA0.erase(m_vectorA0.begin(), m_vectorA0.end());
#endif
}

// Reset mapped values
// retaining keys and nodes. The live native end is checked after each iterator advance.
void SAI::clearSpawnsToPoints()
{
    for (std::map<int, int>::iterator it = m_map1D8.begin(); it != m_map1D8.end(); ++it)
        it->second = 0;
}

// Remaining direct SAI::update dependencies. Paired callers and helper return paths establish
// these signatures; the strategic work behind them remains unrecovered.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// BotInfo::remove: / , paired callers discard
// the result and neither body materializes a return. Assigned/free/present-unit removal is absent.
void SAI::BotInfo::remove(SAI*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::BotInfo::remove");
}

// PE 10140F60. The busy-set entry is erased without an end check, as shipped.
void SAI::BotInfo::release(bool removeFromObject, SAI* sai)
{
    const int botId = static_cast<int>(m_bot->getBotId());
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getStrategicObject(m_int0C);
    if (removeFromObject)
    {
        if (!object->eraseIdFromBotSet20(botId, sai->m_side))
            object->eraseIdFromBotSet44(botId, sai->m_side);
        sai->m_setFC.erase(sai->m_setFC.find(botId));
    }
    sai->m_set44.insert(static_cast<int>(m_bot->getBotId()));
}

// PE 10140CF0. The bot id is read three times; the second read is discarded.
void SAI::BotInfo::assignTo(AIStrategicObject* object, SAI* sai)
{
    m_int0C = object->m_index;
    object->insertBotSet92(static_cast<int>(m_bot->getBotId()), sai->m_side);
    m_bot->getBotId();
    sai->m_set44.insert(static_cast<int>(m_bot->getBotId()));
}

// PE 10140ED0.
void SAI::BotInfo::commit(std::set<int>::iterator& it, bool attack, SAI* sai)
{
    AIStrategicObject* const object =
        AIStrategicObjectManager::getInstance()->getStrategicObject(m_int0C);
    if (attack)
        object->insertBotSet20(static_cast<int>(m_bot->getBotId()), sai->m_side);
    else
        object->insertBotSet44(static_cast<int>(m_bot->getBotId()), sai->m_side);
    const std::set<int>::iterator erased = it;
    ++it;
    sai->m_set44.erase(erased);
    sai->m_setFC.insert(static_cast<int>(m_bot->getBotId()));
}

// PE 1013F160.
void SAI::BotInfo::reassign(std::set<int>::iterator& it, AIStrategicObject* from,
                            AIStrategicObject* to, SAI* sai)
{
    const int botId = static_cast<int>(m_bot->getBotId());
    to->insertBotSet92(botId, sai->m_side);
    if (from->eraseIdFromBotSet20(botId, sai->m_side))
        to->insertBotSet20(botId, sai->m_side);
    else if (from->eraseIdFromBotSet44(botId, sai->m_side))
        to->insertBotSet44(botId, sai->m_side);
    from->eraseFromBotSet92(it, sai->m_side);
    m_int0C = to->m_index;
}

// Void is inferred from complete
// bodies and their callers. The spawn-group selector and dead-bot/respawn chain remain absent.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
void SAI::handleTeleportingBot(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::handleTeleportingBot");
}

bool SAI::updateStrategicObjects()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    bool changed = false;
    if (m_flag1D0)
    {
        m_flag1CF = false;
        m_flag1D0 = false;
        m_strategicObjectIterator = objects.begin();
        m_enemyStats->clear();
        m_friendlyStats->clear();
    }

    // Both ports dereference unconditionally and process exactly one object. The concrete
    // update returns void. There is no empty-vector guard or inner budget check.
    (*m_strategicObjectIterator)->update(m_side, this, changed);
    m_flag1CF = m_flag1CF || changed;

    // The callback may change the cursor/entry/side. Capture status before the getter
    // then load the selected destination; the shipped numeric split is zero/nonzero.
    AIStrategicObject* const object = *m_strategicObjectIterator;
    const int side = m_side;
    const StatusMap::Status status = object->getStatus(side);
    EnemyStatistics& statistics = object->getBattleStatistics(side);
    if (status == 0)
        m_enemyStats->add(statistics);
    else
        m_friendlyStats->add(statistics);

    ++m_strategicObjectIterator;
    if (m_strategicObjectIterator != objects.end())
        return false;

    if (m_flag1CF)
    {
        // Retain this invocation's vector, but reload its live end, entry and side.
        for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin();
             it != objects.end(); ++it)
        {
            (*it)->setAsDirty(m_side);
            (*it)->releaseAssignedResources(m_side);
        }
    }
    m_flag1D0 = true;
    return true;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// PE 10141C00. A forced strategy is activated outright. Otherwise every heat is re-evaluated;
// if no heat moved outside [0.83, 1.2) of its previous value the current choice is kept until its
// time limit (scaled up with each renewal) expires. Then a strategy is drawn with probability
// proportional to its positive heat. An empty strategy list dereferences the active pointer, as
// shipped. The pick is evaluated in double where the PE keeps it on the x87 stack.
void SAI::chooseStrategy()
{
    StrategyManager::getInstance()->isDebugEnabled();
    float* const friendlyState = m_int1F4 ? m_stateB : m_stateA;
    float* const enemyState    = m_int1F4 ? m_stateD : m_stateC;

    Strategy* const forced = StrategyManager::getInstance()->getForcedStrategy(m_side);
    if (forced)
    {
        forced->getHeat(friendlyState, enemyState, m_activeStrategy->m_strategy, m_side);
        for (StrategyContainer* it = m_strategies.empty() ? 0 : &m_strategies.front();
             it && it != &m_strategies.front() + m_strategies.size(); ++it)
        {
            if (it->m_strategy == forced)
                setActiveStrategy(it);
        }
        return;
    }

    bool changed = false;
    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin();
         it != m_strategies.end(); ++it)
    {
        it->m_newHeat = it->m_strategy->getHeat(friendlyState, enemyState,
                                                m_activeStrategy->m_strategy, m_side);
        if (it->m_newHeat < 0.0f)
            it->m_newHeat = 0.0f;

        double ratio;
        if (it->m_newHeat == 0.0f)
            ratio = (it->m_heat == 0.0f) ? 1.0 : 0.0;
        else
            ratio = it->m_heat / it->m_newHeat;

        if (ratio <= 0.83 || ratio >= 1.2)
            changed = true;
    }

    if (!changed)
    {
        StrategyContainer* const active = m_activeStrategy;
        const float count = static_cast<float>(active->m_count);
        const double duration =
            ((0.5 - std::pow(0.5, static_cast<double>(count))) * 2.0 + 1.0)
            * active->m_strategy->m_timeLimit;
        const double now =
            static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        if (!(now - active->m_startTime >= duration))
            return;
        ++m_activeStrategy->m_count;
    }

    float sum = 0.0f;
    for (std::vector<StrategyContainer>::iterator it = m_strategies.begin();
         it != m_strategies.end(); ++it)
    {
        it->m_heat = it->m_newHeat;
        sum = (0.0f < it->m_newHeat ? it->m_newHeat : 0.0f) + sum;
    }

    double pick = static_cast<double>(AIRandom::m_instance->next()) * sum
                * 2.3283064365386963e-10;

    StrategyContainer* chosen = m_strategies.empty() ? 0 : &m_strategies.front();
    StrategyContainer* const end = chosen ? chosen + m_strategies.size() : 0;
    for (; chosen != end; ++chosen)
    {
        pick -= (0.0f < chosen->m_heat ? chosen->m_heat : 0.0f);
        if (pick <= 0.0)
            break;
    }
    if (chosen == end)
        --chosen;

    setActiveStrategy(chosen);
}

// PE 10141A30. On a change, every strategic object first releases its squads for this side, then
// each bot in its two per-side sets is removed from the object and from m_setFC and recorded in
// m_set44. The bot is fetched before the object erases its id. The m_setFC lookup is erased
// without an end check, as shipped.
void SAI::setActiveStrategy(StrategyContainer* const& strategy)
{
    if (m_activeStrategy == strategy)
        return;

    m_activeStrategy = strategy;
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator obj = objects.begin();
         obj != objects.end(); ++obj)
    {
        (*obj)->releaseAssignedResources(m_side);

        std::set<int>& first = (*obj)->getBotSet20(m_side);
        std::set<int>::iterator it = first.begin();
        while (it != first.end())
        {
            BotInfo* const info = m_botInfo[*it];
            unsigned int botId = info->m_bot->getBotId();
            (*obj)->eraseFromBotSet20(it, m_side);
            m_setFC.erase(m_setFC.find(static_cast<int>(botId)));
            botId = info->m_bot->getBotId();
            m_set44.insert(static_cast<int>(botId));
        }

        std::set<int>& second = (*obj)->getBotSet44(m_side);
        it = second.begin();
        while (it != second.end())
        {
            BotInfo* const info = m_botInfo[*it];
            unsigned int botId = info->m_bot->getBotId();
            (*obj)->eraseFromBotSet44(it, m_side);
            m_setFC.erase(m_setFC.find(static_cast<int>(botId)));
            botId = info->m_bot->getBotId();
            m_set44.insert(static_cast<int>(botId));
        }
    }

    m_activeStrategy->m_startTime = static_cast<float>(
        static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
    m_activeStrategy->m_count = 1;
}

// PE 1013DDC0. Applies the active strategy's modifiers to every strategic object; the returned
// temperatures are discarded.
void SAI::modifyStrategicObjectsTemperatures()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end();
         ++it)
        (*it)->updateTemperature(m_activeStrategy->m_strategy, m_side);
}

namespace {

// One pass of the second half of updateBotPositions over a per-side bot set. A bot whose recheck
// interval (20 s on foot, 35 s otherwise) has elapsed and that senses at most one object is
// offered to the object again through retainBot. The waypoint-list gate in front of that test
// has no recovered interface yet, so reaching a bot stops at a named frontier.
void retainBotsInSet(int side, AIStrategicObject* object, std::set<int>& bots, double now)
{
    for (std::set<int>::iterator it = bots.begin(); it != bots.end(); ++it)
    {
        Bot* const bot = IBotManager::instance->getBotFromId(static_cast<unsigned int>(*it));
        const unsigned int soldier =
            bot->getPlayer()->getSoldier()->getInformation(side);
        const float interval = (bot->getCurrentEquipment() == soldier) ? 20.0f : 35.0f;
        (void)interval;
        (void)now;
        (void)object;
        // PE: getWaypoints()->byte +5 and its slot +0x0C == 1 gate the timer test, then
        // getSensedObjects().size() <= 1, isInBotSet92 and retainBot(side, id, sai), and the
        // BotInfo +0x18 time is restamped. [FRONTIER] [TEMP] [CHOSEN, not recovered]
        // [RETIRE-WHEN: waypointlist-interface] Recover WaypointList's flag and slot 3.
        BF2_AI_FRONTIER("dice::hfe::ai::SAI::updateBotPositions (WaypointList gate)");
    }
}

} // anonymous namespace

// PE 101400E0. For every strategic area, each bot in its fourth per-side set whose current
// equipment carries a unit is moved to the neighbouring area that attracts its position most,
// among neighbours that allow its vehicle group, do not expel its equipment type, are areas and
// do not carry its type in m_i340. The three remaining bot sets are then offered for retention.
void SAI::updateBotPositions()
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    IBotManager* const bots = IBotManager::instance;

    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    for (std::vector<AIStrategicObject*>::const_iterator obj = objects.begin();
         obj != objects.end(); ++obj)
    {
        std::set<int>& positioned = (*obj)->getBotSet92(m_side);
        if ((*obj)->getType() != 0)
            continue;

        AIStrategicArea* const area = static_cast<AIStrategicArea*>(*obj);
        std::vector<AIStrategicObject*>& neighbours = area->m_neighbouringObjects;

        std::set<int>::iterator it = positioned.begin();
        while (it != positioned.end())
        {
            Bot* const bot = bots->getBotFromId(static_cast<unsigned int>(*it));
            Information* const information =
                informationManager->getInformation(bot->getCurrentEquipment());
            if (!information || !information->getUnit())
            {
                ++it;
                continue;
            }

            const int equipmentType = information->getUnit()->getEquipmentType();
            const int vehicleGroup = settings->getVehicleGroup(equipmentType);

            AIStrategicObject* best = *obj;
            float bestGravity = best->gravity(information->getPosition(), m_side);
            for (std::vector<AIStrategicObject*>::iterator n = neighbours.begin();
                 n != neighbours.end(); ++n)
            {
                if (!(*n)->allowsVehicleGroup(vehicleGroup))
                    continue;
                if ((*n)->expelsUnitType(equipmentType))
                    continue;
                if ((*n)->getType() != 0)
                    continue;
                if (static_cast<AIStrategicArea*>(*n)->hasFlag340(equipmentType))
                    continue;

                const float gravity = (*n)->gravity(information->getPosition(), m_side);
                if (bestGravity < gravity)
                {
                    best = *n;
                    bestGravity = gravity;
                }
            }

            if (best != *obj)
                m_botInfo[*it]->reassign(it, *obj, best, this);
            else
                ++it;
        }

        const double now =
            static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        retainBotsInSet(m_side, *obj, (*obj)->getBotSet68(m_side), now);
        retainBotsInSet(m_side, *obj, (*obj)->getBotSet20(m_side), now);
        retainBotsInSet(m_side, *obj, (*obj)->getBotSet44(m_side), now);
    }
}

namespace {

// The unit plug-in of a bot's current equipment, or null for a stale handle or a unit-less object.
IPIUnit* equipmentUnit(Bot* bot)
{
    Information* const information = informationManager->getInformation(bot->getCurrentEquipment());
    return information ? information->getUnit() : 0;
}

} // anonymous namespace

// PE 10141EF0. Totals the strategic strength held in each object's role sets: the first role set
// counts toward m_intD8, the second toward m_intDC, the fourth set toward m_intE0, and the
// assigned set toward D8 or DC by the object's attack flag. A set whose release flag
// (m_flag1CD / m_flag1CC) is raised is emptied instead, raising m_flag1CE. Finally the
// truncated sum of both role-set averages is subtracted from m_intE0.
void SAI::updateDistribution()
{
    const std::vector<AIStrategicObject*>& objects =
        AIStrategicObjectManager::getInstance()->getStrategicObjects();
    IBotManager* const bots = IBotManager::instance;

    m_flag1CE = false;
    m_intD8 = 0;
    m_intDC = 0;
    m_intE0 = 0;
    float firstAverage = 0.0f;
    float secondAverage = 0.0f;

    for (std::vector<AIStrategicObject*>::const_iterator obj = objects.begin();
         obj != objects.end(); ++obj)
    {
        if (m_flag1CD)
        {
            while ((*obj)->getBotSet20(m_side).size() > 0)
            {
                m_botInfo[*(*obj)->getBotSet20(m_side).begin()]->release(true, this);
                m_flag1CE = true;
            }
        }
        else
        {
            for (std::set<int>::iterator it = (*obj)->getBotSet20(m_side).begin();
                 it != (*obj)->getBotSet20(m_side).end(); ++it)
            {
                IPIUnit* const unit = equipmentUnit(bots->getBotFromId(static_cast<unsigned int>(*it)));
                if (!unit)
                    continue;
                m_intD8 += static_cast<int>(unit->getStrategicStrengths()[0]);
                firstAverage = unit->getAverageStrategicStrength() + firstAverage;
            }
        }

        if (m_flag1CC)
        {
            while ((*obj)->getBotSet44(m_side).size() > 0)
            {
                m_botInfo[*(*obj)->getBotSet44(m_side).begin()]->release(true, this);
                m_flag1CE = true;
            }
        }
        else
        {
            for (std::set<int>::iterator it = (*obj)->getBotSet44(m_side).begin();
                 it != (*obj)->getBotSet44(m_side).end(); ++it)
            {
                IPIUnit* const unit = equipmentUnit(bots->getBotFromId(static_cast<unsigned int>(*it)));
                if (!unit)
                    continue;
                m_intDC += static_cast<int>(unit->getStrategicStrengths()[1]);
                secondAverage = unit->getAverageStrategicStrength() + secondAverage;
            }
        }

        for (std::set<int>::iterator it = (*obj)->getBotSet92(m_side).begin();
             it != (*obj)->getBotSet92(m_side).end(); ++it)
        {
            IPIUnit* const unit = equipmentUnit(bots->getBotFromId(static_cast<unsigned int>(*it)));
            if (unit)
                m_intE0 += static_cast<int>(unit->getAverageStrategicStrength());
        }

        for (std::set<int>::iterator it = (*obj)->getBotSet68(m_side).begin();
             it != (*obj)->getBotSet68(m_side).end(); ++it)
        {
            IPIUnit* const unit = equipmentUnit(bots->getBotFromId(static_cast<unsigned int>(*it)));
            if (!unit)
                continue;
            if ((*obj)->isAttack(m_side))
            {
                m_intD8 += static_cast<int>(unit->getStrategicStrengths()[0]);
                firstAverage = unit->getAverageStrategicStrength() + firstAverage;
            }
            else
            {
                m_intDC += static_cast<int>(unit->getStrategicStrengths()[1]);
                secondAverage = unit->getAverageStrategicStrength() + secondAverage;
            }
        }
    }

    m_intE0 -= static_cast<int>(secondAverage + firstAverage);
}

// PE 101423A0. Every free bot is placed on a strategic object (its recorded one, or the most
// attractive one for its equipment) and committed to attack or defence so that the attacking
// share of strategic strength tracks the strategy's aggression: all-attack with no owned areas,
// all-defence with nothing left to take. The last bot's role then decides whether an excess of
// more than 1.2 times the wanted share raises m_flag1CD (attackers) or m_flag1CC (defenders).
// A placed bot whose equipment has no unit is dereferenced as shipped.
void SAI::distributeResources()
{
    // Two local lists and a bot-count scratch array are created and released without use.
    std::list<int> unusedA;                                   // [UNRESOLVED] element type
    std::list<int> unusedB;                                   // [UNRESOLVED] element type
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    bool lastWasDefence = true;
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    const int maxBots = settings->getMaxNBots();
    bool* const scratch = checkPointer(new bool[maxBots]);
    if (maxBots > 0)
        std::memset(scratch, 0, static_cast<size_t>(maxBots));

    const float* const friendlyState = m_int1F4 ? m_stateB : m_stateA;
    float attackShare;
    float defenceShare;
    if (friendlyState[StrategyCondition::NumberOfFriendlyAreas] == 0.0f)
    {
        attackShare = 1.0f;
        defenceShare = 0.0f;
    }
    else if (friendlyState[StrategyCondition::NumberOfNeutralAreas] == 0.0f
             && friendlyState[StrategyCondition::NumberOfHostileAreas] == 0.0f)
    {
        attackShare = 0.0f;
        defenceShare = 1.0f;
    }
    else
    {
        attackShare = m_activeStrategy->m_strategy->getAggression();
        defenceShare = 1.0f - m_activeStrategy->m_strategy->getAggression();
    }

    std::set<int>::iterator it = m_set44.begin();
    while (it != m_set44.end())
    {
        if (!m_botInfo[*it])
        {
            ++it;
            continue;
        }

        Information* const information =
            informationManager->getInformation(m_botInfo[*it]->m_bot->getCurrentEquipment());
        if (!information)
        {
            ++it;
            continue;
        }

        AIStrategicObject* object = manager->getStrategicObject(m_botInfo[*it]->m_int0C);
        if (!object)
        {
            if (!information->getUnit())
            {
                ++it;
                continue;
            }
            const int vehicleNumber =
                information->getMobile() ? information->getMobile()->getVehicleNumber() : -1;
            object = manager->getClosestObject(information->getPosition(),
                                               information->getUnit()->getEquipmentType(),
                                               vehicleNumber, m_side);
            if (!object)
            {
                ++it;
                continue;
            }
            m_botInfo[*it]->assignTo(object, this);
        }
        else
        {
            object->insertBotSet92(*it, m_side);
        }

        if (object->m_index != m_botInfo[*it]->m_int08)
        {
            object->retainBot(m_side, *it, this);
            const int index = object->m_index;
            BotInfo* const info = m_botInfo[*it];
            info->m_double14 =
                static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            info->m_int08 = index;
        }

        IPIUnit* const unit = information->getUnit();
        const int attackStrength = m_intD8;
        const int committed = m_intD8 + m_intDC;
        bool attack;
        // The x87 tests send an unordered share down the ratio arm, as written here.
        if (committed == 0)
            attack = attackShare >= 0.5f;
        else if (!(attackShare > 0.0f) && attackShare == attackShare)
            attack = false;
        else if (defenceShare <= 0.0f)
            attack = true;
        else
            attack = static_cast<float>(attackStrength) / static_cast<float>(committed)
                     <= attackShare;

        if (attack)
            m_intD8 = static_cast<int>(unit->getStrategicStrengths()[0]) + attackStrength;
        else
            m_intDC = static_cast<int>(unit->getStrategicStrengths()[1]) + m_intDC;

        m_intE0 = m_intE0 - static_cast<int>(unit->getAverageStrategicStrength());
        lastWasDefence = !attack;
        m_botInfo[*it]->commit(it, attack, this);
    }

    const int attackTotal = m_intD8;
    const int defenceTotal = m_intDC;
    const float total = static_cast<float>(m_intD8 + m_intDC + m_intE0);
    if (total == 0.0f)
    {
        m_flag1CD = false;
        m_flag1CC = false;
    }
    else
    {
        if (attackShare * 1.2f < static_cast<float>(attackTotal) / total && !lastWasDefence)
        {
            m_flag1CD = true;
            m_flag1CE = true;
        }
        else
        {
            m_flag1CD = false;
        }

        if (defenceShare * 1.2f < static_cast<float>(defenceTotal) / total && lastWasDefence)
        {
            m_flag1CC = true;
            m_flag1CE = true;
        }
        else
        {
            m_flag1CC = false;
        }
    }

    delete[] scratch;
}

// PE 10144C30. Runs per category when categories exist, otherwise over all objects; nothing
// happens when the active strategy asks for no defences.
void SAI::distributeDefence()
{
    if (m_activeStrategy->m_strategy->getNumberOfDefences() == 0)
        return;

    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    if (manager->getNumberOfCategories() == 0)
    {
        distributeDefence(manager->getStrategicObjects());
        return;
    }
    for (int category = 0; category < manager->getNumberOfCategories(); ++category)
        distributeDefence(manager->getStrategicObjectsOfCategory(category));
}

namespace {

// Element of distributeDefence's need list; ordered by need alone. [INFERRED] name.
struct DefenceNeed
{
    int index;
    float need;
    bool operator<(const DefenceNeed& other) const { return need < other.need; }
};

// Heap order of distributeDefence: the object with the greater assigned ratio is "less", so the
// heap top is the object with the lowest ratio.
struct AssignedRatioLess
{
    int side;
    bool operator()(AIStrategicObject* a, AIStrategicObject* b) const
    {
        return a->getAssignedRatio(side) > b->getAssignedRatio(side);
    }
};

} // anonymous namespace

// PE 10143ED0. Objects needing defence above 0.1 are ranked by need; objects needing none release
// their squads unless attacking. The list is cut after twice the strategy's defence count once a
// need drops below 90 % of that entry, the cut objects releasing squads. If defenders are in
// excess and nothing was released, squads are released from the least needy until one goes.
// Otherwise up to the defence count of the neediest objects are marked defending and repeatedly
// offered one more squad, lowest assigned ratio first, until none accepts.
void SAI::distributeDefence(const std::vector<AIStrategicObject*>& objects)
{
    std::list<DefenceNeed> needs;
    int released = 0;
    DefenceNeed entry;
    entry.index = -1;
    entry.need = 0.0f;

    for (std::vector<AIStrategicObject*>::const_iterator it = objects.begin(); it != objects.end();
         ++it)
    {
        if (!*it)
            continue;
        entry.need = (*it)->calculateDefenseNeed(m_side);
        if (entry.need > 0.1f)
        {
            entry.index = (*it)->m_index;
            needs.push_back(entry);
        }
        else if (!(entry.need > 0.0f) && entry.need == entry.need)
        {
            if (!(*it)->isAttack(m_side))
                released += (*it)->releaseAssignedResources(m_side);
        }
    }

    needs.sort();
    needs.reverse();
    if (needs.empty())
        return;

    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    float pivot = 0.0f;
    unsigned int position = 0;
    std::list<DefenceNeed>::iterator node = needs.begin();
    for (; node != needs.end(); ++node)
    {
        ++position;
        const unsigned int limit =
            static_cast<unsigned int>(m_activeStrategy->m_strategy->getNumberOfDefences()) << 1;
        if (position == limit)
            pivot = node->need;
        else if (position > limit && pivot * 0.9f > node->need)
            break;
    }
    while (node != needs.end())
    {
        AIStrategicObject* const object = manager->getStrategicObject(node->index);
        if (object && !object->isAttack(m_side))
            released += object->releaseAssignedResources(m_side);
        node = needs.erase(node);
    }
    if (needs.empty())
        return;

    if (m_flag1CC && !m_flag1CE && released == 0)
    {
        needs.reverse();
        for (node = needs.begin(); node != needs.end(); ++node)
        {
            AIStrategicObject* const object = manager->getStrategicObject(node->index);
            if (!object || object->isAttack(m_side))
                continue;
            released += object->releaseAssignedResources(m_side);
            if (released > 0)
                return;
        }
        return;
    }

    AssignedRatioLess less;
    less.side = m_side;
    std::vector<AIStrategicObject*> heap;
    unsigned int marked = 0;
    for (node = needs.begin(); node != needs.end(); ++node)
    {
        AIStrategicObject* object = manager->getStrategicObject(node->index);
        if (!object)
            continue;
        object->setAttack(false, m_side);
        heap.push_back(object);
        std::push_heap(heap.begin(), heap.end(), less);
        ++marked;
        if (static_cast<unsigned int>(m_activeStrategy->m_strategy->getNumberOfDefences())
            <= marked)
            break;
    }

    while (!heap.empty())
    {
        AIStrategicObject* const top = heap.front();
        std::pop_heap(heap.begin(), heap.end(), less);
        heap.pop_back();
        if (top->collectAdditionalSquadResources(1, m_side, this) == 0)
            continue;
        heap.push_back(top);
        std::push_heap(heap.begin(), heap.end(), less);
    }
}

void SAI::distributeAttack()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::distributeAttack");
}

void SAI::distributeWithNoAreas()
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::distributeWithNoAreas");
}

// Recovered empty helper: , (frame prologue/epilogue and return only).
// PE SAI::update omits the empty call at state 14.
void SAI::distributeArtillery()
{
}

// SampleHelper: complete paired bodies, unsigned live bound and no index/size guards.
SampleHelper::SampleHelper(unsigned int size) : m_size(size)
{
}

void SampleHelper::addSample(float* input, int index, float* strength, int* count)
{
    for (unsigned int i = 0; i < m_size; ++i)
        strength[i] = input[i] + strength[i];
    ++count[index];
}

void SampleHelper::clear(float* strength, int* count)
{
    for (unsigned int i = 0; i < m_size; ++i)
    {
        strength[i] = 0.0f;
        count[i] = 0;
    }
}

CommanderInterface* SAI::getCommanderInterface() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::SAI::getCommanderInterface");
}

} // namespace ai
} // namespace hfe
} // namespace dice
