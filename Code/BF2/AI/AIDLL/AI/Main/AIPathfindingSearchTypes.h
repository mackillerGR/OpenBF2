// Shared path containers, node records and reusable A* search resources.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIPathfindingSearchTypes operations.

#ifndef AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
#define AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H

#include <list>
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
        , m_sentinel16(-1)
        , m_i20(0)
    {
        // The vector is default-constructed (three null pointers in retail) by the member above
        // being absent from this list - the binary's four zero stores at +0 and +4..+15 are that
        // and nothing more.
    }

    /*  +0 */ int m_i0;                                       // = 0 [UNRESOLVED]
    /*  +4 */ std::vector<triple<unsigned int, unsigned int, Vec3> > m_nodes;
    /* +16 */ int m_sentinel16;                               // = -1 [UNRESOLVED]
    /* +20 */ int m_i20;                                      // = 0 [UNRESOLVED]
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

    /*  +0 */ int m_i0;                                       // = 0 [UNRESOLVED]
    /*  +4 */ char m_gap4[12];                                // never written
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
        : m_self0((Hash*)this)
        , m_i12(0)
        , m_i16(0)
        , m_self1((Hash*)this)
        , m_buckets(new Bucket[n + 1])
        , m_firstUsed(0)
        , m_count(0)
        , m_capacity((int)n)
        , m_nodeManager(0)
    {
        // m_i0 and m_i4 deliberately not initialised - see above.
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

    /*  +0 */ int m_i0;                  // never written by the constructor
    /*  +4 */ int m_i4;                  // never written by the constructor
    /*  +8 */ Hash* m_self0;             // = this
    /* +12 */ int m_i12;                 // = 0
    /* +16 */ int m_i16;                 // = 0
    /* +20 */ Hash* m_self1;             // = this
    // +0..+8 and +12..+20 are a walk position and its end mark (bucket, node, hash); see the
    // open-set refill. [INFERRED]
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

// State for one in-flight A* search. Its node manager and hash tables belong to the search resource.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
class AStarResource
{
public:
    explicit AStarResource(NodeManager* nodeManager)
        : m_status(0)
        , m_maxIterations(0)
        , m_maxSearchDepth(-1)
        , m_i16(0)
        , m_openSize(0)
        , m_hashSize(0)
        , m_i28(-1)
        , m_nodeManager(nodeManager)
        , m_i36(0)
        , m_open(0)
        , m_closed(0)
        , m_i48(0)
        , m_i52(0)
        , m_i56(0)
        , m_cache(0)
        , m_i64(0)
    {
        // m_i4 deliberately not initialised - see above.
    }

    // One A* step against a deadline. ELF: ClusterNode , QTSearchNode
    // CPNode , all `...AStarResource...6updateEd`. The
    // result is discarded by PathfindingEngine::update. A named stop until the search is read;
    // the definition follows the class.
    void update(double deadline);

    /*  +0 */ int m_status;          // = 0 `PathfindingEngine::getStatus` reads this word
    /*  +4 */ int m_i4;              // never written
    /*  +8 */ int m_maxIterations;   // = 0 [INFERRED] - `setMaxIterationsForResource` writes +8
    /* +12 */ int m_maxSearchDepth;  // = -1 [INFERRED] - `setMaxSearchDepthForResource` writes +12
    /* +16 */ int m_i16;             // = 0, set by the owner after the push
    /* +20 */ int m_openSize;        // = the int handed to BinarySearchTree's constructor
    /* +24 */ int m_hashSize;        // = the int handed to both Hash constructors
    /* +28 */ int m_i28;             // = -1 [UNRESOLVED]
    /* +32 */ NodeManager* m_nodeManager;
    /* +36 */ int m_i36;             // = 0 [UNRESOLVED]
    /* +40 */ BinarySearchTree<Node, SearchNode>* m_open;
    /* +44 */ Hash<Node, NodeManager, SearchNode>* m_closed;   // [INFERRED] which is which
    /* +48 */ int m_i48;             // = 0 [UNRESOLVED]
    /* +52 */ int m_i52;             // = 0 [UNRESOLVED]
    /* +56 */ int m_i56;             // = 0 [UNRESOLVED]
    /* +60 */ Hash<Node, NodeManager, SearchNode>* m_cache;    // [INFERRED]
    /* +64 */ int m_i64;             // = 0 [UNRESOLVED]
};

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
// Defined here because every instantiation needs it; reached only when a search is in flight.
template <class Node, class NodeManager, class SearchNode, class Key, class Path>
void AStarResource<Node, NodeManager, SearchNode, Key, Path>::update(double)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AStarResource::update");
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

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIPATHFINDINGSEARCHTYPES_H
