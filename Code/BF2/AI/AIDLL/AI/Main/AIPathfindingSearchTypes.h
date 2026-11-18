// Shared path containers, node records and reusable A* search resources.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIPathfindingSearchTypes operations.

#ifndef AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
#define AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H

#include <list>
#include <set>
#include <utility>
#include <vector>

#include "IAIPathfinding.h"     // ai::triple, and Vec3 through AIDllTypes.h
#include "AIDllFrontier.h"      // AStarResource::update's named stop
#include "CommonSingletonServer.h"                           // PathfindingEngine::update's clock
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// Navigation path result with polygon positions and traversal state.
class QTPath
{
public:
    QTPath()
        : m_i0(0)
        , m_island(static_cast<unsigned int>(-1))
        , m_cost(0.0f)
    {
        // The vector is default-constructed (three null pointers in retail) by the member above
        // being absent from this list - the binary's four zero stores at +0 and +4..+15 are that
        // and nothing more.
    }

    /*  +0 */ int m_i0;                                       // = 0 [UNRESOLVED]
    /*  +4 */ std::vector<triple<unsigned int, unsigned int, Vec3> > m_nodes;
    /* +16 */ unsigned int m_island;                          // = -1; the island searched
    /* +20 */ float m_cost;                                   // = 0; the goal node's cost
};

// Cluster-level path result and traversal state.
class ClusterNodePath
{
public:
    ClusterNodePath()
        : m_i0(0)
    {
        // m_gap4 deliberately not initialised - the binary steps over it. See above.
    }

    // The position and trailing word of the portal an entry leaves its cluster through. False,
    // writing nothing, when the index is past the end or the entry has no portal (0xFFFF).
    // The index is relative to the start of the path, not to the cursor. PE 10139A40;
    // [INFERRED] name. Defined in AIPathfinding.cpp.
    bool getPortal(Vec3& position, unsigned int& value, unsigned int index) const;

    // The cluster an offset past the cursor, or 0xFFFF past the end. PE 101742A0;
    // [INFERRED] name.
    unsigned short getCluster(int offset) const
    {
        const int index = m_i0 + offset;
        if (index < static_cast<int>(m_nodes.size()))
            return m_nodes[index].first;
        return 0xFFFF;
    }

    /*  +0 */ int m_i0;                                       // = 0; the cursor into m_nodes
    /*  +4 */ int m_map;                                      // not constructed [INFERRED] name
    /*  +8 */ unsigned short m_costType;                      // not constructed [INFERRED] name
    /* +12 */ unsigned int m_island;                          // not constructed [INFERRED] name
    // Cluster and the portal leaving it.
    /* +16 */ std::vector<std::pair<unsigned short, unsigned short> > m_nodes;
};

// Reusable A* node pools, hash tables and search state.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding AIPathfindingSearchTypes operation.

// One tree node. PE layout: value, parent, left, right; a spare node is chained through its
// left link. The constructor (inlined into the tree's) clears only the value.
template <class T>
class BSTNode
{
public:
    BSTNode()
        : m_value(0)
    {
    }

    /*  +0 */ T* m_value;
    /*  +4 */ BSTNode<T>* m_parent;
    /*  +8 */ BSTNode<T>* m_left;    // also the free-list link
    /* +12 */ BSTNode<T>* m_right;
};

// The open set: an unbalanced binary search tree over the nodes' cost (PE node +0x18), with a
// pointer to the cheapest node and a pool of spare tree nodes. Equal costs go right. The node
// type supplies `m_key` and `m_cost`.
template <class Node, class SearchNode>
class BinarySearchTree
{
public:
    explicit BinarySearchTree(int n)
        : m_count(0)
        , m_free(0)
        , m_root(0)
    {
        // m_min deliberately not initialised, as shipped.
        for (int i = n; i > 0; --i)
        {
            BSTNode<SearchNode>* const node = new BSTNode<SearchNode>();
            node->m_left = m_free;
            m_free = node;
        }
    }

