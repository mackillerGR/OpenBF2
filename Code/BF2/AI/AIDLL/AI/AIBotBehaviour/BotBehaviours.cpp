#include "BotBehaviours.h"

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIBotBehaviourPattern/PlanGenerators.h"
#include "../AIBotBehaviourPattern/PlanGeneratorSupport.h"
#include "../Main/AIDllFrontier.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-bot-behaviours] Every
// calculateUrgency body and the per-class extra virtuals below stop by name until recovered and
// exercised by a bot that runs the behaviour.

// ---- SampledFunction -----------------------------------------------------------------------

SampledFunction::SampledFunction(Function function, float low, float high, float step)
    : m_function(function)
    , m_table(0)
    , m_low(low)
    , m_high(high)
    , m_step(step)
{
    build();
}

SampledFunction::~SampledFunction()
{
    delete[] m_table;
}

// PE 101B1CB0. The count truncates; the end points are sampled exactly.
void SampledFunction::build()
{
    if (m_table)
        delete[] m_table;
    m_count = static_cast<int>((m_high - m_low) / m_step);
    m_table = new float[m_count + 1];
    const double interval = static_cast<double>(m_high - m_low) / m_count;
    for (int i = 0; i <= m_count; ++i)
        m_table[i] = m_function(static_cast<float>(i * interval + m_low));
    m_table[0] = m_function(m_low);
    m_table[m_count] = m_function(m_high);
}

namespace {

// PE 10100B70: the fire curve BBFire samples.
float fireCurve(float value)
{
    return static_cast<float>(std::pow(static_cast<double>(value + value - 0.95f), 21.0) * 1.012f
                              + 1.0f);
}

} // anonymous namespace

// ---- Behaviours ----------------------------------------------------------------------------

BBAvoid::BBAvoid(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBAvoid::~BBAvoid()
{
    delete[] m_targets;
}

float BBAvoid::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBAvoid::calculateUrgency");
}

BBMoveTo::BBMoveTo(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = 0;
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBMoveTo::~BBMoveTo()
{
}

float BBMoveTo::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMoveTo::calculateUrgency");
}

// Built without weights, urge curve or generator.
BBMoveToFixed::BBMoveToFixed(int behaviourId, int priority)
    : BotBehaviour(0, 0, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_targets = 0;
    m_generator = 0;
    const int bots = configuredBotCount();
    m_float24 = checkPointer(new float[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_float24[i] = 0.0f;
    }
}

BBMoveToFixed::~BBMoveToFixed()
{
    delete[] m_float24;
}

float BBMoveToFixed::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMoveToFixed::calculateUrgency");
}

BBMedicAssist::BBMedicAssist(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBMedicAssist::~BBMedicAssist()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBMedicAssist::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMedicAssist::calculateUrgency");
}

BBRevive::BBRevive(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

BBRevive::~BBRevive()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBRevive::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRevive::calculateUrgency");
}

BBBridgeRepair::BBBridgeRepair(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle38 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_battle3C = checkPointer(new int[configuredBattleStrengthCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBBridgeRepair::~BBBridgeRepair()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_battle38;
    delete[] m_battle3C;
}

float BBBridgeRepair::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBBridgeRepair::calculateUrgency");
}

bool BBMedicAssist::isApplicable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBMedicAssist::isApplicable");
}

bool BBRevive::isApplicable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRevive::isApplicable");
}

bool BBBridgeRepair::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBBridgeRepair::isUsable");
}

BBIdle::BBIdle(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_double24 = new double[configuredBotCount()];
    m_bool28 = new bool[configuredBotCount()];
    m_int2C = new int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_double24[i] = 0.0;
        m_bool28[i] = false;
        m_int2C[i] = 0;
    }
    m_targets = 0;
}

BBIdle::~BBIdle()
{
    delete[] m_double24;
    delete[] m_bool28;
    delete[] m_int2C;
}

// PE 100F46C0. Restores every bot's state, not just this bot's, and clears the target array
// pointer; neither the generator nor the base state is reset.
void BBIdle::reset(Bot*)
{
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_double24[i] = 0.0;
        m_bool28[i] = false;
        m_int2C[i] = 0;
    }
    m_targets = 0;
}

float BBIdle::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBIdle::calculateUrgency");
}

