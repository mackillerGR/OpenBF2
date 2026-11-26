// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining QTSearchNodeManager operations.

#include "QTSearchNodeManager.h"

#include <cmath>

#include "QuadTree.h"
#include "../Material/MaterialManager.h"
#include "../../Main/AIDllFrontier.h"
#include "../../Main/AIPathfindingSearchTypes.h"
#include "../../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

// Constructors share initialization except for the optional quad-tree pointer.
// Keep both overloads; observed stores do not establish member declaration order.

QTSearchNodeManager::QTSearchNodeManager()
    : m_quadTree(0)
    , m_goal(0)
    , m_nodeCache(0)
    , m_allocatedNodes(0)
    , m_stateCache(0)
    , m_usedEntries(0)
    , m_table(new CacheEntry[kCacheEntries])
    , m_additionalGoals(0)
    , m_allowedClusters(0)
    , m_usePathTopologySmoothing(false)
    , m_materialManager(0)
{
}

QTSearchNodeManager::QTSearchNodeManager(QuadTree* quadTree)
    : m_quadTree(quadTree)
    , m_goal(0)
    , m_nodeCache(0)
    , m_allocatedNodes(0)
    , m_stateCache(0)
    , m_usedEntries(0)
    , m_table(new CacheEntry[kCacheEntries])
    , m_additionalGoals(0)
    , m_allowedClusters(0)
    , m_usePathTopologySmoothing(false)
    , m_materialManager(0)
{
}

QTSearchNode* QTSearchNode::getFirstSuccessor(QTSearchNodeManager* manager, int& kind, unsigned short& edge)
{
    edge = 0;
    QTSearchNode* const successor = manager->getSuccessor(this, edge, kind);
    ++edge;
    return successor;
}

QTSearchNode* QTSearchNode::getNextSuccessor(QTSearchNodeManager* manager, int& kind, unsigned short& edge)
{
    QTSearchNode* const successor = manager->getSuccessor(this, edge, kind);
    ++edge;
    return successor;
}

QTSearchNode* QTSearchNodeManager::allocNode(int key, QTSearchNode* parent, int depth)
{
    QTSearchNode* node = m_nodeCache;
    if (node)
    {
        node->m_key = key;
        node->m_depth = depth;
        m_nodeCache = m_nodeCache->m_next;
    }
    else
    {
        node = new QTSearchNode(key, depth);
        ++m_allocatedNodes;
    }
    node->m_parent = parent;
    node->m_cameFrom = parent;
    return node;
}

// A recycled node takes every word of the original, links included; a new one counts as
// allocated.
QTSearchNode* QTSearchNodeManager::cloneNode(const QTSearchNode* node)
{
    QTSearchNode* const clone = m_nodeCache;
    if (clone)
    {
        m_nodeCache = clone->m_next;
        clone->m_next = node->m_next;
        clone->m_prev = node->m_prev;
        clone->m_parent = node->m_parent;
        clone->m_cameFrom = node->m_cameFrom;
        clone->m_key = node->m_key;
        clone->m_pathCost = node->m_pathCost;
        clone->m_cost = node->m_cost;
        clone->m_depth = node->m_depth;
        clone->m_position = node->m_position;
        return clone;
    }
    QTSearchNode* const fresh = new QTSearchNode(*node);
    ++m_allocatedNodes;
    return fresh;
}

void QTSearchNodeManager::setGoal(QTSearchNode* goal)
{
    if (!m_goal)
        m_goal = cloneNode(goal);
    else
        m_goal->m_key = goal->m_key;
    m_goal->m_position = goal->m_position;
    freeNode(goal);
}

void QTSearchNodeManager::reset()
{
    while (m_usedEntries)
    {
        CacheEntry* const entry = m_usedEntries;
        State* const first = entry->m_head;
        State* state = first;
        while (state->m_next)
        {
            freeNode(state->m_node);
            state = state->m_next;
        }
        freeNode(state->m_node);
        state->m_next = m_stateCache;
        m_stateCache = first;
        entry->m_head = 0;
        m_usedEntries = entry->m_next;
    }
    if (m_goal)
    {
        freeNode(m_goal);
        m_goal = 0;
    }
    m_allowedClusters = 0;
}

// The table has kCacheEntries buckets; a negative key that divides evenly indexes one past them,
// as shipped.
QTSearchNodeManager::State* QTSearchNodeManager::findState(int key) const
{
    int index = key % kCacheEntries;
    if (key < 0)
        index += kCacheEntries;
    for (State* state = m_table[index].m_head; state; state = state->m_next)
    {
        if (state->m_node->m_key == key)
            return state;
    }
    return 0;
}

