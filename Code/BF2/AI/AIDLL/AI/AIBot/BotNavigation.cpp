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
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IAIPathfinding.h"
#include "../AIPathFinding/NavMesh/QuadTree.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Core/Debug.h"
#include <cmath>
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
    , m_startIsland(static_cast<unsigned int>(-1))
    , m_startPolygon(static_cast<unsigned int>(-1))
    , m_goalPolygon(static_cast<unsigned int>(-1))
    , m_goalMaterial(0)
    // the two paths, in the order the binary builds them: the fine one first.
    , m_polygonPath(checkPointer(new QTPath()))
    , m_int76(static_cast<unsigned int>(-1))
    , m_int116(0)
    , m_clusterPath(checkPointer(new ClusterNodePath()))
    , m_int128(static_cast<unsigned int>(-1))
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
        m_zero176[i] = 0;
        m_zero256[i] = 0;
    }
    m_start.x = m_start.y = m_start.z = 0.0f;
    m_goal.x = m_goal.y = m_goal.z = 0.0f;
    m_tracePosition.x = m_tracePosition.y = m_tracePosition.z = 0.0f;
    m_pathOwner = 0;
    m_quadTree = 0;
    m_largestAngleDiff = 0.0f;
    m_pathLength = 0.0f;
    for (int i = 0; i < 3; ++i)
        m_zero280[i] = 0;
    m_float292 = 0.0f;
    m_float296 = 0.0f;
    m_searchStart.x = m_searchStart.y = m_searchStart.z = 0.0f;
    m_searchGoal.x = m_searchGoal.y = m_searchGoal.z = 0.0f;

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
    m_largestAngleDiff = 0.0f;
    m_double300 = -1.0;

    clearProgress();
}

// Resets navigation state and returns path resources to their owners.
// PE 10174D70.
void BotNavigation::reset()
{
    m_flag316 = false;

    if (m_int76 != static_cast<unsigned int>(-1))
    {
        m_pathOwner->releaseResource(m_int76, m_bot->getBotId());
        m_int76 = static_cast<unsigned int>(-1);
    }

    if (m_int128 != static_cast<unsigned int>(-1))
    {
        m_pathOwner->releaseClusterResource(m_int128, m_bot->getBotId());
        m_int128 = static_cast<unsigned int>(-1);
    }

    // Reaching through both path objects - see the block above.
    m_polygonPath->m_nodes.resize(0);
    m_polygonPath->m_i0 = 0;
    m_polygonPath->m_cost = 0.0f;

    m_clusterPath->m_nodes.resize(0);
    m_clusterPath->m_i0 = 0;

    m_state = 0;

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

namespace {

// Component-wise equality within FLT_EPSILON, as the retail vector comparison. PE 100D2910.
bool isSamePosition(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= 1.1920929e-07f
        && std::fabs(a.y - b.y) <= 1.1920929e-07f
        && std::fabs(a.z - b.z) <= 1.1920929e-07f;
}

// The horizontal part (x, z) of a position. PE 1014ADC0.
Vec2 flatOf(const Vec3& position)
{
    Vec2 flat;
    flat.x = position.x;
    flat.y = position.z;
    return flat;
}

// The conditional Vec2 normaliser: unit vectors are left alone, near-zero ones are zeroed.
// PE 100E9DE0.
void normaliseFlat(Vec2& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = 0.0f;
        value.y = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
}

// The signed angle from one unit vector to another, in (-pi, pi]. PE 1014B650.
float flatAngle(const Vec2& from, const Vec2& to)
{
    float cross = from.y * to.x - from.x * to.y;
    if (-1.0f > cross)
        cross = -1.0f;
    else if (1.0f < cross)
        cross = 1.0f;
    // Retail clamps a second time through its generic clamp helper.
    cross = cross < -1.0f ? -1.0f : (cross > 1.0f ? 1.0f : cross);
    float angle = static_cast<float>(1.5707963705062866 - std::acos(static_cast<double>(cross)));
    if (to.y * from.y + from.x * to.x < 0.0f)
    {
        if (angle > 0.0f)
            angle = 1.0f * 3.1415927410125732f - angle;
        else if (angle < 0.0f)
            angle = -1.0f * 3.1415927410125732f - angle;
        else
            angle = 0.0f * 3.1415927410125732f - angle;
    }
    return angle;
}

// Where the line through p1 and p2 meets the line through a and b, as the parameter along
// p1->p2 and the parameter along a->b. False for parallel lines. PE 1014B030.
bool intersectLines(const Vec2& p1, const Vec2& p2, const Vec2& a, const Vec2& b, float& along,
                    float& alongEdge)
{
    const float denominator = (b.y - a.y) * (p2.x - p1.x) - (p2.y - p1.y) * (b.x - a.x);
    if (denominator == 0.0f)
        return false;
    const float inverse = 1.0f / denominator;
    along = ((p1.y - a.y) * (b.x - a.x) - (p1.x - a.x) * (b.y - a.y)) * inverse;
    alongEdge = ((p1.y - a.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p1.x - a.x)) * inverse;
    return true;
}

// Moves a point to a fraction of the way along its edge.
void placeOnEdge(BotNavigation::NavigationPoint& point, float fraction)
{
    point.m_position.x = (point.m_vec12.x - point.m_vec0.x) * fraction + point.m_vec0.x;
    point.m_position.y = (point.m_vec12.y - point.m_vec0.y) * fraction + point.m_vec0.y;
    point.m_position.z = (point.m_vec12.z - point.m_vec0.z) * fraction + point.m_vec0.z;
}

// The trace clearance used by everything but ground vehicles in updateNavPoints. Zero-
// initialised and never written in the retail image. [UNRESOLVED] name.
float s_navPointClearance = 0.0f;

// Zero-initialised and never written in the retail image, so the stuck restart it selects
// is unreachable there. [UNRESOLVED] name.
bool s_restartWithSkip = false;

} // anonymous namespace

// The equipment is not null-checked.
void BotNavigation::getTraceData(int map, unsigned int& polygon, unsigned int& island, Vec3& position)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition(map);

    if (m_traceMap == map && isSamePosition(valid.position, m_tracePosition))
    {
        position = m_tracePosition;
        if (map == IAIPathfinding::instance->getTraceMapIndex())
        {
            polygon = static_cast<unsigned int>(m_int208);
            island = static_cast<unsigned int>(m_int212);
        }
        else if (map == m_vehicleNumber)
        {
            polygon = static_cast<unsigned int>(m_int200);
            island = static_cast<unsigned int>(m_int204);
        }
        else
        {
            Debug(DebugTypeError, __FILE__, __LINE__, "") << "Should not be here!";
        }
        return;
    }

    m_traceMap = map;
    m_tracePosition = valid.position;
    position = valid.position;
    polygon = valid.polygon;
    island = valid.quadTree;
    if (m_vehicleNumber == map)
    {
        m_int200 = static_cast<int>(valid.polygon);
        m_int204 = static_cast<int>(valid.quadTree);
    }
    if (map == IAIPathfinding::instance->getTraceMapIndex())
    {
        m_int208 = static_cast<int>(valid.polygon);
        m_int212 = static_cast<int>(valid.quadTree);
    }
}