// The generator must be a fire generator; it learns its owner after the arrays are set.
BBFire::BBFire(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
               float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    m_int24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
    m_curve = new SampledFunction(fireCurve, 0.0f, 1.0f, 0.03f);
}

// PE 101029E0.
BBFire::~BBFire()
{
    delete[] m_targets;
    delete[] m_int24;
    if (m_curve)
        delete m_curve;
}

float BBFire::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFire::calculateUrgency");
}

// PE 10100C90. The bot's firing target becomes the designator.
void BBFire::fireAtPosition(int designator, const Vec3& position, Bot* bot, int weapon)
{
    bot->setFiringTarget(static_cast<unsigned int>(designator));
    static_cast<FirePlanGenerator*>(m_generator)->setPositionTarget(bot, designator, position,
                                                                     weapon);
}

// PE 10100C30.
void BBFire::fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB)
{
    bot->setFiringTarget(m_targets[bot->getBotId()]);
    static_cast<FirePlanGenerator*>(m_generator)->setObjectTarget(
        bot, m_targets[bot->getBotId()], weapon, flagA, flagB);
}

BBFireInfantery::BBFireInfantery(int behaviourId, int priority, PlanGenerator* generator,
                                 UrgeCurve* urgeCurve, float* weights)
    : BBFire(behaviourId, priority, generator, urgeCurve, weights)
{
    m_infanteryGenerator = generator;
}

BBFireInfantery::~BBFireInfantery()
{
}

// PE 10119BE0 jumps to the BBFire body.
void BBFireInfantery::fireAtTarget(Bot* bot, int weapon, bool flagA, bool flagB)
{
    BBFire::fireAtTarget(bot, weapon, flagA, flagB);
}

BBFire3d::BBFire3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
                   float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = new unsigned int[bots];
    m_int24 = new unsigned int[bots];
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

// PE 10115580.
BBFire3d::~BBFire3d()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFire3d::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFire3d::calculateUrgency");
}

// Built without weights, urge curve or generator.
BBFireUnarmed::BBFireUnarmed(int behaviourId, int priority)
    : BotBehaviour(0, 0, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    const int bots = configuredBotCount();
    m_int24 = checkPointer(new int[bots]);
    m_targets = checkPointer(new unsigned int[bots]);
    m_generator = 0;
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = 0;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBFireUnarmed::~BBFireUnarmed()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFireUnarmed::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireUnarmed::calculateUrgency");
}

BBFireLargeBore::BBFireLargeBore(int behaviourId, int priority, PlanGenerator* generator,
                                 UrgeCurve* urgeCurve, float* weights, bool flag)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_double28 = 0.0;
    m_bool3C = flag;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    m_int24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

// PE 10110370.
BBFireLargeBore::~BBFireLargeBore()
{
    delete[] m_targets;
    delete[] m_int24;
}

float BBFireLargeBore::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireLargeBore::calculateUrgency");
}

void BBFireLargeBore::fireAtTarget(Bot*, int, bool, bool, bool)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireLargeBore::fireAtTarget");
}

// The cover array is allocated twice and the first block is lost, as shipped.
BBTakeCover::BBTakeCover(int behaviourId, int priority, PlanGenerator* generator,
                         UrgeCurve* urgeCurve, float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_botCount = configuredBotCount();
    m_cover = checkPointer(new void*[m_botCount]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[m_botCount]);
    m_cover = checkPointer(new void*[m_botCount]);
    for (int i = 0; i < m_botCount; ++i)
    {
        m_targets[i] = static_cast<unsigned int>(-1);
        m_urgency[i] = 0.0f;
        m_cover[i] = 0;
    }
}

// PE 10119D50. The per-bot cover entries are released as plain blocks.
BBTakeCover::~BBTakeCover()
{
    for (int i = 0; i < m_botCount; ++i)
        ::operator delete(m_cover[i]);
    delete[] m_targets;
    delete[] m_cover;
}

void BBTakeCover::reset(Bot* bot)
{
    BotBehaviour::reset(bot);
    if (m_cover[bot->getBotId()])
        ::operator delete(m_cover[bot->getBotId()]);
    m_cover[bot->getBotId()] = 0;
}

float BBTakeCover::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTakeCover::calculateUrgency");
}

