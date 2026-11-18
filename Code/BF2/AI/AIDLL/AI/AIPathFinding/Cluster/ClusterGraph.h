// Navigation cluster graph with file loading and cleanup.
// [FRONTIER] Remaining graph queries and mesh generation require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H

#include <set>

#include "../../Main/AIDllTypes.h"      // Vec3
#include "Cluster.h"

namespace dice {
namespace hfe {

namespace io { class File; }

namespace ai {

class QuadTree;

// Portal record used by the cluster graph and its serialized format.
class ClusterPortal
{
public:
    ClusterPortal() { m_position.x = 0.0f; m_position.y = 0.0f; m_position.z = 0.0f; }

    /* +0x00 */ Vec3          m_position;          // [INFERRED] name
    /* +0x0C */ unsigned short m_cluster;          // [INFERRED] the cluster the portal leads to
    /* +0x0E */ unsigned char m_unread0E[2];       // [UNRESOLVED]
    /* +0x10 */ unsigned int  m_value10;           // a polygon: the polygon search ends on it
};

typedef char ClusterPortalIsTheFileRecord[sizeof(ClusterPortal) == 20 ? 1 : -1];

class ClusterGraph
{
public:
    ClusterGraph();
    ~ClusterGraph();

    void clean();
    void readFromFile(io::File& file);

    // [CHOSEN name] Returns the stored cluster count used by QuadTreeManager.
    unsigned int getNumberOfClusters() const { return m_numberOfClusters; }

    // A cluster's portal record for a cost type. Nothing is range-checked. PE 101574E0.
    ClusterPortal* getClusterPortal(unsigned short cluster, unsigned short costType,
                                    unsigned short portal) const
    {
        const Cluster& entry = m_clusters[cluster];
        return &m_portals[entry.m_numberOfPortals * costType + entry.m_firstPortal + portal];
    }

    const Cluster& getCluster(unsigned short cluster) const { return m_clusters[cluster]; }

    // The cost table entry between two of a cluster's portals for a cost type. The table holds
    // each unordered pair once. PE 10157840; [INFERRED] name.
    float getPortalToPortalCost(unsigned short cluster, unsigned short a, unsigned short b,
                                unsigned short costType) const;
    // The index of the cluster's portal for a cost type that leads to another cluster, or
    // 0xFFFF. PE 101578D0; [INFERRED] name.
    unsigned short getPortalTo(unsigned short cluster, unsigned short costType, unsigned short other) const;
    // The same, answering 0xFFFF for the cluster 0xFFFF. PE 10157520; [INFERRED] name.
    unsigned short getPortalIndexTo(unsigned short cluster, unsigned short costType,
                                    unsigned short other) const;
    // The index of the portal on the far side of a portal: same polygon, within 0.1. 0xFFFF
    // when none matches or the portal leads nowhere. PE 10159850; [INFERRED] name.
    unsigned short getMatchingPortal(const ClusterPortal& portal, unsigned short costType) const;

    // Whether one of a cluster's first-cost-type portals leads to the other cluster.
    // PE 10157940.
    bool areNeighbouringClusters(unsigned short cluster, unsigned short other) const
    {
        const Cluster& entry = m_clusters[cluster];
        for (unsigned short i = 0; i < entry.m_numberOfPortals; ++i)
        {
            if (m_portals[static_cast<unsigned short>(i) + entry.m_firstPortal].m_cluster == other)
                return true;
        }
        return false;
    }

private:
    friend class QuadTree;
    /* +0x00 */ unsigned int*  m_clusterPolygons;           // [INFERRED] polygon indices grouped by cluster
    /* +0x04 */ unsigned int   m_numberOfClusterPolygons;   // (Dalian Infantry: 18,512, every polygon once)
    /* +0x08 */ Cluster*       m_clusters;                  // getClusterPortal
    /* +0x0C */ unsigned int   m_numberOfClusters;          // loadQuadTrees' empty walk bound
    /* +0x10 */ unsigned int   m_numberOfPortals;
    /* +0x14 */ ClusterPortal* m_portals;                   // getClusterPortal
    /* +0x18 */ std::set<unsigned int>   m_set18;          // [UNRESOLVED] roles for both sets and
    /* +0x24 */ unsigned char*           m_array24;        // the +0x24 array: only the GTS
                                                             // generation path (not written) fills
                                                             // them; clean frees and nulls +0x24
    /* +0x28 */ unsigned int   m_numberOfPortalCosts;
    // float: the PE allocates it through its own checkPointer instantiation (, not the
    // uint one at the +0x00 list uses), and the shipped values are plainly floats
    // (Dalian Infantry opens 31.67, 24.51, 14.41, 56.92). Name [INFERRED] from
    // QuadTreeManager::recalculatePortalCosts -> ClusterGraph::calculatePortalCosts.
    /* +0x2C */ float*         m_portalCosts;
    /* +0x30 */ std::set<unsigned short> m_set30;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTERGRAPH_H