bool BotNavigation::initPathfinding(const Vec3& to, float a, float b)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info)
    {
        m_flag193 = false;
        return false;
    }

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    unsigned int polygon;
    unsigned int island;
    const int equipmentType = info->getUnit()->getEquipmentType();
    if (!pathfinding->getPolygon(to, info->getMobile()->getVehicleNumber(), polygon, island,
                                 equipmentType))
    {
        m_flag193 = false;
        return false;
    }
    return initPathfinding(island, polygon, to, a, b);
}

bool BotNavigation::initPathfinding(unsigned int island, unsigned int polygon, const Vec3& to, float a,
                                    float b)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info)
    {
        m_flag193 = false;
        return false;
    }

    m_flag236 = ((info->getObjectTypeFlags() >> 21) & 1) != 0;
    if (!m_pathOwner)
        m_pathOwner = IAIPathfinding::instance;

    IPIMobile* const mobile = info->getMobile();
    const IPIMobile::ValidPos& valid = mobile->getLastValidPosition();

    if (!isSamePosition(to, m_goal))
        clearProgress();

    if (m_flag193 && isSamePosition(m_goal, to) && mobile->getVehicleNumber() == m_vehicleNumber
        && info->getUnit()->getEquipmentType() == m_equipmentType)
    {
        if (m_state == 0 || m_state == 1)
            return true;
        if (m_points.size() != 0 && m_points[m_int116].m_polygon == valid.polygon)
            return true;
        if (m_points.size() != 0 && stillOnPath())
            return true;
        if (m_points.size() > 1)
        {
            resumeClusterPath();
            return true;
        }
    }

    if (island != static_cast<unsigned int>(-1) && polygon != static_cast<unsigned int>(-1))
    {
        m_goalPolygon = polygon;
        m_goal = to;
        if (valid.quadTree != static_cast<unsigned int>(-1)
            && valid.polygon != static_cast<unsigned int>(-1))
        {
            const Vec3& position = info->getPosition();
            const float dx = valid.position.x - position.x;
            const float dy = valid.position.y - position.y;
            const float dz = valid.position.z - position.z;
            if (!(std::sqrt(dx * dx + dy * dy + dz * dz) > 100.0f))
            {
                if (island != valid.quadTree)
                {
                    Debug(DebugTypeError, __FILE__, __LINE__, "")
                        << "Bot " << m_bot->getName()
                        << " is ordered to go to an island that it is not on (" << island << " != "
                        << valid.quadTree << ")";
                    m_flag193 = false;
                    return false;
                }

                m_flag68 = true;
                m_flag120 = true;
                m_flag192 = false;
                m_startIsland = valid.quadTree;
                m_island112 = valid.quadTree;
                m_startPolygon = valid.polygon;
                m_start = valid.position;
                m_float8 = b;
                m_float8Squared = b * b;
                m_float4 = a;
                m_vehicleNumber = mobile->getVehicleNumber();
                const IPIMobile::ValidPos& last = mobile->getLastValidPosition(m_vehicleNumber);
                m_int204 = static_cast<int>(last.quadTree);
                m_int200 = static_cast<int>(last.polygon);
                m_int212 = static_cast<int>(mobile->getLastValidInfantryPos().quadTree);
                m_int208 = static_cast<int>(mobile->getLastValidInfantryPos().polygon);
                m_equipmentType = info->getUnit()->getEquipmentType();
                m_flag195 = false;
                m_int240 = 0;
                reset();
                m_flag193 = true;
                m_smallestRadius = info->getPhysical()->getSmallestRadius();
                m_pathOwner->getMaterial(m_vehicleNumber, island, m_goalPolygon, m_goalMaterial);
                testDirectPath();
                m_quadTree = static_cast<QuadTree*>(m_pathOwner->getQuadTree(m_vehicleNumber, m_startIsland));
                m_flag316 = true;
                return true;
            }
        }
    }

    m_flag192 = true;
    m_flag193 = false;
    return false;
}

void BotNavigation::testDirectPath()
{
    const float dx = m_goal.x - m_start.x;
    const float dy = m_goal.y - m_start.y;
    const float dz = m_goal.z - m_start.z;
    if (!(dz * dz + dy * dy + dx * dx < 40000.0f))
        return;

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info || !info->getMobile())
        return;

    Vec3 hit;
    hit.x = hit.y = hit.z = 0.0f;
    unsigned int polygon;
    unsigned char material;
    if (!m_pathOwner->trace(m_startPolygon, m_island112, m_vehicleNumber, m_start, m_goal, hit, polygon,
                            m_equipmentType, -1, 0.0f, 5, &material))
        return;

    const unsigned int index = static_cast<unsigned int>(m_points.size());
    m_points.resize(index + 1);
    NavigationPoint& point = m_points[index];
    point.m_position = m_goal;
    point.m_polygon = polygon;
    point.m_vec0 = m_goal;
    point.m_vec12 = m_goal;
    m_goalPolygon = polygon;
    m_goalMaterial = material;
    m_flag68 = false;
    m_flag120 = false;
    m_int116 = 0;
    m_state = 2;
    updateDistanceLeft();
}

// The points are read again after stillOnPath and resumeClusterPath, either of which may
// have moved the cursor or emptied the vector; retail reads the old storage in that case.
bool BotNavigation::updatePath(Vec2& target, bool& arrived, bool& waiting, float distance)
{
    if (m_flag192)
        return false;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    m_double244 = now;
    if (!m_flag193)
        return false;

    arrived = false;
    waiting = false;
    if (m_state == 1 || m_state == 0)
        m_double244 = 0.0;

    if (m_flag120 || m_flag195
        || (m_flag68 && static_cast<unsigned int>(m_int116) >= m_points.size()))
    {
        waiting = true;
        return true;
    }

    // A firing ground vehicle (object type bit 21) holds still on material 4 while its target
    // is on another side.
    Information* const firingTarget = informationManager->getInformation(m_bot->getFiringTarget());
    bool holdForTarget = false;
    if (m_points.size() != 0 && m_flag236 && m_bot->isFiring() && firingTarget)
    {
        const int side = m_bot->getSide();
        holdForTarget = firingTarget->getSide() != side;
    }
    if (holdForTarget)
    {
        const unsigned int polygon =
            m_points.size() > 1 ? m_points[m_int116].m_polygon : m_goalPolygon;
        unsigned char material;
        if (m_pathOwner->getMaterial(m_vehicleNumber, m_island112, polygon, material) && material == 4)
        {
            waiting = true;
            m_double244 = 0.0;
            return true;
        }
    }

    if (m_double168 == -666.0)
    {
        m_bot->playerInput()->setInput(io::PIAction, true);
        m_double168 = now;
    }

    if (checkArrivedAtPoint(distance))
    {
        const unsigned int current = static_cast<unsigned int>(m_int116);
        if (current == m_points.size() - 1)
        {
            m_double244 = 0.0;
            m_sentinel188 = 100000000.0f;
            clearProgress();
            arrived = true;
        }
        else
        {
            m_int116 = static_cast<int>(current + 1);
            if (m_int240 > 0)
                --m_int240;
        }
    }

    // The polygon's cluster is compared with the cluster path's cursor, not with a cluster.
    bool keepPoints = false;
    if (!(m_pathLength < 30.0f))
    {
        const unsigned int current = static_cast<unsigned int>(m_int116);
        if (m_points.size() > current + 10)
        {
            const int cursor = m_clusterPath->m_i0;
            if (m_quadTree->getPolygon(m_points[current].m_polygon)->m_cluster != cursor)
            {
                const Polygon* const polygon = m_quadTree->getPolygon(m_points[m_int116].m_polygon);
                if (polygon->m_cluster != m_clusterPath->getCluster(0))
                    keepPoints = true;
            }
        }
    }
    if (!keepPoints)
    {
        const unsigned int clusters = static_cast<unsigned int>(m_clusterPath->m_nodes.size());
        if (!(static_cast<unsigned int>(m_clusterPath->m_i0) + 1 > clusters) && !m_flag68)
        {
            m_flag68 = true;
            unsigned int count = static_cast<unsigned int>(m_int116) + 10;
            if (count >= m_points.size())
                count = static_cast<unsigned int>(m_points.size());
            m_points.resize(count);
        }
    }

    if (!m_flag195)
    {
        if (m_state != 2)
        {
            if (!m_flag68)
                return false;
            if (m_state != 1 && m_state != 0)
                return false;
        }

        stillOnPath();
        if (m_flag194)
        {
            const double limit = m_flag236 ? 2.0 : 0.5;
            if (now - m_double160 > limit)
            {
                m_flag194 = false;
                if (s_restartWithSkip && m_flag236)
                    m_int240 = 1;
                else
                    m_int240 = 0;
                resumeClusterPath();
            }
        }
        else
        {
            m_double160 = now;
        }
    }

    target = flatOf((m_points.data() + m_int116)->m_position);
    return true;
}

// The equipment is not null-checked. Ground vehicles (object type bit 21) arrive within 0.5;
// anything else within the larger of the request's radius and 1, widened at the last point to
// the equipment's smallest radius less 1.
bool BotNavigation::checkArrivedAtPoint(float)
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3 position = info->getPosition();
    if (m_points.size() == 0)
        return false;

    const Vec3& point = m_points[m_int116].m_position;
    if (!(std::fabs(position.y - point.y) <= 3.0f))
        return false;

    float radius;
    if (m_flag236)
    {
        radius = 0.5f;
    }
    else
    {
        radius = 1.0f < m_float4 ? m_float4 : 1.0f;
        if (static_cast<unsigned int>(m_int116) == m_points.size() - 1)
        {
            const float body = m_smallestRadius - 1.0f;
            radius = radius < body ? body : radius;
        }
    }

    const float dx = position.x - point.x;
    const float dz = position.z - point.z;
    return radius * radius > dz * dz + dx * dx;
}

// PE 10174130. Starting at the current point, hands out each point's flat position until one
// lies at least `distance` from `from`; with none, the last point examined (or nothing).
void BotNavigation::getFirstPointBeyondDist(const Vec2& from, float distance, Vec2& out)
{
    for (unsigned int i = static_cast<unsigned int>(m_int116); i < m_points.size(); ++i)
    {
        out = flatOf(m_points[i].m_position);
        const float dx = from.x - out.x;
        const float dy = from.y - out.y;
        if (distance * distance < dx * dx + dy * dy)
            return;
    }
}

bool BotNavigation::getSecondPoint(Vec2& out)
{
    if (m_points.size() > static_cast<unsigned int>(m_int116) + 1)
    {
        out = flatOf(m_points[m_int116 + 1].m_position);
        return true;
    }
    return false;
}

// The equipment is not null-checked, and neither is the current point's existence.
bool BotNavigation::getLargestAngleDiff(float& out)
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - m_double300 < 0.5)
    {
        out = m_largestAngleDiff;
        return true;
    }
    m_double300 = now;

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const NavigationPoint* const current = m_points.data() + m_int116;
    const Vec3& position = info->getPosition();
    float dx = position.x - current->m_position.x;
    float dy = position.y - current->m_position.y;
    float dz = position.z - current->m_position.z;
    float length = std::sqrt(dy * dy + dz * dz + dx * dx);

    unsigned int i = static_cast<unsigned int>(m_int116);
    const Vec2 facing = flatOf(info->getFaceing());
    out = 0.0f;

    const float reach = info->getMobile()->getSpeedMagnitude() * 2.0f;
    const float limit = 3.0f < reach ? reach : 3.0f;

    while (length < limit && i < m_points.size())
    {
        Vec3 previous;
        if (i == 0)
            previous = info->getPosition();
        else
            previous = m_points[i - 1].m_position;
        const Vec3& point = m_points[i].m_position;

        dx = previous.x - point.x;
        dy = previous.y - point.y;
        dz = previous.z - point.z;
        length += std::sqrt(dz * dz + dy * dy + dx * dx);

        Vec3 direction;
        direction.x = point.x - previous.x;
        direction.y = point.y - previous.y;
        direction.z = point.z - previous.z;
        Vec2 flat = flatOf(direction);
        normaliseFlat(flat);
        const float angle = std::fabs(flatAngle(facing, flat));
        if (angle > out)
            out = angle;
        ++i;
    }

    m_largestAngleDiff = out;
    return true;
}

bool BotNavigation::update(bool& finished, bool* busy)
{
    if (!m_pathOwner)
        return false;
    m_flag316 = false;
    if (m_state == 4)
        return false;

    finished = false;
    if (busy)
        *busy = false;

    if (m_flag195)
    {
        m_points.resize(0);
        m_int116 = 0;
        m_flag68 = true;
        m_flag195 = false;
    }

    if (m_flag120)
        return doClusterSearch(finished, busy);
    if (m_flag68)
        return doPolygonSearch(finished, busy);
    return true;
}

bool BotNavigation::doClusterSearch(bool& finished, bool* busy)
{
    if (m_int128 == static_cast<unsigned int>(-1)
        && !m_pathOwner->acquireClusterResource(m_int128, m_bot->getBotId()))
    {
        if (busy)
            *busy = true;
        return false;
    }

    m_state = m_pathOwner->clusterSearch(m_int128, m_vehicleNumber, m_startIsland, m_startPolygon, m_start,
                                         m_goalPolygon, m_goal, m_equipmentType, m_clusterPath);
    if (m_state == 1)
    {
        finished = true;
        return true;
    }

    m_pathOwner->releaseClusterResource(m_int128, m_bot->getBotId());
    m_int128 = static_cast<unsigned int>(-1);
    if (m_state == 2)
    {
        m_flag120 = false;
        m_state = 0;
        m_flag68 = true;
        return true;
    }
    if (m_state == 3 || m_state == 4)
    {
        reset();
        return false;
    }
    finished = true;
    return true;
}

