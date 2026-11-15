#include "WaypointList.h"
#include <cmath>
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "BotMain.h"

#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

WaypointList::WaypointList(int kind)
    : m_active(true)
    , m_finished(false)
    , m_paused(false)
    , m_float08(-1.0f)
    , m_kind(kind)
{
}

WaypointList::~WaypointList()
{
}

void WaypointList::setPausedFlag(bool paused)
{
    m_paused = paused;
}

bool WaypointList::isPaused() const
{
    return m_paused;
}

float WaypointList::getTolerance() const
{
    return 0.0f;
}

SinglePositionWaypoint::SinglePositionWaypoint(const Vec3& position)
    : WaypointList(1)
    , m_position(position)
{
}

SinglePositionWaypoint::~SinglePositionWaypoint()
{
}

float SinglePositionWaypoint::update(const Vec3& position, Bot*)
{
    m_active = true;
    m_position = position;
    return 0.0f;
}

bool SinglePositionWaypoint::getCurrentWaypoint(Vec3& waypoint) const
{
    waypoint = m_position;
    return true;
}

int SinglePositionWaypoint::getType() const
{
    return 8;
}

// The list is copied after it is cleared.
MultiPositionWaypoint::MultiPositionWaypoint(float toleranceA, float toleranceB, float distance,
                                             int mode, const std::list<Vec3>& waypoints)
    : WaypointList(4)
    , m_toleranceA(toleranceA)
    , m_toleranceB(toleranceB)
    , m_float18(1.0e9f)
    , m_distance(distance)
    , m_mode(mode)
{
    m_waypoints.clear();
    m_waypoints.insert(m_waypoints.end(), waypoints.begin(), waypoints.end());
}

MultiPositionWaypoint::~MultiPositionWaypoint()
{
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-waypoints] Replace with the PE
// 1012D290 body once a bot follows a waypoint list.
float MultiPositionWaypoint::update(const Vec3&, Bot*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::MultiPositionWaypoint::update");
}

bool MultiPositionWaypoint::getCurrentWaypoint(Vec3& waypoint) const
{
    if (m_waypoints.empty())
        return false;
    waypoint = m_waypoints.front();
    return true;
}

int MultiPositionWaypoint::getType() const
{
    return 6;
}

float MultiPositionWaypoint::getTolerance() const
{
    return m_toleranceA;
}

// PE 1012CDF0. The list is copied after it is cleared.
StrategicWaypointList::StrategicWaypointList(float tolerance, float radius, int mode,
                                             const std::list<Point>& points)
    : WaypointList(0)
    , m_tolerance(tolerance)
    , m_radius(1.0f < radius ? radius : 1.0f)
    , m_mode(mode)
    , m_flag28(false)
{
    m_points.clear();
    m_points.insert(m_points.end(), points.begin(), points.end());
}

StrategicWaypointList::~StrategicWaypointList()
{
}

namespace {

// max(a, b) as the x87 code keeps it: b when a < b, otherwise a.
float keepLarger(float a, float b)
{
    return a < b ? b : a;
}

} // anonymous namespace

// PE 1012C8C0. The equipment is not null-checked.
float StrategicWaypointList::update(const Vec3& position, Bot* bot)
{
    if (m_points.empty())
    {
        m_finished = true;
        return 0.0f;
    }
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIPhysical* const physical = equipment->getPhysical();
    Point& front = m_points.front();
    const float dx = position.x - front.position.x;
    const float dy = position.y - front.position.y;
    const float dz = position.z - front.position.z;
    float distanceSquared = dx * dx + dy * dy + dz * dz;

    if (m_mode == 1)
    {
        const float tolerance = static_cast<float>(std::sqrt(static_cast<double>(m_tolerance)));
        const float limit = keepLarger(tolerance, physical->getSmallestRadius());
        const float limitSquared = limit * limit;
        if (distanceSquared < limitSquared)
        {
            m_finished = true;
            m_paused = false;
            return 0.0f;
        }
        float speed;
        AIStrategicObject* const object =
            AIStrategicObjectManager::getInstance()->getStrategicObject(front.strategicObject);
        if (object)
        {
            const int side = bot->getSide();
            if (object->getStatus(side) != 0)
            {
                speed = 2.0f;
            }
            else if (static_cast<AIStrategicArea*>(object)->isInsideArea(position))
            {
                speed = 0.0f;
                distanceSquared = 0.0f;
            }
            else
            {
                const float radius = object->m_size[side];
                distanceSquared -= radius * radius;
                speed = 1.0f;
            }
            if (m_paused)
            {
                object->getRandomPosition(bot->getSide(), 0.9f, front.position);
                m_paused = false;
            }
        }
        else
        {
            speed = m_paused ? 0.0f : 1.0f;
        }
        const float slowdown = limitSquared * 1.5f;
        m_finished = distanceSquared < slowdown;
        const float ratio = distanceSquared / slowdown;
        if (!(1.0f < ratio))
            return speed * (ratio < 0.1f ? 0.1f : ratio);
        return speed * 1.0f;
    }

    const float radius = static_cast<float>(std::sqrt(static_cast<double>(m_radius)));
    float limit = keepLarger(radius, physical->getSmallestRadius());
    if (front.strategicObject == static_cast<unsigned int>(-1))
        limit = keepLarger(10.0f, physical->getSmallestRadius());
    bool advance = distanceSquared < limit * limit;
    if (!advance)
    {
        AIStrategicObject* const object =
            AIStrategicObjectManager::getInstance()->getStrategicObject(front.strategicObject);
        if (object)
        {
            if (static_cast<AIStrategicArea*>(object)->isInsideArea(position))
            {
                advance = true;
            }
            else
            {
                if (m_paused)
                {
                    object->getRandomPosition(bot->getSide(), 0.9f, front.position);
                    m_paused = false;
                }
                return 1.0f;
            }
        }
        else if (m_paused)
        {
            advance = true;
        }
        else
        {
            return 1.0f;
        }
    }
    m_points.pop_front();
    m_active = true;
    m_paused = false;
    return 1.0f;
}

bool StrategicWaypointList::getCurrentWaypoint(Vec3& waypoint) const
{
    if (m_points.empty())
        return false;
    waypoint = m_points.front().position;
    return true;
}

int StrategicWaypointList::getType() const
{
    return 1;
}

float StrategicWaypointList::getTolerance() const
{
    return m_tolerance;
}

} // namespace ai
} // namespace hfe
} // namespace dice
