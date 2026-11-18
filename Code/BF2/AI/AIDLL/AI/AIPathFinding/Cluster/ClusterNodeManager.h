// Node pool for cluster-graph A* searches, including an intrusive free list for open/closed records.
// [FRONTIER] The destructor remains unwritten.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the node-pool teardown.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H

#include "../../Main/AIPathfindingSearchTypes.h"      // Hash, ClusterNodePath

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class ClusterGraph;
class ClusterNodeManager;
class ClusterNodePath;
class QuadTree;

// One cluster search node: a cluster entered through a portal. 28 bytes. A spare node is
// chained through m_next. Field names are [INFERRED] from their readers.
class ClusterNode
{
public:
    // PE 10176AE0.
    ClusterNode(unsigned short key, int depth)
        : m_next(0), m_prev(0), m_parent(0), m_pathCost(0.0f), m_cost(0.0f), m_key(key), m_portal(0xFFFF),
          m_depth(depth)
    {
    }

    // PE 10176B10: the key and portal of another node; everything else cleared.
    explicit ClusterNode(const ClusterNode& other)
        : m_next(0), m_prev(0), m_parent(0), m_pathCost(0.0f), m_cost(0.0f), m_key(other.m_key),
          m_portal(other.m_portal), m_depth(0)
    {
    }

    // The successor kinds getSuccessor reports.
    enum SuccessorKind { New = 0, AlreadyOpen = 1, AlreadyClosed = 2, Goal = 3, Skip = 4 };

    // The first successor and the next one; `edge` carries the portal position between calls.
    // PE 10176B60 / 10176B90.
    ClusterNode* getFirstSuccessor(ClusterNodeManager* manager, int& kind, unsigned short& edge);
    ClusterNode* getNextSuccessor(ClusterNodeManager* manager, int& kind, unsigned short& edge);

    /* +0x00 */ ClusterNode* m_next;      // hash chain, or the spare list
    /* +0x04 */ ClusterNode* m_prev;
    /* +0x08 */ ClusterNode* m_parent;
    /* +0x0C */ float m_pathCost;         // cost from the start
    /* +0x10 */ float m_cost;             // path cost plus the distance left
    /* +0x14 */ unsigned short m_key;     // the cluster
    /* +0x16 */ unsigned short m_portal;  // the portal entered through, 0xFFFF at the start
    /* +0x18 */ int m_depth;
};

class ClusterNodeManager
{
public:
    // Whether a cluster's node is open or closed in the running search. 16 bytes. Attested as a
    // nested type by the mangled names of `getNewOCData`, `recycleOCData` and
    // `HashIterator<ClusterNodeManager::OCData, ...>`.
    class OCData
    {
    public:
        bool m_open;
        bool m_closed;
        ClusterNode* m_node;
        OCData* m_next;     // hash chain, or the free list
        OCData* m_prev;
    };

    // the same prime the cluster search resource'S two hashes use. A literal in the binary.
    enum { kOCDataBuckets = 313 };

    ClusterNodeManager();
    ~ClusterNodeManager();       // , [FRONTIER]

    // PE 10170340 / 10170350.
    void setQuadTree(QuadTree* quadTree) { m_quadTree = quadTree; }
    void setClusterGraph(ClusterGraph* graph) { m_clusterGraph = graph; }
    // The search's end positions. PE 1009F9C0.
    void setPositions(const Vec3& start, const Vec3& goal)
    {
        m_start = start;
        m_goal = goal;
    }
    void setCostType(unsigned short costType) { m_costType = costType; }

    // A node for a cluster, reused from the spare list when one is there. A reused node keeps its
    // costs and portal. PE 10170360.
    ClusterNode* allocNode(unsigned short key, ClusterNode* parent, int depth);
    void freeNode(ClusterNode* node)
    {
        node->m_next = m_spareNodes;
        m_spareNodes = node;
    }
    // Copies the goal node into the manager's own and frees the one passed. PE 10170760.
    void setGoal(ClusterNode* goal);
    // Frees every node the open/closed table holds, and the goal. PE 101711A0.
    void reset();
    // PE 10170440: nothing to do.
    void returnReset() {}
    // PE 10170CC0 / 10170D50.
    void markOpen(ClusterNode* node);
    void markClosed(ClusterNode* node);

    // The neighbour across the portal at or after `edge` that the node can reach. PE 10170DD0.
    ClusterNode* getSuccessor(ClusterNode* node, unsigned short& edge, int& kind);
    // Re-parents an open or closed node when the current one reaches it more cheaply.
    // PE 10171050.
    bool relax(ClusterNode* current, ClusterNode* existing, int maxDepth);
    // Writes the clusters from the start to the goal into the path. PE 101715D0.
    void buildPath(ClusterNodePath* path, ClusterNode* goal);

private:
    // The cheapest way from a node through one of its cluster's portals into the neighbour
    // across `edge`: the portal's index and the matching portal on the far side. -1 when the
    // neighbour is not walkable or none fits. PE 10170950.
    float getStepCost(unsigned short& portalIndex, unsigned short& farPortal, ClusterNode* node,
                      unsigned short fromPortal, unsigned short edge, unsigned short cluster);
    // The node's path cost plus the way from where it entered to a portal. PE 101707B0.
    float getPortalCost(ClusterNode* node, unsigned short portal);
    // A cost plus the straight distance from the node's portal (or the start) to the goal.
    // PE 10170880.
    float getTotalCost(float cost, unsigned short portal, ClusterNode* node);
    // Reports a parent chain that loops back within ten steps. PE 10170210.
    bool checkParentChain(ClusterNode* node);
    ClusterNode* copyNode(const ClusterNode& node);    // PE 101703E0
    OCData* getNewOCData();                            // PE 10170900
    void recycleOCData(OCData* data)
    {
        data->m_next = m_freeOCData;
        m_freeOCData = data;
    }
    OCData* findOCData(const OCData* data) const;      // PE 10170BC0
    void insertOCData(OCData* data);                   // PE 10170B40
    void removeOCData(OCData* data);                   // PE 10170C40

public:
    /*  +0 */ QuadTree* m_quadTree;          // = 0
    /*  +4 */ ClusterGraph* m_clusterGraph;  // = 0
    /*  +8 */ Vec3 m_start;                  // = 0
    /* +20 */ Vec3 m_goal;                   // = 0
    /* +32 */ ClusterNode* m_goalNode;       // = 0
    /* +36 */ ClusterNode* m_spareNodes;     // = 0
    /* +40 */ int m_allocatedNodes;          // = 0
    /* +44 */ OCData* m_freeOCData;          // = 0
    /* +48 */ int m_allocatedOCData;         // = 0
    /* +52 */ Hash<OCData, ClusterNodeManager, OCData>* m_ocDataHash;   // new Hash(313), then ->this
    /* +56 */ std::vector<ClusterNode*> m_pathNodes;   // buildPath's scratch
    /* +68 */ unsigned short m_costType;     // = 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERNODEMANAGER_H