// The cluster list handed to the search holds the cursor's cluster and the next two; one
// past the end goes in as 0xFFFF.
bool BotNavigation::doPolygonSearch(bool& finished, bool* busy)
{
    if (m_int76 == static_cast<unsigned int>(-1))
    {
        if (!m_pathOwner->acquireResource(m_int76, m_bot->getBotId()))
        {
            if (busy)
                *busy = true;
            return false;
        }

        if (m_vehicleNumber != m_pathOwner->getTraceMapIndex())
            m_pathOwner->setUsePathTopologySmoothing(m_int76, true);

        if (m_points.size() != 0)
        {
            const NavigationPoint& last = m_points[m_points.size() - 1];
            m_searchStart = last.m_position;
            m_searchStartPolygon = last.m_polygon;
        }
        else
        {
            m_searchStart = m_start;
            m_searchStartPolygon = m_startPolygon;
        }

        m_allowedClusters.clear();
        m_allowedClusters.insert(m_quadTree->getPolygon(m_searchStartPolygon)->m_cluster);

        m_searchReachesGoal = false;
        if (!m_clusterPath->getPortal(m_searchGoal, m_searchGoalPolygon,
                                      static_cast<unsigned int>(m_clusterPath->m_i0 + 2)))
        {
            m_searchGoal = m_goal;
            m_searchGoalPolygon = m_goalPolygon;
            m_searchReachesGoal = true;
        }

        for (int i = 0; i <= 2; ++i)
        {
            if (static_cast<unsigned int>(m_clusterPath->m_i0 + i) > m_clusterPath->m_nodes.size())
                break;
            m_allowedClusters.insert(m_clusterPath->getCluster(i));
        }
        m_allowedClusters.insert(m_quadTree->getPolygon(m_searchGoalPolygon)->m_cluster);
        m_pathOwner->setAllowedClusters(m_int76, &m_allowedClusters);
    }

    m_state = m_pathOwner->localSearch(m_int76, m_vehicleNumber, m_startIsland, m_searchStartPolygon,
                                       m_searchStart, m_searchGoalPolygon, m_searchGoal, m_equipmentType,
                                       m_polygonPath);
    if (m_state == 1)
    {
        finished = true;
        return true;
    }

    m_pathOwner->releaseResource(m_int76, m_bot->getBotId());
    m_int76 = static_cast<unsigned int>(-1);
    if (m_state == 2)
    {
        calculatePoints();
        if (m_searchReachesGoal)
        {
            const unsigned int index = static_cast<unsigned int>(m_points.size());
            m_points.resize(index + 1);
            m_points[index].m_position = m_goal;
            m_points[index].m_polygon = m_goalPolygon;
            m_clusterPath->m_nodes.resize(0);
            m_clusterPath->m_i0 = 0;
        }

        m_flag192 = false;
        m_quadTree = static_cast<QuadTree*>(m_pathOwner->getQuadTree(m_vehicleNumber, m_polygonPath->m_island));
        optimizePoints();
        updateNavPoints();
        updateNavPoints();
        ++m_clusterPath->m_i0;
        m_flag68 = false;
        return true;
    }
    if (m_state == 3 || m_state == 4)
        return false;
    finished = true;
    return true;
}

// Each node's point takes the node's position and polygon, and the edge between the
// polygon it came from (or the previous node's, when it records none) and its own. A node
// with no previous polygon takes its own position for both edge ends. Retail follows this
// with a loop over the points that has no effect.
void BotNavigation::calculatePoints()
{
    QTPath* const path = m_polygonPath;
    path->m_i0 = 0;
    const unsigned int island = path->m_island;
    const unsigned int count = static_cast<unsigned int>(path->m_nodes.size());

    unsigned int first = static_cast<unsigned int>(m_points.size());
    if (static_cast<unsigned int>(m_int116) < first)
        --first;
    else
        first = 0;

    const unsigned int total = count + first;
    const unsigned int cursor = static_cast<unsigned int>(path->m_i0);
    const unsigned int size = total >= cursor ? total - cursor : 0;

    unsigned int from = static_cast<unsigned int>(-1);
    unsigned int polygon = static_cast<unsigned int>(-1);
    if (path->m_i0 < static_cast<int>(path->m_nodes.size()))
    {
        from = path->m_nodes[path->m_i0].second;
        polygon = path->m_nodes[path->m_i0].first;
    }

    m_points.resize(size);

    for (unsigned int i = first; static_cast<unsigned int>(path->m_i0) < count; ++i)
    {
        NavigationPoint& point = m_points[i];
        const unsigned int node = static_cast<unsigned int>(path->m_i0);
        point.m_position = path->m_nodes[node].third;
        point.m_polygon = path->m_nodes[node].first;
        if (from != static_cast<unsigned int>(-1))
        {
            m_pathOwner->getEdge(point.m_vec0, point.m_vec12, m_vehicleNumber, from, polygon, island);
        }
        else
        {
            point.m_vec0 = path->m_nodes[node].third;
            point.m_vec12 = path->m_nodes[node].third;
        }

        ++path->m_i0;
        const int next = path->m_i0;
        if (next < static_cast<int>(path->m_nodes.size())
            && path->m_nodes[next].second != static_cast<unsigned int>(-1))
            from = path->m_nodes[next].second;
        else
            from = polygon;
        polygon = next < static_cast<int>(path->m_nodes.size()) ? path->m_nodes[next].first
                                                                : static_cast<unsigned int>(-1);
    }

    path->m_nodes.resize(0);
    path->m_i0 = 0;
    path->m_cost = 0.0f;
}

