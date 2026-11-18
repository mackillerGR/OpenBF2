// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining ClusterNodeManager operations.

#include "ClusterNodeManager.h"

#include "../../AIUtils/checkPointer.h"
#include "../../Main/AIDllFrontier.h"
#include "../../../../../Core/Debug.h"
#include "ClusterGraph.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// Initializes empty node lists and a 313-bucket hash linked to this manager.

ClusterNodeManager::ClusterNodeManager()
    : m_quadTree(0)
    , m_clusterGraph(0)
    , m_goalNode(0)
    , m_spareNodes(0)
    , m_allocatedNodes(0)
    , m_freeOCData(0)
    , m_allocatedOCData(0)
    , m_ocDataHash(checkPointer(new Hash<OCData, ClusterNodeManager, OCData>(kOCDataBuckets)))
    , m_costType(0)
{
    m_start.x = m_start.y = m_start.z = 0.0f;
    m_goal.x = m_goal.y = m_goal.z = 0.0f;
    m_ocDataHash->m_nodeManager = this;
}

// [FRONTIER] Cleanup of the hash, buckets, and OCData chain remains unimplemented.
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// Replace the stop when ordered node-manager teardown is recovered.
ClusterNodeManager::~ClusterNodeManager()
{
    BF2_AI_FRONTIER("dice::hfe::ai::ClusterNodeManager::~ClusterNodeManager");
}

ClusterNode* ClusterNode::getFirstSuccessor(ClusterNodeManager* manager, int& kind, unsigned short& edge)
{
    edge = 0;
    ClusterNode* const successor = manager->getSuccessor(this, edge, kind);
    ++edge;
    return successor;
}

ClusterNode* ClusterNode::getNextSuccessor(ClusterNodeManager* manager, int& kind, unsigned short& edge)
{
    ClusterNode* const successor = manager->getSuccessor(this, edge, kind);
    ++edge;
    return successor;
}

// The parent is written last, after the key and depth, on both paths.
ClusterNode* ClusterNodeManager::allocNode(unsigned short key, ClusterNode* parent, int depth)
{
    ClusterNode* node = m_spareNodes;
    if (node)
    {
        node->m_key = key;
        node->m_depth = depth;
        m_spareNodes = node->m_next;
        node->m_parent = parent;
        return node;
    }
    node = new ClusterNode(key, depth);
    ++m_allocatedNodes;
    node->m_parent = parent;
    return node;
}

ClusterNode* ClusterNodeManager::copyNode(const ClusterNode& source)
{
    ClusterNode* node = m_spareNodes;
    if (node)
    {
        m_spareNodes = node->m_next;
        *node = source;
        return node;
    }
    node = new ClusterNode(source);
    ++m_allocatedNodes;
    return node;
}

void ClusterNodeManager::setGoal(ClusterNode* goal)
{
    if (m_goalNode)
    {
        m_goalNode->m_key = goal->m_key;
        m_goalNode->m_portal = goal->m_portal;
    }
    else
    {
        m_goalNode = copyNode(*goal);
    }
    freeNode(goal);
}

void ClusterNodeManager::reset()
{
    Hash<OCData, ClusterNodeManager, OCData>* const hash = m_ocDataHash;
    while (hash->m_firstUsed)
    {
        OCData* const data = hash->m_firstUsed->m_head;
        freeNode(data->m_node);
        removeOCData(data);
        recycleOCData(data);
    }
    if (m_goalNode)
    {
        freeNode(m_goalNode);
        m_goalNode = 0;
    }
}

ClusterNodeManager::OCData* ClusterNodeManager::getNewOCData()
{
    OCData* data = m_freeOCData;
    if (data)
    {
        m_freeOCData = data->m_next;
        return data;
    }
    ++m_allocatedOCData;
    data = new OCData;
    data->m_open = true;
    data->m_closed = false;
    data->m_node = 0;
    data->m_next = 0;
    return checkPointer(data);
}

ClusterNodeManager::OCData* ClusterNodeManager::findOCData(const OCData* data) const
{
    const unsigned short key = data->m_node->m_key;
    const Hash<OCData, ClusterNodeManager, OCData>* const hash = m_ocDataHash;
    for (OCData* entry = hash->m_buckets[key % static_cast<unsigned int>(hash->m_capacity)].m_head; entry;
         entry = entry->m_next)
    {
        if (entry->m_node->m_key == key)
            return entry;
    }
    return 0;
}

