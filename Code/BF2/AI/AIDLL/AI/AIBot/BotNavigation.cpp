// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotNavigation operations.

#include "BotNavigation.h"
#include <cstring>

#include "BotMain.h"                       // getBotId / getCurrentEquipment - vtable +0xE8, +0xD8
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"   // IBFTimer::getGameTime
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"

namespace dice {
namespace hfe {
namespace ai {

// Creates navigation paths and initializes movement state for the supplied bot.
BotNavigation::BotNavigation(BotMain* owner)
    : m_bot(owner)
    , m_float4(2.0f)
    , m_float8(2.0f)
    , m_int16(-1)
    , m_int20(-1)
    , m_int36(-1)
    , m_flag40(false)
    // the two paths, in the order the binary builds them: the fine one first.
    , m_polygonPath(checkPointer(new QTPath()))
    , m_int76(-1)
    , m_int116(0)
    , m_clusterPath(checkPointer(new ClusterNodePath()))
    , m_int128(-1)
    , m_double144(0.0)
    , m_double152(0.0)
    , m_double160(0.0)
    , m_double168(0.0)
    // 9,999,999.0f - a distance sentinel, not a time and not a magnitude. See above.
    , m_sentinel188(9999999.0f)
    , m_flag192(false)
    , m_flag193(false)
    , m_flag195(false)
    , m_int200(-1)
    , m_int204(-1)
    , m_int208(-1)
    , m_int212(-1)
    , m_int240(0)
    , m_double268(0.0)
    , m_flag276(false)
    // Independent sentinel, initialized to -1.0.
    , m_double300(-1.0)
{
    for (int i = 0; i < 3; ++i)
    {
        m_zero24[i]  = 0;
        m_zero44[i]  = 0;
        m_zero176[i] = 0;
        m_zero216[i] = 0;
        m_zero256[i] = 0;
    }
    m_pathOwner = 0;                   // +60 - a pointer, see BotNavigation.h
    m_zero64 = 0;
    m_zero308[0] = 0;
    m_zero308[1] = 0;
    for (int i = 0; i < 3; ++i)
        m_zero280[i] = 0;
    m_float292 = 0.0f;
    m_float296 = 0.0f;
    for (int i = 0; i < 6; ++i)
        m_zero320[i] = 0;

    // the dead resize - see the block above. Written as what it is rather than dropped.
    m_points.resize(0);
}

// [FRONTIER] Remaining navigation operations stop until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

BotNavigation::~BotNavigation()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BotNavigation::~BotNavigation");
}

// PE 101742E0.
bool BotNavigation::isInsideObstacle(const Vec3& position) const
{
    for (unsigned int i = 0; i < m_obstacles.size(); ++i)
    {
        Information* const obstacle = informationManager->getInformation(m_obstacles[i]);
        if (!obstacle || ((obstacle->getObjectTypeFlags() >> 21) & 1) || !obstacle->getPhysical())
            continue;
        const Vec3& centre = obstacle->getPosition();
        const float dx = position.x - centre.x;
        const float dy = position.y - centre.y;
        const float dz = position.z - centre.z;
        const float radius = obstacle->getPhysical()->getSmallestRadius();
        if (radius * radius > dz * dz + dx * dx + dy * dy)
            return true;
    }
    return false;
}

// PE 101729D0.
bool BotNavigation::getAvoidPosition(Vec3& position, double& timeLeft) const
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    std::memcpy(&position, m_zero256, sizeof(position));
    timeLeft = 2.8 - (now - m_double268);
    return timeLeft > 0.0;
}

// Resets navigation, movement flags and progress timing.
void BotNavigation::resetAll()
{
    reset();

    m_obstacleCount = 0;
    m_flag276 = false;
    // Not the constructor's 0.0 - see the block above.
    m_double268 = -9999.0;
    m_zero308[0] = 0;
    m_double300 = -1.0;

    clearProgress();
}

// Resets navigation state and returns path resources to their owners.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] Incomplete resource paths stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Implement and validate path release and progress reset.
// [RETIRE-WHEN: botmain-bases] Remove when the corresponding implementation is complete and validated.
void BotNavigation::reset()
{
    m_flag316 = false;

    // giving the polygon path back. See the block above.
    if (m_int76 != -1)
    {
        // Retail: `m_pathOwner->release(&m_int76, m_bot->getBotId)`. The bot half is typed now;
        // the object at +60 still has no class, so the slot cannot be.
        BF2_AI_FRONTIER("dice::hfe::ai::BotNavigation::reset (a polygon path is reserved)");
        m_int76 = -1;
    }

    // and the cluster path, through a different slot on the same object.
    if (m_int128 != -1)
    {
        BF2_AI_FRONTIER("dice::hfe::ai::BotNavigation::reset (a cluster path is reserved)");
        m_int128 = -1;
    }

    // Reaching through both path objects - see the block above.
    m_polygonPath->m_nodes.resize(0);
    m_polygonPath->m_i0 = 0;
    m_polygonPath->m_cost = 0.0f;

    m_clusterPath->m_nodes.resize(0);
    m_clusterPath->m_i0 = 0;

    m_int56 = 0;

    m_points.resize(0);

    m_int240 = 0;
    m_int116 = 0;
    m_double160 = 0.0;
    m_flag195 = false;
    m_flag193 = false;
    m_flag194 = false;
    m_double244 = 0.0;

    // Resolves the bot's current equipment handle; skips equipment work when it has no object.
    const unsigned int equipment = m_bot->getCurrentEquipment();
    Information* const info = informationManager->getPool()->get(equipment);
    // PE 10174EB7: a mobile equipment seeds the last valid positions for its vehicle number and
    // for infantry.
    if (info && info->getMobile())
    {
        IPIMobile* const mobile = info->getMobile();
        m_vehicleNumber = mobile->getVehicleNumber();
        const IPIMobile::ValidPos& last = mobile->getLastValidPosition(m_vehicleNumber);
        m_int204 = static_cast<int>(last.quadTree);
        m_int200 = static_cast<int>(last.polygon);
        m_int212 = static_cast<int>(mobile->getLastValidInfantryPos().quadTree);
        m_int208 = static_cast<int>(mobile->getLastValidInfantryPos().polygon);
    }

    // The sentinel is the interesting one - see the block above.
    m_sentinel188 = 9999999.0f;
    m_float292 = 0.0f;
    m_float296 = 0.0f;
}

// Clears progress counters and restarts the progress timer at the current game time.
// Paused game time does not accumulate lack of progress.
void BotNavigation::clearProgress()
{
    m_zero176[0] = 0;                      // +176

    // the one non-zero store. See the block above.
    m_double144 =
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    m_double168 = 0.0;
    m_zero176[1] = 0;                      // +180
    m_zero176[2] = 0;                      // +184
    m_double152 = 0.0;
    m_flag192 = false;
    m_zero280[0] = 0;                      // +280
    m_zero280[1] = 0;                      // +284
    m_zero280[2] = 0;                      // +288
}

namespace {

// Primary mobile objects, and anything of type bit 25. PE vtable 102811F0, PE 10172770.
// [INFERRED] name.
class ObstaclePredicate : public IAIInformationGridPredicate
{
public:
    bool includeInformation(Information* information) const override
    {
        if (information->isPrimary() && information->hasMobile())
            return true;
        return ((information->getObjectTypeFlags() >> 25) & 1) != 0;
    }
};

} // anonymous namespace

// PE 101749A0. Collects obstacles within 50 of the current equipment from both grids of the
// bot's side. The equipment is not null-checked.
void BotNavigation::updateObstacles()
{
    m_obstacles.resize(0);
    const Vec3& position =
        informationManager->getInformation(m_bot->getCurrentEquipment())->getPosition();
    ObstaclePredicate predicate;
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getObjectsWithinRadius(m_obstacles, position, 50.0f, m_bot->getSide(), true, &predicate);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getObjectsWithinRadius(m_obstacles, position, 50.0f, m_bot->getSide(), false, &predicate);
    m_obstacleCount = static_cast<int>(m_obstacles.size());
}

// PE 10172230. True within three seconds of the time held in +244.
bool BotNavigation::isActive()
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    return now - m_double244 < 3.0;
}

void BotNavigation::updateOccationally()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BotNavigation::updateOccationally");
}

// Always returns true, leaving BotManager's final pathfinding sweep gated by isSearchActive.
bool BotNavigation::isRunning()
{
    return true;
}

void BotNavigation::getMeshPosition(int, unsigned int&, unsigned int&, Vec3&)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BotNavigation::getMeshPosition");
}

} // namespace ai
} // namespace hfe
} // namespace dice
