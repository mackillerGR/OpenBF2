// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining ClusterGraph operations.

#include "ClusterGraph.h"

#include "../../AIUtils/checkPointer.h"
#include "../../../../../IO/File/File.h"

namespace dice {
namespace hfe {
namespace ai {

// Zeroes the six fields at +0x00..+0x14 and the three at +0x24..+0x2C; the two sets
// construct empty (their sentinel allocations are the only calls). is bigger only because the
// SGI node allocator is inlined into it - same source, two inlining decisions.
ClusterGraph::ClusterGraph()
    : m_clusterPolygons(0)
    , m_numberOfClusterPolygons(0)
    , m_clusters(0)
    , m_numberOfClusters(0)
    , m_numberOfPortals(0)
    , m_portals(0)
    , m_array24(0)
    , m_numberOfPortalCosts(0)
    , m_portalCosts(0)
{
}

// clean, then the two sets in reverse declaration order.
ClusterGraph::~ClusterGraph()
{
    clean();
}

// Releases arrays and clears counts, but leaves four freed pointers unchanged.
// Do not clean twice without repopulating those pointers; readFromFile assigns them
// immediately after cleanup. Only the pointer at +0x24 is cleared.
void ClusterGraph::clean()
{
    delete[] m_clusterPolygons;
    m_numberOfClusterPolygons = 0;

    delete[] m_clusters;
    m_numberOfClusters = 0;

    delete[] m_portals;
    m_numberOfPortals = 0;

    delete[] m_array24;
    m_array24 = 0;

    delete[] m_portalCosts;
    m_numberOfPortalCosts = 0;
}

// Loads a version-5 cluster graph. Unsupported versions leave the existing graph unchanged.
void ClusterGraph::readFromFile(io::File& file)
{
    int version = 0;
    file.read(version);

    if (version == 5)
    {
        clean();

        file.read(m_numberOfClusters);
        m_clusters = checkPointer(new Cluster[m_numberOfClusters]);
        file.read(m_clusters, sizeof(Cluster) * m_numberOfClusters);

        file.read(m_numberOfClusterPolygons);
        m_clusterPolygons = checkPointer(new unsigned int[m_numberOfClusterPolygons]);
        file.read(m_clusterPolygons, sizeof(unsigned int) * m_numberOfClusterPolygons);

        file.read(m_numberOfPortals);
        if (m_numberOfPortals)
        {
            m_portals = checkPointer(new ClusterPortal[m_numberOfPortals]);
            file.read(m_portals, sizeof(ClusterPortal) * m_numberOfPortals);
        }
        else
        {
            m_portals = 0;
        }

        file.read(m_numberOfPortalCosts);
        if (m_numberOfPortalCosts)
        {
            m_portalCosts = checkPointer(new float[m_numberOfPortalCosts]);
            file.read(m_portalCosts, sizeof(float) * m_numberOfPortalCosts);
        }
        else
        {
            m_portalCosts = 0;
        }
    }
}

float ClusterGraph::getPortalToPortalCost(unsigned short cluster, unsigned short a, unsigned short b,
                                          unsigned short costType) const
{
    const Cluster& entry = m_clusters[cluster];
    unsigned short low;
    unsigned short high;
    if (a < b)
    {
        low = a;
        high = b;
    }
    else
    {
        low = b;
        high = a;
    }
    const int count = entry.m_numberOfPortals;
    const int pairs = (count - 1) * count / 2;
    const int rest = static_cast<unsigned short>(count - low);
    const int tail = (rest - 1) * rest / 2;
    const unsigned short within = static_cast<unsigned short>(pairs - tail - low + high);
    const int index = within + static_cast<unsigned short>(pairs) * costType + entry.m_firstPortalCost;
    // [PLATFORM] The x86 images index `costs + index * 4 - 4` in 32-bit address arithmetic, so a
    // portal paired with itself in the first cluster (index 0) reads the word just before the
    // table. Signed arithmetic keeps that same element on a 64-bit port instead of wrapping the
    // unsigned index to 2^32 - 1.
    return m_portalCosts[index - 1];
}

unsigned short ClusterGraph::getPortalTo(unsigned short cluster, unsigned short costType,
                                         unsigned short other) const
{
    const Cluster& entry = m_clusters[cluster];
    const unsigned short count = entry.m_numberOfPortals;
    for (unsigned short i = 0; i < count; ++i)
    {
        if (m_portals[i + entry.m_firstPortal + count * costType].m_cluster == other)
            return i;
    }
    return 0xFFFF;
}

unsigned short ClusterGraph::getPortalIndexTo(unsigned short cluster, unsigned short costType,
                                              unsigned short other) const
{
    if (cluster == 0xFFFF)
        return 0xFFFF;
    return getPortalTo(cluster, costType, other);
}

unsigned short ClusterGraph::getMatchingPortal(const ClusterPortal& portal, unsigned short costType) const
{
    if (portal.m_cluster == 0xFFFF)
        return 0xFFFF;
    const Cluster& entry = m_clusters[portal.m_cluster];
    const unsigned short count = entry.m_numberOfPortals;
    for (unsigned short i = 0; i < count; ++i)
    {
        const ClusterPortal& candidate = m_portals[i + count * costType + entry.m_firstPortal];
        if (candidate.m_value10 != portal.m_value10)
            continue;
        const float dx = portal.m_position.x - candidate.m_position.x;
        const float dy = portal.m_position.y - candidate.m_position.y;
        const float dz = portal.m_position.z - candidate.m_position.z;
        if (dx * dx + dy * dy + dz * dz < 0.01f)
            return i;
    }
    return 0xFFFF;
}

} // namespace ai
} // namespace hfe
} // namespace dice