void ClusterNodeManager::insertOCData(OCData* data)
{
    Hash<OCData, ClusterNodeManager, OCData>* const hash = m_ocDataHash;
    Hash<OCData, ClusterNodeManager, OCData>::Bucket* const bucket =
        &hash->m_buckets[data->m_node->m_key % static_cast<unsigned int>(hash->m_capacity)];
    OCData* const head = bucket->m_head;
    if (!head)
    {
        bucket->m_next = hash->m_firstUsed;
        if (hash->m_firstUsed)
            hash->m_firstUsed->m_prev = bucket;
        bucket->m_prev = 0;
        hash->m_firstUsed = bucket;
    }
    else
    {
        head->m_prev = data;
    }
    data->m_next = head;
    data->m_prev = 0;
    bucket->m_head = data;
    ++hash->m_count;
}

void ClusterNodeManager::removeOCData(OCData* data)
{
    Hash<OCData, ClusterNodeManager, OCData>* const hash = m_ocDataHash;
    if (!data->m_prev)
    {
        Hash<OCData, ClusterNodeManager, OCData>::Bucket* const bucket =
            &hash->m_buckets[data->m_node->m_key % static_cast<unsigned int>(hash->m_capacity)];
        bucket->m_head = data->m_next;
        if (!bucket->m_head)
        {
            if (hash->m_firstUsed == bucket)
                hash->m_firstUsed = bucket->m_next;
            else
                bucket->m_prev->m_next = bucket->m_next;
            if (bucket->m_next)
                bucket->m_next->m_prev = bucket->m_prev;
        }
    }
    else
    {
        data->m_prev->m_next = data->m_next;
    }
    --hash->m_count;
    if (data->m_next)
        data->m_next->m_prev = data->m_prev;
}

// An existing entry only has its open flag raised; its closed flag stays.
void ClusterNodeManager::markOpen(ClusterNode* node)
{
    OCData* const data = getNewOCData();
    data->m_node = node;
    data->m_open = true;
    data->m_closed = false;
    OCData* const existing = findOCData(data);
    if (!existing)
    {
        insertOCData(data);
        return;
    }
    recycleOCData(data);
    existing->m_open = true;
}

// The node must already be in the table; the lookup is not null-checked.
void ClusterNodeManager::markClosed(ClusterNode* node)
{
    OCData* const data = getNewOCData();
    data->m_open = true;
    data->m_closed = false;
    data->m_node = node;
    OCData* const existing = findOCData(data);
    recycleOCData(data);
    existing->m_open = false;
    existing->m_closed = true;
}

bool ClusterNodeManager::checkParentChain(ClusterNode* node)
{
    ClusterNode* parent = node->m_parent;
    for (int i = 0; parent && i < 10; ++i)
    {
        if (parent == node)
        {
            Debug(DebugTypeError, __FILE__, __LINE__, "") << "Circular heritage for " << node->m_key;
            return false;
        }
        parent = parent->m_parent;
    }
    return true;
}

float ClusterNodeManager::getTotalCost(float cost, unsigned short portal, ClusterNode* node)
{
    const Vec3* position = &m_start;
    if (node->m_parent)
        position = &m_clusterGraph->getClusterPortal(node->m_key, m_costType, portal)->m_position;
    const float dx = m_goal.x - position->x;
    const float dy = m_goal.y - position->y;
    const float dz = m_goal.z - position->z;
    return std::sqrt(dx * dx + dy * dy + dz * dz) + cost;
}