// The equipment is not null-checked. The margin is 0.8 of the equipment's smallest radius
// (at least 1), and at most half the edge.
void BotNavigation::optimizePoints()
{
    if (m_points.size() == 0)
        return;

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    Vec2 previous = flatOf(info->getPosition());

    for (int pass = 2; pass != 0; --pass)
    {
        for (unsigned int i = 0; i < m_points.size() - 1; ++i)
        {
            const Vec3& position = info->getPosition();
            const NavigationPoint& here = m_points[i];
            const float dx = position.x - here.m_position.x;
            const float dy = position.y - here.m_position.y;
            const float dz = position.z - here.m_position.z;
            if (!(dx * dx + dy * dy + dz * dz < 25.0f))
            {
                const Vec2 next = flatOf(m_points[i + 1].m_position);
                const Vec2 edgeStart = flatOf(here.m_vec0);
                const Vec2 edgeEnd = flatOf(here.m_vec12);
                float along;
                float fraction;
                if (intersectLines(previous, next, edgeStart, edgeEnd, along, fraction))
                {
                    const float ex = edgeEnd.x - edgeStart.x;
                    const float ey = edgeEnd.y - edgeStart.y;
                    const float length = std::sqrt(ex * ex + ey * ey);
                    const float distance = length * fraction;

                    float margin = m_smallestRadius * 0.8f;
                    if (!(1.0f < margin))
                        margin = 1.0f;
                    const float half = length * 0.5f;
                    const float clamp = margin < half ? margin : half;

                    if (distance < half)
                    {
                        if (distance < clamp)
                            fraction = clamp / length;
                    }
                    else if (distance > length - clamp)
                    {
                        fraction = 1.0f - clamp / length;
                    }

                    placeOnEdge(m_points[i], 1.0f < fraction ? 1.0f : fraction);
                }
            }
            previous = flatOf(m_points[i].m_position);
        }
    }

    previous = flatOf(info->getPosition());
    for (unsigned int i = 0; i < m_points.size() - 1; ++i)
    {
        const Vec2 next = flatOf(m_points[i + 1].m_position);
        const Vec2 edgeStart = flatOf(m_points[i].m_vec0);
        const Vec2 edgeEnd = flatOf(m_points[i].m_vec12);
        float along;
        float fraction;
        if (intersectLines(previous, next, edgeStart, edgeEnd, along, fraction) && !(fraction < 0.0f)
            && fraction <= 1.0f)
            placeOnEdge(m_points[i], fraction);
        previous = flatOf(m_points[i].m_position);
    }
}

// Ground vehicles (object type bit 21) look one point ahead within 10, tracing at most 8
// polygons with 0.5 clearance; anything else up to four points ahead within 20, 30 polygons
// with s_navPointClearance. A skipped point leaves m_int240 to the stuck restart.
void BotNavigation::updateNavPoints()
{
    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    if (!info)
        return;
    IPIMobile* const mobile = info->getMobile();
    if (!mobile || m_state != 2)
        return;
    if (mobile->getLastValidPosition().polygon == static_cast<unsigned int>(-1))
        return;

    Vec3 hit;
    hit.x = hit.y = hit.z = 0.0f;
    const Vec3& position = info->getPosition();
    const bool ground = m_flag236;
    const float range = ground ? 100.0f : 400.0f;
    const int maxPolygons = ground ? 8 : 30;
    const int lookAhead = ground ? 1 : 4;
    const float clearance = ground ? 0.5f : s_navPointClearance;
    bool advanced = false;

    if (m_int240 == 0)
    {
        const Vec3& point = m_points[m_int116].m_position;
        const float dx = position.x - point.x;
        const float dy = position.y - point.y;
        const float dz = position.z - point.z;
        if (dx * dx + dy * dy + dz * dz < range)
        {
            for (int k = 0; k < lookAhead; ++k)
            {
                if (static_cast<unsigned int>(m_int116) >= m_points.size() - 1)
                    break;
                // Retail reuses the range's stack slot for this out-parameter; the range is
                // not read again.
                unsigned int polygon;
                if (!m_pathOwner->trace(this, false, m_points[m_int116 + 1].m_position, hit, polygon,
                                        m_equipmentType, maxPolygons, clearance, 5, 0))
                    break;
                ++m_int116;
                advanced = true;
                if (k + 1 >= lookAhead)
                    return;
            }
            if (advanced)
                return;
        }
    }

    unsigned int polygon;
    if (!m_pathOwner->trace(this, false, m_points[m_int116].m_position, hit, polygon, m_equipmentType, -1,
                            0.0f, 0, 0))
        m_flag194 = true;
}