// Owns an unarmed and a large-bore behaviour with the same id and priority.
BBFireDriver::BBFireDriver(int behaviourId, int priority, PlanGenerator* generator,
                           UrgeCurve* urgeCurve, float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_unarmed = checkPointer(new BBFireUnarmed(behaviourId, priority));
    m_largeBore = checkPointer(
        new BBFireLargeBore(behaviourId, priority, generator, urgeCurve, weights, false));
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    m_targets = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    static_cast<FirePlanGenerator*>(generator)->setOwner(this);
}

BBFireDriver::~BBFireDriver()
{
    delete[] m_targets;
    if (m_unarmed)
        delete m_unarmed;
    if (m_largeBore)
        delete m_largeBore;
}

float BBFireDriver::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBFireDriver::calculateUrgency");
}

BBChange::BBChange(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_owned44 = 0;
    m_owned48 = 0;
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

// PE 100F7420.
BBChange::~BBChange()
{
    delete[] m_targets;
    if (m_owned44)
        delete m_owned44;
    if (m_owned48)
        delete m_owned48;
}

float BBChange::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChange::calculateUrgency");
}

BBChangeLandingCraft::BBChangeLandingCraft(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBChangeLandingCraft::~BBChangeLandingCraft()
{
    delete[] m_targets;
}

float BBChangeLandingCraft::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChangeLandingCraft::calculateUrgency");
}

BBChangeTeleport::BBChangeTeleport(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBChangeTeleport::~BBChangeTeleport()
{
    delete[] m_targets;
}

float BBChangeTeleport::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBChangeTeleport::calculateUrgency");
}

BBSuicide::BBSuicide(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

BBSuicide::~BBSuicide()
{
}

float BBSuicide::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBSuicide::calculateUrgency");
}

BBCallForMedic::BBCallForMedic(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
}

BBCallForMedic::~BBCallForMedic()
{
    delete[] m_targets;
}

float BBCallForMedic::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBCallForMedic::calculateUrgency");
}

bool BBCallForMedic::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBCallForMedic::isUsable");
}

BBTakeCover3d::BBTakeCover3d(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = new unsigned int[configuredBotCount()];
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBTakeCover3d::~BBTakeCover3d()
{
    delete[] m_targets;
}

float BBTakeCover3d::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTakeCover3d::calculateUrgency");
}

BBC4::BBC4(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBC4::~BBC4()
{
    delete[] m_targets;
}

float BBC4::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBC4::calculateUrgency");
}

bool BBC4::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBC4::isUsable");
}

BBRunOverSoldier::BBRunOverSoldier(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
}

BBRunOverSoldier::~BBRunOverSoldier()
{
    delete[] m_targets;
}

float BBRunOverSoldier::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRunOverSoldier::calculateUrgency");
}

bool BBRunOverSoldier::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRunOverSoldier::isUsable");
}

BBDefuseMines::BBDefuseMines(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_battle34 = checkPointer(new int[configuredBattleStrengthCount()]);
    m_int38 = checkPointer(new int[configuredBotCount()]);
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_int38[i] = -1;
    }
}

BBDefuseMines::~BBDefuseMines()
{
    delete[] m_targets;
    delete[] m_battle34;
    delete[] m_int38;
}

float BBDefuseMines::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBDefuseMines::calculateUrgency");
}

bool BBDefuseMines::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBDefuseMines::isUsable");
}

BBRandom::BBRandom(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
    m_double28 = 0.0;
    m_bool24 = false;
    m_double30 = 0.0;
}

BBRandom::~BBRandom()
{
    delete[] m_targets;
}

float BBRandom::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRandom::calculateUrgency");
}

bool BBRandom::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBRandom::isUsable");
}

BBTriggerable::BBTriggerable(int behaviourId, int priority, PlanGenerator* generator, UrgeCurve* urgeCurve,
           float* weights)
    : BotBehaviour(weights, urgeCurve, configuredBotCount())
{
    m_behaviourId = static_cast<unsigned char>(behaviourId);
    m_priority = static_cast<unsigned char>(priority);
    m_generator = generator;
    m_targets = checkPointer(new unsigned int[configuredBotCount()]);
    const int bots = configuredBotCount();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
    }
    m_int24 = 0;
}

BBTriggerable::~BBTriggerable()
{
    delete[] m_targets;
}

float BBTriggerable::calculateUrgency(Bot*, bool, float)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTriggerable::calculateUrgency");
}

bool BBTriggerable::isUsable(Information*, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBTriggerable::isUsable");
}

} // namespace ai
} // namespace hfe
} // namespace dice