float ClusterNodeManager::getPortalCost(ClusterNode* node, unsigned short portal)
{
    if (!node)
        return 0.0f;
    float cost;
    if (node->m_portal != 0xFFFF)
    {
        cost = m_clusterGraph->getPortalToPortalCost(node->m_key, node->m_portal, portal, m_costType);
        if (cost < 0.0f)
            return -1.0f;
    }
    else
    {
        const Vec3& position = m_clusterGraph->getClusterPortal(node->m_key, m_costType, portal)->m_position;
        const float dx = position.x - m_start.x;
        const float dy = position.y - m_start.y;
        const float dz = position.z - m_start.z;
        cost = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return cost + node->m_pathCost;
}

// fromPortal is not read.
float ClusterNodeManager::getStepCost(unsigned short& portalIndex, unsigned short& farPortal, ClusterNode* node,
                                      unsigned short, unsigned short edge, unsigned short cluster)
{
    const unsigned short count = m_clusterGraph->getCluster(cluster).m_numberOfPortals;
    const unsigned short target = m_clusterGraph->getClusterPortal(cluster, m_costType, edge)->m_cluster;
    if (target >= m_clusterGraph->getNumberOfClusters() || !m_clusterGraph->getCluster(target).isWalkable())
        return -1.0f;

    farPortal = 0xFFFF;
    float best = 1.0e9f;
    if (count == 0)
        return -1.0f;
    for (unsigned short i = 0; i < count; ++i)
    {
        ClusterPortal* const portal = m_clusterGraph->getClusterPortal(cluster, m_costType, i);
        if (portal->m_cluster != target)
            continue;
        const float cost = getPortalCost(node, i);
        if (cost < best && !(cost < 0.0f))
        {
            portalIndex = i;
            farPortal = m_clusterGraph->getMatchingPortal(*portal, m_costType);
            best = cost;
        }
    }
    if (farPortal == 0xFFFF)
        return -1.0f;
    return best;
}

// The probe record goes back to the free list on every path, including the new-node one.
ClusterNode* ClusterNodeManager::getSuccessor(ClusterNode* node, unsigned short& edge, int& kind)
{
    const unsigned short count = m_clusterGraph->getCluster(node->m_key).m_numberOfPortals;
    float cost = -1.0f;
    unsigned short farPortal = 0xFFFF;
    unsigned short portalIndex = 0;
    if (edge < count)
    {
        cost = getStepCost(portalIndex, farPortal, node, node->m_portal, edge, node->m_key);
        while (edge < count)
        {
            if (!(cost < 0.0f) && farPortal != 0xFFFF && edge != node->m_portal && portalIndex >= edge)
                break;
            ++edge;
            cost = getStepCost(portalIndex, farPortal, node, node->m_portal, edge, node->m_key);
        }
    }
    if (edge >= count)
    {
        kind = ClusterNode::Skip;
        return 0;
    }

    const unsigned short neighbour =
        m_clusterGraph->getClusterPortal(node->m_key, m_costType, portalIndex)->m_cluster;
    ClusterNode* const successor = allocNode(neighbour, node, node->m_depth + 1);
    OCData* const data = getNewOCData();
    data->m_open = true;
    data->m_closed = false;
    data->m_node = successor;
    OCData* const existing = findOCData(data);
    if (existing)
    {
        if (existing->m_open)
            kind = ClusterNode::AlreadyOpen;
        else if (existing->m_closed)
            kind = ClusterNode::AlreadyClosed;
        else
        {
            Debug(DebugTypeError, __FILE__, __LINE__, "") << "This case should not exists!";
            kind = ClusterNode::Skip;
        }
        recycleOCData(data);
        return successor;
    }

    checkParentChain(successor);
    successor->m_portal = farPortal;
    if (successor->m_key == m_goalNode->m_key)
    {
        kind = ClusterNode::Goal;
        recycleOCData(data);
        return successor;
    }
    kind = ClusterNode::New;
    successor->m_pathCost = cost;
    successor->m_cost = getTotalCost(cost, farPortal, successor);
    recycleOCData(data);
    return successor;
}

// The portal back to the current cluster is written and then overwritten by the far portal.
bool ClusterNodeManager::relax(ClusterNode* current, ClusterNode* existing, int maxDepth)
{
    const int depth = current->m_depth + 1;
    if (maxDepth == depth)
        return false;
    if (!existing->m_parent)
        return false;

    const unsigned short edge = m_clusterGraph->getPortalTo(current->m_key, m_costType, existing->m_key);
    unsigned short portalIndex;
    unsigned short farPortal;
    const float cost = getStepCost(portalIndex, farPortal, current, current->m_portal, edge, current->m_key);
    if (cost < 0.0f)
        return false;
    const float total = getTotalCost(cost, farPortal, existing);
    if (!(total < existing->m_cost))
        return false;

    existing->m_portal = m_clusterGraph->getPortalIndexTo(existing->m_key, m_costType, current->m_key);
    existing->m_parent = current;
    checkParentChain(existing);
    existing->m_pathCost = cost;
    existing->m_portal = farPortal;
    existing->m_cost = total;
    existing->m_depth = depth;
    return true;
}

void ClusterNodeManager::buildPath(ClusterNodePath* path, ClusterNode* goal)
{
    m_pathNodes.resize(0, 0);
    for (ClusterNode* node = goal; node; node = node->m_parent)
        m_pathNodes.push_back(node);
    for (int i = static_cast<int>(m_pathNodes.size()) - 1; i >= 0; --i)
    {
        const ClusterNode* const node = m_pathNodes[i];
        path->m_nodes.push_back(std::make_pair(node->m_key, node->m_portal));
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