// A new state starts not closed only when it is freshly allocated; a recycled one keeps its
// closed flag, as shipped.
void QTSearchNodeManager::markOpen(QTSearchNode* node)
{
    const int key = node->m_key;
    int index = key % kCacheEntries;
    if (key < 0)
        index += kCacheEntries;
    if (State* const known = findState(key))
    {
        known->m_open = true;
        return;
    }
    State* state = m_stateCache;
    if (state)
    {
        m_stateCache = state->m_next;
        state->m_open = true;
        state->m_node = node;
    }
    else
    {
        state = new State;
        state->m_open = true;
        state->m_closed = false;
        state->m_node = node;
    }
    CacheEntry* const entry = &m_table[index];
    state->m_next = entry->m_head;
    if (!entry->m_head)
    {
        entry->m_next = m_usedEntries;
        m_usedEntries = entry;
    }
    entry->m_head = state;
}

void QTSearchNodeManager::markClosed(QTSearchNode* node)
{
    State* const state = findState(node->m_key);
    state->m_open = false;
    state->m_closed = true;
}

Vec3 QTSearchNodeManager::getEdgeMidpoint(unsigned char edge, const QTSearchNode* node) const
{
    const Vec3& a = m_quadTree->getVertexOfPolygon(node->m_key, edge);
    const Vec3& b = m_quadTree->getWrappedVertexOfPolygon(node->m_key, static_cast<unsigned char>(edge + 1));
    Vec3 middle;
    middle.x = (b.x + a.x) * 0.5f;
    middle.y = (b.y + a.y) * 0.5f;
    middle.z = (b.z + a.z) * 0.5f;
    return middle;
}

// With path-topology smoothing, steps of 0.25 or longer pay more on slopes (the material cost
// is scaled by the departure polygon's steepness) and for the bend between the two polygons.
float QTSearchNodeManager::getStepCost(int fromKey, const Vec3& from, int toKey, const Vec3& to) const
{
    float cost = m_materialManager->getMaterialCost(m_quadTree->getMaterialOfPolygon(fromKey));
    if (cost < 0.0f)
        return -1.0f;
    const float dx = from.x - to.x;
    const float dy = from.y - to.y;
    const float dz = from.z - to.z;
    const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
    if (!m_usePathTopologySmoothing)
        return distance * cost;
    float bend = 0.0f;
    if (!(distance < 0.25f))
    {
        const Vec3& toNormal = m_quadTree->getNormalOfPolygon(toKey);
        const Vec3& fromNormal = m_quadTree->getNormalOfPolygon(fromKey);
        const float dot = toNormal.z * fromNormal.z + toNormal.y * fromNormal.y + toNormal.x * fromNormal.x;
        float slope = m_quadTree->getNormalOfPolygon(fromKey).y * 5.0f - 4.0f;
        if (!(0.0f < slope))
            slope = 0.0f;
        slope = 4.0f - slope * slope * 3.0f;
        cost = slope * slope * cost;
        float turn = dot * 5.0f - 4.0f;
        if (!(0.0f < turn))
            turn = 0.0f;
        turn = 2.0f - turn * turn;
        bend = turn * turn;
    }
    return distance * cost + bend;
}

float QTSearchNodeManager::getPathCost(const QTSearchNode* from, const QTSearchNode* to,
                                       const Vec3& position) const
{
    if (!from)
        return 0.0f;
    const float step = getStepCost(from->m_key, from->m_position, to->m_key, position);
    if (step <= 0.0f)
        return step;
    return step + from->m_pathCost;
}

float QTSearchNodeManager::getTotalCost(float pathCost, const Vec3& position) const
{
    const float dx = m_goal->m_position.x - position.x;
    const float dy = m_goal->m_position.y - position.y;
    const float dz = m_goal->m_position.z - position.z;
    return std::sqrt(dz * dz + dx * dx + dy * dy) + pathCost;
}

QTSearchNode* QTSearchNodeManager::getSuccessor(QTSearchNode* node, unsigned short& edge, int& kind)
{
    unsigned int neighbour = m_quadTree->getNeighbour(node->m_key, static_cast<unsigned char>(edge));
    while (neighbour == static_cast<unsigned int>(-1))
    {
        ++edge;
        if (edge >= m_quadTree->getPolygon(node->m_key)->m_numberOfVertices)
        {
            kind = QTSearchNode::Skip;
            return 0;
        }
        neighbour = m_quadTree->getNeighbour(node->m_key, static_cast<unsigned char>(edge));
    }
    QTSearchNode* const successor = allocNode(static_cast<int>(neighbour), node, node->m_depth + 1);
    State* const state = findState(successor->m_key);
    Polygon* const polygon = m_quadTree->getPolygon(neighbour);
    if (m_materialManager->getMaterialCost(polygon->m_material) < 0.0f
        || (m_allowedClusters && m_allowedClusters->find(polygon->m_cluster) == m_allowedClusters->end()))
    {
        kind = QTSearchNode::Skip;
        return successor;
    }
    if (state)
    {
        if (state->m_open)
        {
            kind = QTSearchNode::AlreadyOpen;
            return successor;
        }
        if (state->m_closed)
        {
            kind = QTSearchNode::AlreadyClosed;
            return successor;
        }
        Debug(DebugTypeError, __FILE__, __LINE__, "") << "This case should not exists!";
        kind = QTSearchNode::Skip;
        return successor;
    }
    const Vec3 position = getEdgeMidpoint(static_cast<unsigned char>(edge), node);
    successor->m_position = position;
    const float pathCost = getPathCost(node, successor, successor->m_position);
    successor->m_pathCost = pathCost;
    if (successor->m_key != m_goal->m_key
        && (!m_additionalGoals || m_additionalGoals->find(neighbour) == m_additionalGoals->end()))
    {
        successor->m_cost = getTotalCost(pathCost, position);
        kind = QTSearchNode::New;
        return successor;
    }
    successor->m_cost = getStepCost(node->m_key, position, static_cast<int>(neighbour), m_goal->m_position)
                        + successor->m_pathCost;
    kind = QTSearchNode::Goal;
    return successor;
}

bool QTSearchNodeManager::relax(QTSearchNode* node, QTSearchNode* existing, int maxDepth)
{
    const int depth = node->m_depth + 1;
    if (maxDepth == depth)
        return false;
    if (node->m_cameFrom == existing || !existing->m_parent)
        return false;
    unsigned char edge;
    m_quadTree->getNeighbourEdge(edge, node->m_key, existing->m_key);
    const Vec3 position = getEdgeMidpoint(edge, node);
    const float pathCost = getPathCost(node, existing, position);
    const float cost = getTotalCost(pathCost, position);
    if (!(cost + 0.5f < existing->m_cost))
        return false;
    existing->m_cameFrom = node;
    existing->m_position = position;
    existing->m_cost = cost;
    existing->m_parent = node;
    existing->m_pathCost = pathCost;
    existing->m_depth = depth;
    return true;
}

void QTSearchNodeManager::buildPath(QTPath* path, QTSearchNode* goal)
{
    m_pathNodes.resize(0, 0);
    int count = 0;
    for (QTSearchNode* node = goal; node;)
    {
        ++count;
        m_pathNodes.push_back(node);
        node = node->m_parent;
        if (count > 200)
            break;
    }
    for (int i = static_cast<int>(m_pathNodes.size()) - 1; i >= 0; --i)
    {
        QTSearchNode* const node = m_pathNodes[i];
        triple<unsigned int, unsigned int, Vec3> step;
        step.first = static_cast<unsigned int>(node->m_key);
        step.second = node->m_cameFrom ? static_cast<unsigned int>(node->m_cameFrom->m_key)
                                       : static_cast<unsigned int>(-1);
        step.third = node->m_position;
        path->m_nodes.push_back(step);
    }
    path->m_cost = goal->m_cost;
}

// PE 1016FAD0. Frees the spare nodes, then every state in the table with the node it holds, then
// the spare states, the table and the goal. Only the spare nodes and the goal lower the
// allocation count, as shipped.
QTSearchNodeManager::~QTSearchNodeManager()
{
    while (m_nodeCache)
    {
        QTSearchNode* const node = m_nodeCache;
        m_nodeCache = node->m_next;
        delete node;
        --m_allocatedNodes;
    }

    for (int i = 0; i < kCacheEntries; ++i)
    {
        State* state = m_table[i].m_head;
        while (state)
        {
            State* const next = state->m_next;
            delete state->m_node;
            delete state;
            state = next;
        }
    }

    while (m_stateCache)
    {
        State* const state = m_stateCache;
        m_stateCache = state->m_next;
        delete state;
    }

    delete[] m_table;

    if (m_goal)
    {
        delete m_goal;
        --m_allocatedNodes;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
