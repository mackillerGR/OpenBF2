// Cluster record used by the navigation graph. Loading initializes its stored data.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H
#define AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H

namespace dice {
namespace hfe {
namespace ai {

class Cluster
{
public:
    Cluster() {}      // empty
    ~Cluster() {}     // empty

    /* +0x00 */ unsigned char  m_unread00[8];         // [UNRESOLVED] - see the note above
    /* +0x08 */ unsigned short m_firstPortal;         // getClusterPortal's base
    /* +0x0A */ unsigned short m_numberOfPortals;     // getClusterPortal's stride
    /* +0x0C */ unsigned short m_firstPortalCost;     // base of the portal-to-portal cost table
    /* +0x0E */ unsigned char  m_walkable;            // map-part walkability state
    /* +0x0F */ unsigned char  m_unread0F;            // [UNRESOLVED]

    bool isWalkable() const { return m_walkable != 0; }
};

typedef char ClusterIsTheFileRecord[sizeof(Cluster) == 16 ? 1 : -1];

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPATHFINDING_CLUSTER_CLUSTER_H
