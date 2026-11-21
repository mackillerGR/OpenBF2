// Per-bot navigation state and owned paths. The bot owner is borrowed.

#ifndef AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H

#include <set>
#include <vector>

#include "../Main/AIPathfindingSearchTypes.h"   // QTPath, ClusterNodePath - both already written
#include "../../../../Math/Vec3.h"        // Vec2 and Vec3 both live here, and both are typedefs
                                          // to dice::hfe::Math::* - never forward-declare either

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class IAIPathfinding;
class QuadTree;
class Information;

class BotNavigation
{
public:
    // The obstacles gathered by updateObstacles. [INFERRED] accessor; read inline in PE.
    const std::vector<unsigned int>& getObstacles() const { return m_obstacles; }
    // Whether the position lies within the smallest radius of a gathered obstacle that is not a
    // soldier. PE 101742E0; [INFERRED] name.
    bool isInsideObstacle(const Vec3& position) const;
    // The avoid position and the time left (2.8 s from when it was set); true while time is
    // left. [INFERRED] name. PE 101729D0.
    bool getAvoidPosition(Vec3& position, double& timeLeft) const;
    float getFloat292() const { return m_float292; }   // [INFERRED] accessors
    bool getFlag276() const { return m_flag276; }
    float getFloat296() const { return m_float296; }
    explicit BotNavigation(BotMain* owner);
    ~BotNavigation();                           // [FRONTIER]

    // [FRONTIER] Navigation methods below remain incomplete unless implemented explicitly.
    // Drives the cluster search, then the polygon search, one step per call. `finished` is
    // cleared and set when a step completes or stays in progress; `busy`, when given, is set
    // when no search resource was free. False without a path owner, in state 4 or when a
    // search fails. PE 10176520.
    bool update(bool& finished, bool* busy);
    // The periodic upkeep BotManager runs for recently steered bots: skip ahead, refresh the
    // distance left, reset an illegal path or flag one with no progress, and for ground
    // vehicles measure the room to either side. PE 101756F0. Dice's spelling - "Occationally".
    void updateOccationally();
    // Advances along the path and hands out the horizontal position to steer for. `arrived`
    // is set on reaching the last point, `waiting` while a search or a firing stop holds the
    // bot. False when no point can be given. PE 10175360.
    bool updatePath(Vec2& target, bool& arrived, bool& waiting, float distance);
    // On path state 2, skips the cursor ahead past points the equipment can reach directly;
    // when none can be skipped and the current point cannot be traced to either, marks the bot
    // as off its path (+194). PE 101733E0.
    void updateNavPoints();
    void updateObstacles();
    // Once the cluster path is used up and the bot is on its fine path, moves the last point to
    // the object's last valid position for the bot's vehicle, when that is on the path's island.
    // PE 101732E0.
    void updateLastPoint(Information* info);
    // Sets the pending-search flag, which stays clear while the bot's player is on a ladder.
    // PE 10172260; [INFERRED] name.
    void setPathSearchPending(bool pending);
    // The island of the equipment's last valid infantry position, as of the last request.
    int getInfantryIsland() const { return m_int212; }
    // Traces 2 to the right and to the left of the equipment's last valid position (ground
    // vehicles); a clear side stores 2, a blocked one the distance to the hit. PE 101725B0.
    void updateSpaceLeftAndRight();
    void updateClosestDistance();

    // Requests a path to a position: finds the polygon under it on the equipment's map and
    // forwards. False when the bot has no equipment or the position is off the mesh. PE 10176460.
    bool initPathfinding(const Vec3& to, float a, float b);
    // Requests a path to a position on a known island and polygon. Keeps the current path when
    // the request repeats and the bot is still on it; otherwise restarts navigation, tries a
    // straight line and marks the search as new. False when either end is off the mesh, the
    // equipment is over 100 from its last valid position or on another island. PE 10175BD0.
    bool initPathfinding(unsigned int island, unsigned int polygon, const Vec3& to, float a, float b);
    // One step of the polygon search towards the next cluster portal (or the goal). On success
    // the polygon path becomes points and the cluster cursor advances. PE 10175FC0.
    bool doPolygonSearch(bool& finished, bool* busy);
    // One step of the cluster search; on success the polygon search starts. A failed search
    // resets navigation. PE 10175260.
    bool doClusterSearch(bool& finished, bool* busy);
    // Appends the polygon path's nodes as points (replacing the last point when the cursor is
    // on the path) and empties the polygon path. PE 10175740.
    void calculatePoints();
    // Pulls each point along its edge onto the straight line between its neighbours, twice,
    // keeping a margin from the edge ends; then once more without the margin. PE 101735B0.
    void optimizePoints();
    // Within 200 of the goal, traces a straight line to it; a clear line becomes a one-point
    // path and state 2. PE 10174F10; no value is returned there.
    void testDirectPath();
    void retrace();