    // PE 100A1730.
    void insert(SearchNode* value)
    {
        BSTNode<SearchNode>* node = m_free;
        if (node)
            m_free = node->m_left;
        else
            node = new BSTNode<SearchNode>();
        node->m_value = value;
        node->m_left = 0;
        node->m_right = 0;
        if (m_count != 0)
        {
            attach(m_root, node);
            if (node->m_value->m_cost < m_min->m_value->m_cost)
                m_min = node;
            ++m_count;
        }
        else
        {
            m_min = node;
            m_root = node;
            node->m_parent = 0;
            ++m_count;
        }
    }

    // Removes and returns the cheapest node, or null when empty. PE 100A17C0.
    SearchNode* popMin()
    {
        BSTNode<SearchNode>* const node = m_min;
        if (!node)
            return 0;
        --m_count;
        if (node->m_right)
        {
            if (node->m_parent)
            {
                node->m_parent->m_left = node->m_right;
                node->m_right->m_parent = node->m_parent;
            }
            else
            {
                m_root = node->m_right;
                node->m_right->m_parent = 0;
            }
            m_min = leftmost(node->m_right);
        }
        else
        {
            m_min = node->m_parent;
            if (m_min)
            {
                m_min->m_left = 0;
            }
            else
            {
                // The shipped body reloads the (null) right child into both pointers here.
                m_min = node->m_right;
                m_root = node->m_right;
            }
        }
        release(node);
        return node->m_value;
    }

    // Removes the node with the value's key and returns it, or null. PE 100A18B0. A root
    // without a right child hands the tree to its left child unchecked, as shipped.
    SearchNode* remove(SearchNode* value)
    {
        if (m_count == 0)
            return 0;
        if (value->m_key == m_min->m_value->m_key)
            return popMin();
        BSTNode<SearchNode>* const node = find(value, m_root);
        if (!node)
            return 0;
        --m_count;
        if (node->m_parent)
        {
            BSTNode<SearchNode>*& slot =
                node->m_parent->m_left == node ? node->m_parent->m_left : node->m_parent->m_right;
            if (node->m_right)
            {
                slot = node->m_right;
                node->m_right->m_parent = node->m_parent;
                reattach(slot, node->m_left);
            }
            else
            {
                slot = node->m_left;
                if (node->m_left)
                    node->m_left->m_parent = node->m_parent;
            }
        }
        else if (node->m_right)
        {
            m_root = node->m_right;
            node->m_right->m_parent = 0;
            reattach(node->m_right, node->m_left);
        }
        else
        {
            m_root = node->m_left;
            node->m_left->m_parent = 0;
        }
        release(node);
        return node->m_value;
    }

    // The tree node holding the value's key in the subtree, searching each node's left subtree
    // before moving right. PE 1009F430.
    BSTNode<SearchNode>* find(SearchNode* value, BSTNode<SearchNode>* node)
    {
        for (; node; node = node->m_right)
        {
            if (value->m_key == node->m_value->m_key)
                return node;
            if (BSTNode<SearchNode>* const found = find(value, node->m_left))
                return found;
        }
        return 0;
    }

    // Returns every tree node to the pool. Inlined into PE 100A36D0 around the flatten
    // (PE 1009E950).
    void clear()
    {
        if (!m_root)
            return;
        BSTNode<SearchNode>* last;
        flatten(m_root, last);
        last->m_left = m_free;
        m_free = m_root;
        m_root = 0;
        m_count = 0;
        m_min = 0;
    }