// Neither the equipment nor its mobile part is null-checked.
bool BotNavigation::stillOnPath()
{
    if (m_points.size() < 2)
        return false;

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const unsigned int polygon = info->getMobile()->getLastValidPosition().polygon;
    if (polygon == static_cast<unsigned int>(-1))
        return false;

    const unsigned int current = static_cast<unsigned int>(m_int116);
    if (current >= m_points.size())
        return false;

    unsigned int previous = static_cast<unsigned int>(-1);
    if (current > 0)
        previous = m_points[current - 1].m_polygon;
    if (polygon == previous || polygon == m_points[current].m_polygon)
        return true;

    for (int i = m_int116; i >= 0; --i)
    {
        if (m_points[i].m_polygon == polygon)
            return true;
    }

    unsigned int count = static_cast<unsigned int>(m_points.size()) - current;
    if (count >= 20)
        count = 20;
    const unsigned int end = current + count;
    for (unsigned int i = current; i < end; ++i)
    {
        if (m_points[i].m_polygon == polygon)
        {
            const int skipped = m_int116 - static_cast<int>(i) + m_int240;
            m_int116 = static_cast<int>(i);
            m_int240 = skipped <= 0 ? 0 : skipped;
            return true;
        }
    }
    return false;
}

// The equipment is not null-checked. The first pass looks for the bot's own cluster and the
// second for a neighbour of it; both leave the cluster path's cursor on the match and neither
// examines the last entry.
void BotNavigation::resumeClusterPath()
{
    ClusterNodePath* const path = m_clusterPath;
    const unsigned int count = static_cast<unsigned int>(path->m_nodes.size());
    if (count > 1)
    {
        path->m_i0 = 0;
        m_points.resize(0);

        Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
        const IPIMobile::ValidPos& valid = info->getMobile()->getLastValidPosition();
        m_startIsland = valid.quadTree;
        m_startPolygon = valid.polygon;
        m_start = valid.position;
        m_flag195 = true;

        const int cluster = m_pathOwner->getClusterOfPolygon(m_vehicleNumber, m_startIsland, m_startPolygon);
        const unsigned int last = count - 1;

        for (unsigned int i = 0; i < last; ++i)
        {
            const unsigned short node = m_clusterPath->getCluster(0);
            if (static_cast<unsigned short>(cluster) == node)
                return;
            ++m_clusterPath->m_i0;
        }

        m_clusterPath->m_i0 = 0;
        for (unsigned int i = 0; i < last; ++i)
        {
            const unsigned short node = m_clusterPath->getCluster(0);
            if (m_pathOwner->areNeighbouringClusters(m_vehicleNumber, m_startIsland,
                                                     static_cast<unsigned short>(cluster), node))
                return;
            ++m_clusterPath->m_i0;
        }
    }

    m_flag195 = false;
    reset();
}

// The equipment is not null-checked. A portal that cannot be resolved leaves the previous
// portal position in place and so adds its distance again.
void BotNavigation::updateDistanceLeft()
{
    if (m_points.size() == 0)
    {
        m_sentinel188 = 0.0f;
        return;
    }

    Information* const info = informationManager->getInformation(m_bot->getCurrentEquipment());
    const Vec3& position = info->getPosition();
    const NavigationPoint& current = m_points[m_int116];
    float dx = position.x - current.m_position.x;
    float dy = position.y - current.m_position.y;
    float dz = position.z - current.m_position.z;
    float distance = std::sqrt(dz * dz + dy * dy + dx * dx);

    unsigned int i = static_cast<unsigned int>(m_int116) + 1;
    for (; i < m_points.size(); ++i)
    {
        dx = m_points[i - 1].m_position.x - m_points[i].m_position.x;
        dy = m_points[i - 1].m_position.y - m_points[i].m_position.y;
        dz = m_points[i - 1].m_position.z - m_points[i].m_position.z;
        distance += std::sqrt(dz * dz + dy * dy + dx * dx);
    }
    m_pathLength = distance;

    const unsigned int clusters = static_cast<unsigned int>(m_clusterPath->m_nodes.size());
    const unsigned int cursor = static_cast<unsigned int>(m_clusterPath->m_i0);
    if (clusters > cursor)
    {
        Vec3 previous = m_points[i - 1].m_position;
        const unsigned int remaining = clusters - cursor;
        Vec3 portal;
        portal.x = portal.y = portal.z = 0.0f;
        for (unsigned int j = 1; j < remaining; ++j)
        {
            unsigned int value;
            m_clusterPath->getPortal(portal, value, j);
            dx = previous.x - portal.x;
            dy = previous.y - portal.y;
            dz = previous.z - portal.z;
            previous = portal;
            distance += std::sqrt(dz * dz + dy * dy + dx * dx);
        }
    }
    m_sentinel188 = distance;
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

} // namespace ai
} // namespace hfe
} // namespace dice