    // Whether the equipment's last valid polygon is the previous or current point's, an earlier
    // point's, or one of the next 20 (which then becomes current). PE 101739B0.
    bool stillOnPath();
    // Whether a point's polygon is missing or has material 0. PE 10173380.
    bool hasIllegalPath();
    // [INFERRED] accessor name for the +192 byte, which retail reads inline (BotManager::
    // actionComAgent). initPathfinding and updateOccationally set it; clearProgress
    // checkProgress, doPolygonSearch and setIsStuck clear it; updatePath tests it.
    bool isPathSearchPending() const { return m_flag192; }
    // [INFERRED] accessor names for the +193 and + (PE +0xC9 / +0x148), both read inline
    // by BotManager::actionPathfinding (, ).
    // +193: set to 1 only on initPathfinding's search-started path , cleared by the
    // constructor, initPathfinding's other exits and reset. +316: set by initPathfinding
    // cleared by update and reset.
    bool isSearchActive() const { return m_flag193; }
    // The goal of the current path request. [INFERRED] accessor name.
    const Vec3& getGoal() const { return m_goal; }
    // The distance left along the path, as updatePath and updateDistanceLeft keep it.
    // [INFERRED] accessor name.
    float getDistanceLeft() const { return m_sentinel188; }
    bool isSearchNew() const { return m_flag316; }
    bool isActive();
    // returns true, unconditionally, in both images.
    bool isRunning();
    // Whether the bot is still making progress along a followed path (state 2): moving the
    // controls or getting more than 2 (ground vehicles) or 4 from where progress was last seen
    // restarts the clock; false after 10 (ground vehicles) or 20 seconds without. Also releases
    // the action input a second after it was pressed and, for a ground vehicle stuck in place
    // for 2 seconds, arms another press. PE 10172CC0.
    bool checkProgress();
    void clearProgress();
    // Whether the equipment is within 3 vertically and the arrival radius horizontally of the
    // current point. The argument is not read. PE 10173FC0.
    bool checkArrivedAtPoint(float distance);
    void setIsStuck(bool stuck);

    bool pointWithinObstacleRadius(const Vec3& point);
    void getFirstPointBeyondDist(const Vec2& from, float distance, Vec2& out);
    // The horizontal position of the point after the current one; false at the end of the
    // path. PE 10173B30.
    bool getSecondPoint(Vec2& out);
    // The largest absolute angle between the equipment's facing and the path segments within
    // reach (twice its speed, at least 3), recomputed at most every half second. Always true.
    // PE 10173B80.
    bool getLargestAngleDiff(float& out);
    // The equipment's last valid polygon, island and position on a map, cached per position.
    // Only the bot's own map and the trace map are supported. PE 101727B0.
    void getTraceData(int map, unsigned int& polygon, unsigned int& island, Vec3& position);

    void getGrenadeWarning(Vec3& position, double& time);
    void setGrenadeWarning(const Vec3& position, double time, bool b);

    void reset();                          // - (one arm frontier)
    void resetAll();

    // attested as a nested type by the container instantiations, which spell
    // `std::vector<dice::hfe::ai::BotNavigation::NavigationPoint>` in full. sizeof is 40.
    struct NavigationPoint
    {
        NavigationPoint()
            : m_polygon(static_cast<unsigned int>(-1))
        {
            m_vec0.x = m_vec0.y = m_vec0.z = 0.0f;
            m_vec12.x = m_vec12.y = m_vec12.z = 0.0f;
            m_position.x = m_position.y = m_position.z = 0.0f;
        }

        /*  +0 */ Vec3 m_vec0;              // [UNRESOLVED] role; testDirectPath stores the goal
        /* +12 */ Vec3 m_vec12;             // [UNRESOLVED] role; testDirectPath stores the goal
        /* +24 */ Vec3 m_position;          // [INFERRED] name
        /* +36 */ unsigned int m_polygon;   // = -1 [INFERRED] name
    };

    int getVehicleNumber() const { return m_vehicleNumber; }

private:
    // Moves the bot back onto its cluster path from its last valid position, or resets when no
    // remaining cluster is the bot's or a neighbour of it. PE 101750B0; [INFERRED] name.
    void resumeClusterPath();
    // Stores the length left along the points and the cluster portals. PE 10173DD0;
    // [INFERRED] name.
    void updateDistanceLeft();