    /*  +0 */ int m_count;
    /*  +4 */ BSTNode<SearchNode>* m_free;   // the pool of spare tree nodes
    /*  +8 */ BSTNode<SearchNode>* m_min;    // the cheapest node; not written by the constructor
    /* +12 */ BSTNode<SearchNode>* m_root;

private:
    // PE 1009F3E0: descends from `at` and hangs the node where its cost belongs.
    static void attach(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node)
    {
        for (;;)
        {
            if (node->m_value->m_cost < at->m_value->m_cost)
            {
                if (!at->m_left)
                {
                    at->m_left = node;
                    node->m_parent = at;
                    return;
                }
                at = at->m_left;
            }
            else
            {
                if (!at->m_right)
                {
                    at->m_right = node;
                    node->m_parent = at;
                    return;
                }
                at = at->m_right;
            }
        }
    }

    // PE 1009E950: threads the subtree into one chain through the left links, a node before
    // its left subtree before its right one, and reports the chain's last node.
    static void flatten(BSTNode<SearchNode>* node, BSTNode<SearchNode>*& last)
    {
        if (node->m_left)
        {
            BSTNode<SearchNode>* leftLast;
            flatten(node->m_left, leftLast);
            if (node->m_right)
            {
                flatten(node->m_right, last);
                leftLast->m_left = node->m_right;
            }
            else
            {
                last = leftLast;
            }
        }
        else if (node->m_right)
        {
            flatten(node->m_right, last);
            node->m_left = node->m_right;
        }
        else
        {
            last = node;
        }
    }

    // PE 1009F480: attach, for a subtree that may be absent.
    static void reattach(BSTNode<SearchNode>* at, BSTNode<SearchNode>* node)
    {
        if (node)
            attach(at, node);
    }

    // PE 1009F4D0.
    static BSTNode<SearchNode>* leftmost(BSTNode<SearchNode>* node)
    {
        if (!node)
            return 0;
        while (node->m_left)
            node = node->m_left;
        return node;
    }

    void release(BSTNode<SearchNode>* node)
    {
        node->m_left = m_free;
        m_free = node;
    }
};

// Chained hash over the nodes' key (PE node +0x10), with the non-empty buckets linked so they
// can be walked and cleared without scanning the table. The node type supplies `m_next`,
// `m_prev` and `m_key`.
template <class Node, class NodeManager, class SearchNode>
class Hash
{
public:
    // A bucket: its links in the list of non-empty buckets, and its chain.
    struct Bucket
    {
        Bucket* m_next;
        Bucket* m_prev;
        SearchNode* m_head;
    };

    explicit Hash(unsigned int n)
        : m_walkHash((Hash*)this)
        , m_endBucket(0)
        , m_endNode(0)
        , m_endHash((Hash*)this)
        , m_buckets(new Bucket[n + 1])
        , m_firstUsed(0)
        , m_count(0)
        , m_capacity((int)n)
        , m_nodeManager(0)
    {
        // The walk bucket and node are deliberately not initialised.
        // n + 1 entries, cleared from n down through zero.
        for (unsigned int i = 0; i <= n; ++i)
        {
            m_buckets[i].m_next = 0;
            m_buckets[i].m_prev = 0;
            m_buckets[i].m_head = 0;
        }
    }

    // PE 100A19C0: the node goes to the front of its chain.
    void insert(SearchNode* node)
    {
        Bucket* const bucket = bucketOf(node->m_key);
        SearchNode* const head = bucket->m_head;
        if (!head)
        {
            bucket->m_next = m_firstUsed;
            if (m_firstUsed)
                m_firstUsed->m_prev = bucket;
            bucket->m_prev = 0;
            m_firstUsed = bucket;
        }
        else
        {
            head->m_prev = node;
        }
        node->m_next = head;
        node->m_prev = 0;
        bucket->m_head = node;
        ++m_count;
    }

    // PE 100A1A80.
    void remove(SearchNode* node)
    {
        if (!node->m_prev)
        {
            Bucket* const bucket = bucketOf(node->m_key);
            bucket->m_head = node->m_next;
            if (!bucket->m_head)
            {
                if (m_firstUsed == bucket)
                    m_firstUsed = bucket->m_next;
                else
                    bucket->m_prev->m_next = bucket->m_next;
                if (bucket->m_next)
                    bucket->m_next->m_prev = bucket->m_prev;
            }
        }
        else
        {
            node->m_prev->m_next = node->m_next;
        }
        --m_count;
        if (node->m_next)
            node->m_next->m_prev = node->m_prev;
    }

