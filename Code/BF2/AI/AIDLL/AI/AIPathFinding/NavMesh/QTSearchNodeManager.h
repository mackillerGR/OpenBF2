// Node pool and expansion rules for navigation-mesh A* searches: it makes the successor nodes of
// a polygon, prices the steps and remembers which polygons are open or closed.
// [RETIRE-WHEN: aidll-02-skeleton] Recover the destructor and validate the node-pool lifecycle.

#ifndef AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H

#include <set>
#include <vector>

#include "../../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class MaterialManager;
class QTPath;
class QTSearchNodeManager;
class QuadTree;

// One search node: a polygon reached through an edge. 44 bytes. A spare node is chained through
// m_next. Field names are [INFERRED] from their readers.
class QTSearchNode
{
public:
    // PE 10176A60.
    QTSearchNode(int key, int depth)
        : m_next(0), m_prev(0), m_parent(0), m_cameFrom(0), m_key(key), m_pathCost(0.0f), m_cost(0.0f),
          m_depth(depth)
    {
        m_position.x = m_position.y = m_position.z = 0.0f;
    }

    // PE 10176AA0: the key and position of another node; everything else cleared.
    explicit QTSearchNode(const QTSearchNode& other)
        : m_next(0), m_prev(0), m_parent(0), m_cameFrom(0), m_key(other.m_key), m_pathCost(0.0f),
          m_cost(0.0f), m_depth(0), m_position(other.m_position)
    {
    }

    // The successor kinds getSuccessor reports.
    enum SuccessorKind { New = 0, AlreadyOpen = 1, AlreadyClosed = 2, Goal = 3, Skip = 4 };

    // The first successor and the next one; `edge` carries the position between calls.
    // PE 10176A10 / 10176A40.
    QTSearchNode* getFirstSuccessor(QTSearchNodeManager* manager, int& kind, unsigned short& edge);
    QTSearchNode* getNextSuccessor(QTSearchNodeManager* manager, int& kind, unsigned short& edge);

    /* +0x00 */ QTSearchNode* m_next;      // hash chain, or the spare list
    /* +0x04 */ QTSearchNode* m_prev;
    /* +0x08 */ QTSearchNode* m_parent;
    /* +0x0C */ QTSearchNode* m_cameFrom;  // written with the parent; the path reads its key
    /* +0x10 */ int m_key;                 // the polygon
    /* +0x14 */ float m_pathCost;          // cost from the start
    /* +0x18 */ float m_cost;              // path cost plus the distance left
    /* +0x1C */ int m_depth;
    /* +0x20 */ Vec3 m_position;           // the midpoint of the edge crossed
};

class QTSearchNodeManager
{
public:
    // Whether a polygon's node is open or closed in the running search. 12 bytes.
    struct State
    {
        bool m_open;
        bool m_closed;
        QTSearchNode* m_node;
        State* m_next;
    };

    // A bucket of the state table: its link in the list of used buckets and its chain. The
    // constructor zeroes both words.
    class CacheEntry
    {
    public:
        CacheEntry()
            : m_next(0)
            , m_head(0)
        {
        }

        CacheEntry* m_next;
        State* m_head;
    };

    // the same prime the search resource's two hashes use. Written out as a literal in the binary.
    enum { kCacheEntries = 1031 };

    QTSearchNodeManager();
    explicit QTSearchNodeManager(QuadTree* quadTree);
    ~QTSearchNodeManager();                      // [FRONTIER]

    void setQuadTree(QuadTree* quadTree) { m_quadTree = quadTree; }          // PE 1016EDC0
    // A node for the polygon, from the spare list when it has one; a recycled node keeps its
    // old costs and position. PE 1016EDD0.
    QTSearchNode* allocNode(int key, QTSearchNode* parent, int depth);
    // Returns a node to the spare list. Inlined at every PE use.
    void freeNode(QTSearchNode* node) { node->m_next = m_nodeCache; m_nodeCache = node; }
    // Takes the goal's polygon and position and frees the node. PE 1016F5C0.
    void setGoal(QTSearchNode* goal);
    // Ends a search: frees every node the state table knows and the goal. PE 1016EE40.
    void reset();
    // Drops the per-search options the owner set. PE 1016EEC0.
    void returnReset() { m_usePathTopologySmoothing = false; m_additionalGoals = 0; }
    // PE 1016EF80 / 1016F030. markClosed does not check that the polygon has a state.
    void markOpen(QTSearchNode* node);
    void markClosed(QTSearchNode* node);
    // The successor of `node` across the first usable edge from `edge` on, with its kind; null
    // with kind Skip when the edges are used up. PE 1016FBD0.
    QTSearchNode* getSuccessor(QTSearchNode* node, unsigned short& edge, int& kind);
    // Re-parents `existing` under `node` when that is cheaper by more than 0.5. PE 1016F610.
    bool relax(QTSearchNode* node, QTSearchNode* existing, int maxDepth);
    // Writes the polygons from the start to `goal`, at most 201 of them, into the path.
    // PE 10170030.
    void buildPath(QTPath* path, QTSearchNode* goal);

    /*  +0 */ QuadTree* m_quadTree;
    /*  +4 */ QTSearchNode* m_goal;      // = 0
    /*  +8 */ QTSearchNode* m_nodeCache; // = 0; the spare nodes
    /* +12 */ int m_allocatedNodes;      // = 0
    /* +16 */ State* m_stateCache;       // = 0; the spare states
    /* +20 */ CacheEntry* m_usedEntries; // = 0; the buckets in use
    /* +24 */ CacheEntry* m_table;       // new CacheEntry[kCacheEntries]
    /* +28 */ std::vector<QTSearchNode*> m_pathNodes;   // buildPath's scratch
    /* +40 */ const std::set<unsigned int>* m_additionalGoals;     // = 0
    /* +44 */ const std::set<unsigned short>* m_allowedClusters;   // = 0
    /* +48 */ bool m_usePathTopologySmoothing;                     // = false
    /* +52 */ MaterialManager* m_materialManager;                  // = 0

private:
    QTSearchNode* cloneNode(const QTSearchNode* node);                       // PE 1016F3F0
    State* findState(int key) const;
    Vec3 getEdgeMidpoint(unsigned char edge, const QTSearchNode* node) const;   // PE 1016F2B0
    // The cost of going from one polygon's position to another's; -1 across an impassable
    // material. PE 1016F0D0.
    float getStepCost(int fromKey, const Vec3& from, int toKey, const Vec3& to) const;
    float getPathCost(const QTSearchNode* from, const QTSearchNode* to, const Vec3& position) const;  // PE 1016F480
    float getTotalCost(float pathCost, const Vec3& position) const;          // PE 1016F250
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_NAVMESH_QTSEARCHNODEMANAGER_H