    /*   0 */ BotMain* m_bot;              // the ctor argument, and the first store
    /*   4 */ float m_float4;              // = 2.0f [UNRESOLVED]
    /*   8 */ float m_float8;              // = 2.0f [UNRESOLVED]
    /*  12 */ float m_float8Squared;       // initPathfinding: m_float8 squared
    /*  16 */ unsigned int m_startIsland;  // = -1 [INFERRED] name
    /*  20 */ unsigned int m_startPolygon; // = -1 [INFERRED] name
    /*  24 */ Vec3  m_start;               // [INFERRED] name
    /*  36 */ unsigned int m_goalPolygon;  // = -1 [INFERRED] name
    /*  40 */ unsigned char m_goalMaterial; // = 0 [INFERRED] name
    /*  41 */ char  m_pad41[3];
    /*  44 */ Vec3  m_goal;                // [INFERRED] name
    /*  56 */ int   m_state;               // reset: 0; testDirectPath: 2 [INFERRED] name
    // Owns the polygon and cluster search reservations. Null until the first path request.
    /*  60 */ IAIPathfinding* m_pathOwner;  // = 0
    /*  64 */ QuadTree* m_quadTree;         // = 0; the start island's quad tree
    /*  68 */ bool  m_flag68;               // [UNRESOLVED] role
    /*  69 */ char  m_pad69[3];
    /*  72 */ QTPath* m_polygonPath;       // new(0x18) - the fine path
    /*  76 */ unsigned int m_int76;        // = -1; polygon search resource
    /*  80 */ int   m_vehicleNumber;       // reset: the equipment's vehicle number. [INFERRED] name
    /*  84 */ int   m_equipmentType;       // initPathfinding [INFERRED] name
    // Handles of nearby obstacles, refreshed by updateObstacles (PE +0x58).
    /*  88 */ std::vector<unsigned int> m_obstacles;
    // the walkable result - see the header note. The constructor's dead resize targets this.
    /* 100 */ std::vector<NavigationPoint> m_points;
    /* 112 */ unsigned int m_island112;    // initPathfinding: the start island again
    /* 116 */ int   m_int116;              // = 0
    /* 120 */ bool  m_flag120;             // [UNRESOLVED] role
    /* 121 */ char  m_pad121[3];
    /* 124 */ ClusterNodePath* m_clusterPath;   // new(0x1C) - the coarse path
    /* 128 */ unsigned int m_int128;       // = -1; cluster search resource
    // The clusters the polygon search may enter: its start, the next three on the cluster path
    // and the end polygon's. Handed to IAIPathfinding::setAllowedClusters.
    /* 132 */ std::set<unsigned short> m_allowedClusters;
    /* 144 */ double m_double144;          // = 0.0
    /* 152 */ double m_double152;          // = 0.0
    /* 160 */ double m_double160;          // = 0.0
    /* 168 */ double m_double168;          // = 0.0
    /* 176 */ Vec3  m_actionCheckPosition; // checkProgress: where the last action press was armed
    // is 9,999,999.0f - a "no distance yet" sentinel, and a third kind after the
    // DBL_MAX timestamps and -9999.0 magnitudes BotMain uses. Do not normalise them together.
    /* 188 */ float m_sentinel188;         // = 9999999.0f
    /* 192 */ bool  m_flag192;             // = false
    /* 193 */ bool  m_flag193;             // = false
    /* 194 */ bool  m_flag194;             // not padding - `reset` clears it
    /* 195 */ bool  m_flag195;             // = false
    /* 196 */ char  m_gap196[4];           // [UNRESOLVED] - never written
    /* 200 */ int   m_int200;              // = -1
    /* 204 */ int   m_int204;              // = -1
    /* 208 */ int   m_int208;              // = -1
    /* 212 */ int   m_int212;              // = -1
    // getTraceData's cache: the last valid position handed out and the map it was for.
    /* 216 */ Vec3  m_tracePosition;
    /* 228 */ int   m_traceMap;            // never initialised
    /* 232 */ float m_smallestRadius;      // initPathfinding: the equipment's smallest radius
    /* 236 */ bool  m_flag236;             // initPathfinding: object type bit 21
    /* 237 */ char  m_pad237[3];
    /* 240 */ int   m_int240;              // = 0
    /* 244 */ double m_double244;          // not a gap - `reset` zeroes it
    // resetAll clears this field.
    /* 252 */ int   m_obstacleCount;       // resetAll: 0; updateObstacles writes the size
    // A position the bot should get away from and when it was set; read as a vector by
    // getAvoidPosition. [INFERRED] meaning.
    /* 256 */ int   m_zero256[3];          // +256, +260, +264
    /* 268 */ double m_double268;          // = 0.0
    /* 276 */ bool  m_flag276;             // = false
    /* 277 */ char  m_pad277[3];
    /* 280 */ Vec3  m_progressPosition;    // checkProgress: where progress was last seen
    // Read as floats against 0.5 by BotMain::afterPlanInterpreted to steer around friends in the
    // line of fire (PE +0x130 / +0x134). [UNRESOLVED] what writes them.
    /* 292 */ float m_float292;
    /* 296 */ float m_float296;
    /* 300 */ double m_double300;          // Initially -1.0; independent of the distance sentinel.
    /* 308 */ float m_largestAngleDiff;    // getLargestAngleDiff's cached answer
    /* 312 */ float m_pathLength;          // updateDistanceLeft: length along the points
    // `reset`'s first statement clears the byte here.
    /* 316 */ bool  m_flag316;             // reset: false
    /* 317 */ char  m_pad317[3];
    // The current polygon search's ends: the last point (or the start) and the next portal
    // (or the goal).
    /* 320 */ Vec3  m_searchStart;
    /* 332 */ Vec3  m_searchGoal;
    /* 344 */ unsigned int m_searchStartPolygon;   // not constructed
    /* 348 */ unsigned int m_searchGoalPolygon;    // not constructed
    /* 352 */ bool  m_searchReachesGoal;           // not constructed
    /* 353 */ char  m_pad353[3];
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTNAVIGATION_H
