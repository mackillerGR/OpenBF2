#include "WaypointList.h"

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

float SinglePositionWaypoint::update(const Vec3& position, float)
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
                                             bool flag, const std::list<Vec3>& waypoints)
    : WaypointList(4)
    , m_toleranceA(toleranceA)
    , m_toleranceB(toleranceB)
    , m_float18(1.0e9f)
    , m_distance(distance)
    , m_flag(flag)
{
    m_waypoints.clear();
    m_waypoints.insert(m_waypoints.end(), waypoints.begin(), waypoints.end());
}

MultiPositionWaypoint::~MultiPositionWaypoint()
{
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-waypoints] Replace with the PE
// 1012D290 body once a bot follows a waypoint list.
float MultiPositionWaypoint::update(const Vec3&, float)
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

} // namespace ai
} // namespace hfe
} // namespace dice
