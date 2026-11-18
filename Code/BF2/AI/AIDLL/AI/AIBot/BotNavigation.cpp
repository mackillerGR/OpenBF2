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
