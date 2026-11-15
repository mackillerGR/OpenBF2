// What a bot is moving towards: a single position or a list of positions.
// [INFERRED] placement and every name: the image carries no __FILE__ string or RTTI for these
// classes. Offsets in the comments are the retail PE (x86, MSVC) layout.

#ifndef AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H
#define AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H

#include "../../../../Math/Vec3.h"

#include <list>

namespace dice {
namespace hfe {
namespace ai {

class Bot;

// PE vtable 1027F910; seven slots.
class WaypointList
{
public:
    explicit WaypointList(int kind);
    virtual ~WaypointList();

    // Advances along the list from the bot's position; returns a distance. The second argument
    // is the bot: the list bodies make Bot virtual calls on it (PE 1012D290, 1012C8C0).
    virtual float update(const Vec3& position, Bot* bot) = 0;
    // The waypoint the bot is heading for, false when there is none.
    virtual bool getCurrentWaypoint(Vec3& waypoint) const = 0;
    // 8 for a single position, 6 for a list.
    virtual int getType() const = 0;
    virtual void setPausedFlag(bool paused);                         // PE 1012C460
    virtual bool isPaused() const;                                   // PE 1012C470
    // The base reports zero; the list reports its first tolerance. [INFERRED] name.
    virtual float getTolerance() const;                              // PE 1012C480

protected:
    /* +0x04 */ bool  m_active;      // = true
    /* +0x05 */ bool  m_finished;    // = false
    /* +0x06 */ bool  m_paused;      // = false
    /* +0x08 */ float m_float08;     // = -1
    /* +0x0C */ int   m_kind;        // 1 single, 4 list
};

// PE vtable 102851A8, constructor 101DABC0; 0x1C bytes.
class SinglePositionWaypoint : public WaypointList
{
public:
    explicit SinglePositionWaypoint(const Vec3& position);
    virtual ~SinglePositionWaypoint();                               // PE 101DAC10
    // Stores the position and reports no distance left. PE 101DAC40.
    virtual float update(const Vec3& position, Bot* bot);
    virtual bool getCurrentWaypoint(Vec3& waypoint) const;           // PE 101DAC70
    virtual int getType() const;                                     // PE 101DAC00

private:
    /* +0x10 */ Vec3 m_position;
};

// PE vtable 1027F968, constructor 1012D4A0; 0x30 bytes.
class MultiPositionWaypoint : public WaypointList
{
public:
    MultiPositionWaypoint(float toleranceA, float toleranceB, float distance, int mode,
                          const std::list<Vec3>& waypoints);
    virtual ~MultiPositionWaypoint();                                // PE 1012D240
    virtual float update(const Vec3& position, Bot* bot);       // PE 1012D290
    virtual bool getCurrentWaypoint(Vec3& waypoint) const;           // PE 1012D110
    virtual int getType() const;                                     // PE 1012D100
    virtual float getTolerance() const;                              // PE 1012CF00

private:
    /* +0x10 */ float           m_toleranceA;
    /* +0x14 */ float           m_toleranceB;
    /* +0x18 */ float           m_float18;     // = 1e9
    /* +0x1C */ float           m_distance;
    /* +0x20 */ int             m_mode;        // 1 or 2; stored as a full word
    /* +0x24 */ std::list<Vec3> m_waypoints;
};

// A list of points, each tied to a strategic object (or -1), that a moving squad member
// follows. [INFERRED] name. PE vtable 1027F92C, constructor 1012CDF0; 0x2C bytes.
class StrategicWaypointList : public WaypointList
{
public:
    struct Point
    {
        Vec3 position;
        unsigned int strategicObject;   // index for AIStrategicObjectManager, or -1
    };

    StrategicWaypointList(float tolerance, float radius, int mode, const std::list<Point>& points);
    virtual ~StrategicWaypointList();                                // PE 1012C870
    // Steps to the next point once the bot is close enough or inside the point's object, and
    // returns the speed to keep: lower near a point, 2 toward an object the side holds.
    virtual float update(const Vec3& position, Bot* bot);            // PE 1012C8C0
    virtual bool getCurrentWaypoint(Vec3& waypoint) const;           // PE 1012C780
    virtual int getType() const;                                     // PE 1012C830
    virtual float getTolerance() const;                              // PE 1012C540

private:
    /* +0x10 */ float            m_tolerance;
    /* +0x14 */ float            m_radius;     // at least 1
    /* +0x18 */ int              m_mode;       // 1: stop at the point; otherwise pass through
    /* +0x1C */ std::list<Point> m_points;
    /* +0x28 */ bool             m_flag28;     // = false [UNRESOLVED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_WAYPOINTLIST_H