    // The stored node with the key, or null. Inlined at every PE use.
    SearchNode* find(int key) const
    {
        for (SearchNode* node = bucketOf(key)->m_head; node; node = node->m_next)
        {
            if (key == node->m_key)
                return node;
        }
        return 0;
    }

    // Empties every used bucket without touching the nodes. Inlined into PE 100A36D0.
    void clear()
    {
        m_count = 0;
        for (Bucket* bucket = m_firstUsed; bucket; bucket = bucket->m_next)
            bucket->m_head = 0;
        m_firstUsed = 0;
    }

    // A walk position and its end mark, each (bucket, node, hash); the open-set refill writes
    // the walk's start here. [INFERRED] names.
    /*  +0 */ Bucket* m_walkBucket;      // never written by the constructor
    /*  +4 */ SearchNode* m_walkNode;    // never written by the constructor
    /*  +8 */ Hash* m_walkHash;          // = this
    /* +12 */ Bucket* m_endBucket;       // = 0
    /* +16 */ SearchNode* m_endNode;     // = 0
    /* +20 */ Hash* m_endHash;           // = this
    /* +24 */ Bucket* m_buckets;
    /* +28 */ Bucket* m_firstUsed;       // head of the list of non-empty buckets
    /* +32 */ int m_count;
    /* +36 */ int m_capacity;            // = n
    /* +40 */ NodeManager* m_nodeManager;// = 0, written by the owner right after construction

private:
    Bucket* bucketOf(int key) const
    {
        int index = key % m_capacity;
        if (key < 0)
            index += m_capacity;
        return &m_buckets[index];
    }
};

// State for one A* search. The open set is split: the tree holds up to m_openSize of the
// cheapest open nodes, all cheaper than m_threshold, and every other open node waits in the
// overflow hash; when the tree runs dry it is refilled from the hash. Closed nodes sit in their
// own hash. The node type supplies the links, key, cost, depth and the successor walk; the
// node manager supplies markOpen / markClosed / relax / freeNode / reset.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
class AStarResource
{
public:
    // Status values. [INFERRED] names; 4 is only ever returned by the owner.
    enum { Idle = 0, Searching = 1, Found = 2, Failed = 3 };

    explicit AStarResource(NodeManager* nodeManager)
        : m_status(0)
        , m_maxIterations(0)
        , m_maxSearchDepth(-1)
        , m_iterations(0)
        , m_openSize(0)
        , m_hashSize(0)
        , m_owner(-1)
        , m_nodeManager(nodeManager)
        , m_threshold(0)
        , m_open(0)
        , m_openOverflow(0)
        , m_closed(0)
        , m_goal(0)
    {
        // m_i4 deliberately not initialised - see above.
    }

    // Expands nodes until the update time reaches the deadline, the goal is found, the open
    // set runs out, or the iteration or depth limit is hit. PE 100ACCD0 (QTSearchNode).
    void update(double deadline);
    // Empties the open and closed sets and resets the node manager. PE 100A36D0.
    void reset();
    // Marks the node open and files it in the tree or the overflow. PE 100A6610.
    void addOpen(SearchNode* node);

    /*  +0 */ int m_status;          // = 0
    /*  +4 */ int m_i4;              // never written
    /*  +8 */ int m_maxIterations;   // = 0
    /* +12 */ int m_maxSearchDepth;  // = -1
    /* +16 */ int m_iterations;      // = 0
    /* +20 */ int m_openSize;        // = the int handed to BinarySearchTree's constructor
    /* +24 */ int m_hashSize;        // = the int handed to both Hash constructors
    /* +28 */ int m_owner;           // = -1 while the resource is free
    /* +32 */ NodeManager* m_nodeManager;
    /* +36 */ SearchNode* m_threshold;   // = 0; the dearest node of the last refill
    /* +40 */ BinarySearchTree<Node, SearchNode>* m_open;
    /* +44 */ Hash<Node, NodeManager, SearchNode>* m_openOverflow;
    /* +48 */ std::vector<SearchNode*> m_scratch;    // only ever emptied [UNRESOLVED] purpose
    /* +60 */ Hash<Node, NodeManager, SearchNode>* m_closed;
    /* +64 */ SearchNode* m_goal;    // = 0; the goal node of a finished search

private:
    struct CostLess
    {
        bool operator()(const SearchNode* a, const SearchNode* b) const { return a->m_cost < b->m_cost; }
    };

    SearchNode* popBest();                               // PE 100AC880
    void refill();                                       // PE 100ABE20
    void spill();                                        // PE 100A3DE0
    SearchNode* findOpen(SearchNode* node);              // PE 100A3A30
    bool removeOpen(SearchNode* node);                   // PE 100A3AA0
    void close(SearchNode* node);                        // PE 100A3B10
    // Advances a walk over the overflow hash; both become null at the end.
    static void advance(typename Hash<Node, NodeManager, SearchNode>::Bucket*& bucket, SearchNode*& node)
    {
        node = node->m_next;
        if (!node)
        {
            bucket = bucket->m_next;
            if (bucket)
                node = bucket->m_head;
        }
    }
};

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::reset()
{
    m_iterations = 0;
    m_status = Idle;
    m_threshold = 0;
    m_open->clear();
    m_openOverflow->clear();
    m_closed->clear();
    m_nodeManager->reset();
}

// A node at least as dear as the threshold, or any node while the tree is empty, waits in the
// overflow. A full tree is spilled to the overflow before it takes a cheaper node.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::addOpen(SearchNode* node)
{
    m_nodeManager->markOpen(node);
    if (m_open->m_count != 0 && node->m_cost < m_threshold->m_cost)
    {
        if (m_open->m_count >= m_openSize)
            spill();
        m_open->insert(node);
        return;
    }
    m_openOverflow->insert(node);
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::spill()
{
    while (m_open->m_count != 0)
        m_openOverflow->insert(m_open->popMin());
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
SearchNode* AStarResource<Node, NodeManager, SearchNode, Key, Path>::popBest()
{
    while (m_open->m_count == 0)
    {
        if (m_openOverflow->m_count == 0)
            return 0;
        refill();
    }
    return m_open->popMin();
}

// Moves the cheapest overflow nodes into the tree. The first m_openSize nodes of the walk seed
// an ordered set; each later node at most as dear as the set's dearest replaces it - the erase
// is by cost, so every node of that cost leaves the set together, as shipped. The threshold
// becomes the dearest node kept. The walk's start is written into the hash's own position words.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::refill()
{
    typedef typename Hash<Node, NodeManager, SearchNode>::Bucket Bucket;
    m_scratch.resize(0, 0);
    Hash<Node, NodeManager, SearchNode>* const hash = m_openOverflow;
    Bucket* bucket;
    SearchNode* node;
    if (hash->m_count > 0)
    {
        bucket = hash->m_firstUsed;
        hash->m_walkBucket = bucket;
        node = bucket->m_head;
        hash->m_walkNode = node;
    }
    else
    {
        hash->m_walkBucket = hash->m_endBucket;
        bucket = hash->m_walkBucket;
        hash->m_walkNode = hash->m_endNode;
        hash->m_walkHash = hash->m_endHash;
        node = hash->m_endNode;
    }
    if (bucket == hash->m_endBucket && node == hash->m_endNode)
        return;

    std::multiset<SearchNode*, CostLess> best;
    SearchNode* dearest = node;
    for (int i = 0; i < m_openSize; ++i)
    {
        if (bucket == hash->m_endBucket && node == hash->m_endNode)
            break;
        best.insert(node);
        if (dearest->m_cost < node->m_cost)
            dearest = node;
        advance(bucket, node);
    }
    while (!(bucket == hash->m_endBucket && node == hash->m_endNode))
    {
        if (dearest->m_cost < node->m_cost)
        {
            advance(bucket, node);
            continue;
        }
        typename std::multiset<SearchNode*, CostLess>::iterator last = best.end();
        --last;
        best.erase(*last);
        best.insert(node);
        advance(bucket, node);
        last = best.end();
        --last;
        dearest = *last;
    }
    for (typename std::multiset<SearchNode*, CostLess>::iterator it = best.begin(); it != best.end(); ++it)
    {
        m_openOverflow->remove(*it);
        m_open->insert(*it);
    }
    m_threshold = dearest;
}

// The open node with the given node's key: from the overflow, else from the tree.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
SearchNode* AStarResource<Node, NodeManager, SearchNode, Key, Path>::findOpen(SearchNode* node)
{
    if (SearchNode* const found = m_openOverflow->find(node->m_key))
        return found;
    BSTNode<SearchNode>* const treeNode = m_open->find(node, m_open->m_root);
    return treeNode ? treeNode->m_value : 0;
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
bool AStarResource<Node, NodeManager, SearchNode, Key, Path>::removeOpen(SearchNode* node)
{
    if (SearchNode* const found = m_openOverflow->find(node->m_key))
    {
        m_openOverflow->remove(found);
        return true;
    }
    return m_open->remove(node) != 0;
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::close(SearchNode* node)
{
    m_nodeManager->markClosed(node);
    m_closed->insert(node);
}

// A successor leading straight back to the node's parent is dropped before it is looked at.
// An already-open or already-closed polygon is re-queued when the new way is cheaper; the
// successor node itself is always returned to the pool. Finding the goal leaves the expanded
// node closed and the goal node in m_goal.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::update(double deadline)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    if (m_status != Searching)
        return;
    while (timer->getUpdateTime() < deadline)
    {
        SearchNode* const current = popBest();
        if (!current)
        {
            m_status = Failed;
            return;
        }
        int kind;
        unsigned short edge;
        SearchNode* successor = current->getFirstSuccessor(m_nodeManager, kind, edge);
        while (successor)
        {
            if (current->m_parent && successor->m_key == current->m_parent->m_key)
            {
                m_nodeManager->freeNode(successor);
                successor = current->getNextSuccessor(m_nodeManager, kind, edge);
                continue;
            }
            switch (kind)
            {
            case SearchNode::New:
                addOpen(successor);
                break;
            case SearchNode::AlreadyOpen:
            {
                SearchNode* const existing = findOpen(successor);
                if (m_nodeManager->relax(current, existing, m_maxSearchDepth))
                {
                    removeOpen(successor);
                    addOpen(existing);
                }
                m_nodeManager->freeNode(successor);
                break;
            }
            case SearchNode::AlreadyClosed:
            {
                SearchNode* const existing = m_closed->find(successor->m_key);
                if (m_nodeManager->relax(current, existing, m_maxSearchDepth))
                {
                    m_closed->remove(existing);
                    addOpen(existing);
                }
                m_nodeManager->freeNode(successor);
                break;
            }
            case SearchNode::Goal:
                m_goal = successor;
                successor->m_parent = current;
                close(current);
                m_status = Found;
                return;
            case SearchNode::Skip:
                m_nodeManager->freeNode(successor);
                break;
            default:
                break;
            }
            if (m_status == Found)
                return;
            successor = current->getNextSuccessor(m_nodeManager, kind, edge);
        }
        if (m_status == Found)
            return;
        const int depth = current->m_depth;
        close(current);
        ++m_iterations;
        if (m_iterations == m_maxIterations || m_maxSearchDepth == depth)
        {
            m_status = Failed;
            return;
        }
    }
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// The cluster search runs the same template once ClusterNode and ClusterNodeManager's search
// members are recovered; until then its update is a named stop.
class ClusterNode;
class ClusterNodeManager;
template <>
inline void AStarResource<ClusterNode, ClusterNodeManager, ClusterNode, unsigned short,
                          ClusterNodePath>::update(double)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AStarResource<ClusterNode>::update");
}

// Pathfinding engine and its pool of reusable search resources.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
class PathfindingEngine
{
public:
    PathfindingEngine() {}

    // [INFERRED] Steps active searches round-robin until now + budget, at most once per
    // search per call, moving each processed search from front to back.
    void update(double budget);
    // Hands out the first available search for the owner; -1 and false when there is none or
    // it is still owned. PE 100A8F30.
    bool acquire(unsigned int& id, int owner);
    // Takes the search back from the owner that holds it and clears its limits. PE 100A8FE0.
    void release(unsigned int& id, int owner);
    // Starts the search between two nodes; a start on the goal's polygon is found at once.
    // PE 100A9080.
    int start(unsigned int id, SearchNode* from, SearchNode* goal);

    /*  +0 */ std::vector<AStarResource<Node, NodeManager, SearchNode, Key, Path>*> m_searches;
    /* +12 */ std::list<unsigned int> m_available;    // [INFERRED] the free list
    /* +16 */ std::list<unsigned int> m_inFlight;     // [INFERRED] - isLockComplete waits on this one
};

// The timer is fetched once; the in-flight count once, before the pass (libstdc++'s O(n) size).
// The walk keeps its own iterator but always erases the front and re-appends its id, which is the
// same element on every step because each processed id moves to the back. The deadline test comes
// before the count test in both images.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::update(double budget)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double deadline = timer->getUpdateTime() + budget;
    const unsigned int count = static_cast<unsigned int>(m_inFlight.size());
    unsigned int processed = 0;
    std::list<unsigned int>::iterator it = m_inFlight.begin();
    while (deadline > timer->getUpdateTime() && processed < count)
    {
        ++processed;
        const unsigned int id = *it;
        m_searches[id]->update(deadline);
        ++it;
        m_inFlight.erase(m_inFlight.begin());
        m_inFlight.insert(m_inFlight.end(), id);
    }
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
bool PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::acquire(unsigned int& id, int owner)
{
    if (m_available.size() == 0)
    {
        id = static_cast<unsigned int>(-1);
        return false;
    }
    id = m_available.front();
    AStarResource<Node, NodeManager, SearchNode, Key, Path>* const search = m_searches[id];
    if (search->m_owner >= 0)
    {
        id = static_cast<unsigned int>(-1);
        return false;
    }
    search->m_owner = owner;
    m_available.pop_front();
    m_inFlight.push_back(id);
    return true;
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::release(unsigned int& id, int owner)
{
    AStarResource<Node, NodeManager, SearchNode, Key, Path>* const search = m_searches[id];
    if (search->m_owner != owner)
        return;
    search->m_owner = -1;
    search->m_maxSearchDepth = -1;
    search->m_maxIterations = 0;
    search->m_nodeManager->returnReset();
    search->m_status = 0;
    search->m_owner = -1;
    m_available.push_back(id);
    m_inFlight.remove(id);
    id = static_cast<unsigned int>(-1);
}

template <class Node, class NodeManager, class SearchNode, class Key, class Path>
int PathfindingEngine<Node, NodeManager, SearchNode, Key, Path>::start(unsigned int id, SearchNode* from,
                                                                       SearchNode* goal)
{
    m_searches[id]->reset();
    AStarResource<Node, NodeManager, SearchNode, Key, Path>* const search = m_searches[id];
    if (from->m_key != goal->m_key)
    {
        search->m_nodeManager->setGoal(goal);
        search->addOpen(from);
        search->m_status = 1;
    }
    else
    {
        search->m_nodeManager->freeNode(goal);
        search->m_goal = from;
        search->m_status = 2;
    }
    return search->m_status;
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
