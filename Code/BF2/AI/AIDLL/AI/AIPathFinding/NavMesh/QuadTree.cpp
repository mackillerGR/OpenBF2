// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining QuadTree operations.

#include "QuadTree.h"

#include <cmath>

#include "../Cluster/ClusterGraph.h"
#include "../Material/MaterialManager.h"
#include "../../AIUtils/checkPointer.h"                 // and, through it, Core/Debug.h
#include "../../DrawPrimitives/DPManager.h"             // IDPManager::deleteMesh
#include "../../Main/CommonSingletonServer.h"
#include "../../../../../IO/File/File.h"
#include "../../../../../Math/Mat4.h"
#include "../../../../../Math/Plane3.h"

namespace dice {
namespace hfe {
namespace ai {

// Initializes the island, bounds, array pointers, selected counts, timing values, and
// debug mesh. Size and node/vertex-index/neighbor counts remain unset until clean,
// which readFromFile calls before loading.
QuadTree::QuadTree(unsigned int island)
    : m_island(island)
    , m_numberOfPolygons(0)
    , m_numberOfNodePolygons(0)
    , m_numberOfVertices(0)
    , m_nodes(0)
    , m_polygons(0)
    , m_nodePolygons(0)
    , m_vertices(0)
    , m_vertexIndices(0)
    , m_neighbours(0)
    , m_array84(0)
    , m_array88(0)
    , m_double8C(0.0)
    , m_double94(0.0)
    , m_double9C(0.0)
    , m_doubleA4(0.0)
    , m_debugMesh(0)
{
    m_min.x = 0.0f;
    m_min.y = 0.0f;
    m_max.x = 0.0f;
    m_max.y = 0.0f;

    // m_size, m_numberOfNodes, m_numberOfVertexIndices and m_numberOfNeighbours deliberately not
    // initialised - both binaries step over them.
}

// clean, then the five vectors in reverse declaration order.
QuadTree::~QuadTree()
{
    clean();
}

// Releases mesh arrays, resets their counts and pointers, and removes debug drawing.
void QuadTree::clean()
{
    delete[] m_nodes;
    delete[] m_polygons;
    delete[] m_nodePolygons;
    delete[] m_vertices;
    delete[] m_vertexIndices;
    delete[] m_neighbours;

    m_numberOfNodes = 0;
    m_numberOfPolygons = 0;
    m_numberOfNodePolygons = 0;
    m_numberOfVertices = 0;
    m_numberOfVertexIndices = 0;
    m_numberOfNeighbours = 0;
    m_nodes = 0;
    m_polygons = 0;
    m_nodePolygons = 0;
    m_vertices = 0;
    m_vertexIndices = 0;
    m_neighbours = 0;

    delete[] m_array84;
    delete[] m_array88;
    m_array84 = 0;
    m_array88 = 0;

    for (unsigned int i = 0; i < m_destroyableMapParts.size(); ++i)
        delete m_destroyableMapParts[i];
    m_destroyableMapParts.clear();

    undraw();
}

// Reads a version-3 navigation tree. Unsupported versions leave the existing tree unchanged.
void QuadTree::readFromFile(io::File& file)
{
    int version = 0;
    file.read(version);

    if (version < 3)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp", 132, "")
            << "Quad-tree file is not up to date. Reload GTS-data end save new quadtree-files for this level.";
        return;
    }
    if (version > 3)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPathFinding/NavMesh/QuadTree.cpp", 137, "")
            << "Quad-tree file is newer than your build. Update to latest build.";
        return;
    }

    clean();

    file.read(&m_min, sizeof(m_min));
    file.read(&m_max, sizeof(m_max));
    file.read(m_size);

    file.read(m_numberOfNodes);
    m_nodes = checkPointer(new QuadTreeNode[m_numberOfNodes]);
    file.read(m_nodes, sizeof(QuadTreeNode) * m_numberOfNodes);

    file.read(m_numberOfPolygons);
    m_polygons = checkPointer(new Polygon[m_numberOfPolygons]);
    file.read(m_polygons, sizeof(Polygon) * m_numberOfPolygons);

    file.read(m_numberOfNodePolygons);
    m_nodePolygons = checkPointer(new unsigned int[m_numberOfNodePolygons]);
    file.read(m_nodePolygons, sizeof(unsigned int) * m_numberOfNodePolygons);

    file.read(m_numberOfVertices);
    m_vertices = checkPointer(new Vec3[m_numberOfVertices]());
    file.read(m_vertices, sizeof(Vec3) * m_numberOfVertices);

    file.read(m_numberOfVertexIndices);
    m_vertexIndices = checkPointer(new unsigned int[m_numberOfVertexIndices]);
    file.read(m_vertexIndices, sizeof(unsigned int) * m_numberOfVertexIndices);

    file.read(m_numberOfNeighbours);
    m_neighbours = checkPointer(new unsigned int[m_numberOfNeighbours]);
    file.read(m_neighbours, sizeof(unsigned int) * m_numberOfNeighbours);
}

// Finds a polygon for a position. Empty trees and points outside the island bounds return false.
bool QuadTree::findPolygon(const Vec3& position, unsigned int& polygon,
                           MaterialManager* materials, float allowedHeightDiff) const
{
    if (!m_nodes)
        return false;

    if (position.x < m_min.x || position.x > m_max.x
        || position.z < m_min.y || position.z > m_max.y)
        return false;

    return m_nodes[0].findPolygon(this, m_min, m_size, position, polygon, materials,
                                  allowedHeightDiff);
}

bool QuadTree::pointWithinPolygonEdges(Polygon* polygon, const Vec3& point) const
{
    Vec2 point2;
    point2.x = 0.0f;
    point2.y = 0.0f;
    vec3ToVec2XZ(point, point2);

    unsigned char edge = 0;
    if (polygon->m_numberOfVertices == 0)
        return true;

    do
    {
        Vec2 first;
        first.x = 0.0f;
        first.y = 0.0f;
        vec3ToVec2XZ(m_vertices[m_vertexIndices[polygon->m_firstIndex + edge]], first);

        Vec2 second;
        second.x = 0.0f;
        second.y = 0.0f;
        const unsigned char next = static_cast<unsigned char>(
            (edge + 1) % polygon->m_numberOfVertices);
        vec3ToVec2XZ(m_vertices[m_vertexIndices[polygon->m_firstIndex + next]], second);

        if ((second.y - first.y) * (point2.x - first.x)
            + (point2.y - first.y) * -(second.x - first.x) < -0.001f)
            return false;

        ++edge;
    }
    while (edge < polygon->m_numberOfVertices);

    return true;
}

bool QuadTree::withinPolygon(Polygon* polygon, const Vec3& point, float& distanceSquared,
                             float allowedHeightDiff) const
{
    const Vec3& first = m_vertices[m_vertexIndices[polygon->m_firstIndex]];

    Plane3 plane;
    plane.normal = polygon->m_normal;
    plane.d = -(first.y * polygon->m_normal.y
              + first.x * polygon->m_normal.x
              + first.z * polygon->m_normal.z);

    Vec3 lineEnd;
    lineEnd.x = point.x;
    lineEnd.y = point.y - 1.0f;
    lineEnd.z = point.z;

    Vec3 intersection;
    intersection.x = 0.0f;
    intersection.y = 0.0f;
    intersection.z = 0.0f;
    bool insideSegment;

    if (!plane.intersectLine(point, lineEnd, intersection, insideSegment)
        || !pointWithinPolygonEdges(polygon, intersection))
        return false;

    const float dx = point.x - intersection.x;
    const float dy = point.y - intersection.y;
    const float dz = point.z - intersection.z;
    distanceSquared = dz * dz + dy * dy + dx * dx;

    static const float maxDistanceAbove = 1.0f;
    const float limit = point.y > intersection.y ? allowedHeightDiff : maxDistanceAbove;
    return distanceSquared <= limit * limit;
}

bool QuadTree::isPointInPolygon(const Vec3& point, unsigned int polygon, float allowedHeightDiff,
                                MaterialManager* materials) const
{
    Polygon* const candidate = &m_polygons[polygon];
    // The shipped known-polygon gate is a positive `cost >= 0` test. Keep its unordered case:
    // unlike the leaf scan's rejection-form comparison, a NaN cost rejects this fast path.
    if (materials && !(materials->getMaterialCost(candidate->m_material) >= 0.0f))
        return false;

    float distanceSquared = 0.0f;
    return withinPolygon(candidate, point, distanceSquared, allowedHeightDiff);
}

void QuadTree::getVerticiesOfPolygon(std::vector<const Vec3*>& vertices,
                                     unsigned int polygon) const
{
    Polygon* const candidate = &m_polygons[polygon];
    unsigned char corner = 0;
    if (candidate->m_numberOfVertices == 0)
        return;

    do
    {
        vertices.push_back(&m_vertices[m_vertexIndices[candidate->m_firstIndex + corner]]);
        ++corner;
    }
    while (corner < candidate->m_numberOfVertices);
}

// Retail directly indexes the polygon array; there is no bounds or
// null guard between the caller-supplied polygon id and the material byte.
unsigned char QuadTree::getMaterialOfPolygon(unsigned int polygon) const
{
    return m_polygons[polygon].m_material;
}

Polygon* QuadTree::getPolygon(unsigned int polygon) const
{
    if (polygon < m_numberOfPolygons)
        return &m_polygons[polygon];
    return 0;
}

unsigned int QuadTree::getNeighbour(unsigned int polygon, unsigned char edge) const
{
    const Polygon& record = m_polygons[polygon];
    if (edge < record.m_numberOfVertices)
        return m_neighbours[record.m_firstIndex + edge];
    return static_cast<unsigned int>(-1);
}

bool QuadTree::getNeighbourEdge(unsigned char& edge, unsigned int from, unsigned int to) const
{
    const Polygon& record = m_polygons[from];
    edge = 0;
    while (m_neighbours[record.m_firstIndex + edge] != to)
    {
        ++edge;
        if (edge >= record.m_numberOfVertices)
            return false;
    }
    return true;
}

const Vec3& QuadTree::getVertexOfPolygon(unsigned int polygon, unsigned char corner) const
{
    return m_vertices[m_vertexIndices[m_polygons[polygon].m_firstIndex + corner]];
}

const Vec3& QuadTree::getWrappedVertexOfPolygon(unsigned int polygon, unsigned char corner) const
{
    const Polygon& record = m_polygons[polygon];
    return m_vertices[m_vertexIndices[record.m_firstIndex + corner % record.m_numberOfVertices]];
}

const Vec3& QuadTree::getNormalOfPolygon(unsigned int polygon) const
{
    return m_polygons[polygon].m_normal;
}

void QuadTree::setMaterialOfPolygon(unsigned int polygon, unsigned char material)
{
    m_polygons[polygon].m_material = material;
}

void QuadTree::getCentreOfPolygon(Vec3& centre, unsigned int polygon) const
{
    const Polygon& record = m_polygons[polygon];
    centre.x = 0.0f;
    centre.y = 0.0f;
    centre.z = 0.0f;
    for (unsigned char corner = 0; corner < record.m_numberOfVertices; ++corner)
    {
        const Vec3& vertex = m_vertices[m_vertexIndices[record.m_firstIndex + corner]];
        centre.x = vertex.x + centre.x;
        centre.y = vertex.y + centre.y;
        centre.z = vertex.z + centre.z;
    }
    const float scale = 1.0f / static_cast<float>(record.m_numberOfVertices);
    centre.x = scale * centre.x;
    centre.y = scale * centre.y;
    centre.z = scale * centre.z;
}

namespace {

// PE 1014B030: where the line through p1 and p2 meets the line through a and b, as the
// parameter along p1->p2 and the parameter along a->b. False for parallel lines.
bool intersectLines(const Vec2& p1, const Vec2& p2, const Vec2& a, const Vec2& b, float& along,
                    float& alongEdge)
{
    const float denominator = (b.y - a.y) * (p2.x - p1.x) - (p2.y - p1.y) * (b.x - a.x);
    if (denominator == 0.0f)
        return false;
    const float inverse = 1.0f / denominator;
    along = ((p1.y - a.y) * (b.x - a.x) - (p1.x - a.x) * (b.y - a.y)) * inverse;
    alongEdge = ((p1.y - a.y) * (p2.x - p1.x) - (p2.y - p1.y) * (p1.x - a.x)) * inverse;
    return true;
}

} // anonymous namespace

// The eighth shipped argument is never read and is dropped here.
bool QuadTree::getLineExitFromPolygon(Vec3& exit, unsigned int polygon, unsigned char& edge,
                                      unsigned int ignoredNeighbour, const Vec3& origin,
                                      const Vec3& direction, float* edgeDistance) const
{
    Vec2 start;
    start.x = origin.x;
    start.y = origin.z;
    const Polygon& record = m_polygons[polygon];
    float nearestMiss = 99999.0f;
    float nearestAlong = 0.0f;
    Vec2 nearestEnd;
    nearestEnd.x = 0.0f;
    nearestEnd.y = 0.0f;
    for (edge = 0; edge < record.m_numberOfVertices; ++edge)
    {
        if (ignoredNeighbour != static_cast<unsigned int>(-1)
            && ignoredNeighbour == m_neighbours[record.m_firstIndex + edge])
            continue;
        const Vec3& cornerA = m_vertices[m_vertexIndices[record.m_firstIndex + edge]];
        Vec2 a;
        a.x = cornerA.x;
        a.y = cornerA.z;
        const Vec3& cornerB = m_vertices[m_vertexIndices[
            record.m_firstIndex + static_cast<unsigned char>(static_cast<unsigned char>(edge + 1)
                                                             % record.m_numberOfVertices)]];
        Vec2 b;
        b.x = cornerB.x;
        b.y = cornerB.z;
        Vec2 end;
        end.x = direction.x + origin.x;
        end.y = direction.z + origin.z;
        float along;
        float alongEdge;
        if (!intersectLines(start, end, a, b, along, alongEdge))
            continue;
        if (along < 0.0f || alongEdge < 0.0f || alongEdge > 1.0f)
        {
            float miss = 9999.0f;
            if (along < 0.0f)
                miss = std::fabs(along);
            bool settled = false;
            if (alongEdge < 0.0f)
            {
                const float before = -alongEdge;
                if (before < miss)
                {
                    miss = before;
                    settled = true;
                }
            }
            if (!settled && alongEdge > 1.0f)
            {
                const float beyond = alongEdge - 1.0f;
                if (beyond < miss)
                    miss = beyond;
            }
            if (miss < 0.001f && miss < nearestMiss)
            {
                nearestMiss = miss;
                nearestAlong = along;
                nearestEnd = end;
            }
            continue;
        }
        // A crossing at either end of the edge only counts when the ray heads out through it.
        if (alongEdge <= 0.001f || !(alongEdge < 0.999f))
        {
            const float outward = -(b.x - a.x) * direction.z + (b.y - a.y) * direction.x;
            if (!(outward < 0.0f))
                continue;
        }
        const float rest = 1.0f - along;
        exit.x = end.x * along + start.x * rest;
        exit.y = 0.0f;
        exit.z = end.y * along + start.y * rest;
        const float dx = exit.x - start.x;
        const float dz = exit.z - start.y;
        exit.y = std::sqrt(dz * dz + dx * dx) * direction.y + origin.y;
        if (edgeDistance)
        {
            const float ex = a.x - b.x;
            const float ez = a.y - b.y;
            const float length = std::sqrt(ez * ez + ex * ex);
            float nearer = 1.0f - alongEdge;
            if (alongEdge < nearer)
                nearer = alongEdge;
            *edgeDistance = nearer * length;
        }
        return true;
    }
    if (!(nearestMiss < 0.001f))
        return false;
    const float rest = 1.0f - nearestAlong;
    exit.x = nearestEnd.x * nearestAlong + start.x * rest;
    exit.y = 0.0f;
    exit.z = nearestEnd.y * nearestAlong + start.y * rest;
    const float dx = exit.x - start.x;
    const float dz = exit.z - start.y;
    exit.y = std::sqrt(dz * dz + dx * dx) * direction.y + origin.y;
    return true;
}

// Tracks polygons affected by destroyable map parts and restores their original materials.
void QuadTree::getPolygonsWithinBoundary(const Vec3& minimum, const Vec3& maximum,
                                         const Mat4& transformation,
                                         std::vector<unsigned int>& polygons) const
{
    Mat4 inverse;
    fullInvert(transformation, inverse);

    for (unsigned int polygonId = 0; polygonId < m_numberOfPolygons; ++polygonId)
    {
        const Polygon& polygon = m_polygons[polygonId];
        for (unsigned int corner = 0; corner < polygon.m_numberOfVertices; ++corner)
        {
            const Vec3& vertex = m_vertices[m_vertexIndices[polygon.m_firstIndex + corner]];

            Vec3 local;
            local.x = inverse.m[0] * vertex.x + inverse.m[4] * vertex.y
                    + inverse.m[8] * vertex.z + inverse.m[12];
            local.y = inverse.m[1] * vertex.x + inverse.m[5] * vertex.y
                    + inverse.m[9] * vertex.z + inverse.m[13];
            local.z = inverse.m[2] * vertex.x + inverse.m[6] * vertex.y
                    + inverse.m[10] * vertex.z + inverse.m[14];

            if (local.x >= minimum.x && local.x <= maximum.x
                && local.y >= minimum.y && local.y <= maximum.y
                && local.z >= minimum.z && local.z <= maximum.z)
            {
                polygons.push_back(polygonId);
                break;
            }
        }
    }
}

void QuadTree::getPolygonsWithinBoundaryDoubles(const Vec3& minimum, const Vec3& maximum,
                                                const Mat4& transformation,
                                                std::vector<unsigned int>& polygons) const
{
    Mat4 inverse;
    fullInvert(transformation, inverse);

    for (unsigned int polygonId = 0; polygonId < m_numberOfPolygons; ++polygonId)
    {
        const Polygon& polygon = m_polygons[polygonId];
        for (unsigned int corner = 0; corner < polygon.m_numberOfVertices; ++corner)
        {
            const DoubleVec3& vertex = m_array84[m_vertexIndices[polygon.m_firstIndex + corner]];

            Vec3 local;
            local.x = static_cast<float>(inverse.m[0] * vertex.x + inverse.m[4] * vertex.y
                                       + inverse.m[8] * vertex.z + inverse.m[12]);
            local.y = static_cast<float>(inverse.m[1] * vertex.x + inverse.m[5] * vertex.y
                                       + inverse.m[9] * vertex.z + inverse.m[13]);
            local.z = static_cast<float>(inverse.m[2] * vertex.x + inverse.m[6] * vertex.y
                                       + inverse.m[10] * vertex.z + inverse.m[14]);

            if (local.x >= minimum.x && local.x <= maximum.x
                && local.y >= minimum.y && local.y <= maximum.y
                && local.z >= minimum.z && local.z <= maximum.z)
            {
                polygons.push_back(polygonId);
                break;
            }
        }
    }
}

void QuadTree::initializeDestroyableMapPart(int mapPartId, bool initiallyWalkable,
                                            const Vec3& minimum, const Vec3& maximum,
                                            const Mat4& transformation, ClusterGraph* graph,
                                            bool useDoubles)
{
    const bool translationIsZero = transformation.m[12] == 0.0f
                                && transformation.m[13] == 0.0f
                                && transformation.m[14] == 0.0f;

    DestroyableMapPart* const part = checkPointer(new DestroyableMapPart);
    if (useDoubles)
        getPolygonsWithinBoundaryDoubles(minimum, maximum, transformation, part->m_polygons);
    else
        getPolygonsWithinBoundary(minimum, maximum, transformation, part->m_polygons);

    if (translationIsZero || part->m_polygons.empty())
    {
        part->m_originalMaterial = 0;
        m_destroyableMapParts.push_back(part);
        part->m_initiallyWalkable = initiallyWalkable;
        part->m_destroyed = false;
        return;
    }

    part->m_originalMaterial = m_polygons[part->m_polygons[0]].m_material;
    if (!part->m_originalMaterial)
    {
        for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
        {
            const unsigned char material = m_polygons[part->m_polygons[i]].m_material;
            if (material)
            {
                part->m_originalMaterial = material;
                break;
            }
        }
    }

    m_destroyableMapParts.push_back(part);
    part->m_initiallyWalkable = initiallyWalkable;
    if (!initiallyWalkable && !useDoubles)
        restoreMapPart(mapPartId, graph);
    part->m_destroyed = false;
}

void QuadTree::destroyMapPart(int mapPartId, ClusterGraph* graph)
{
    if (!m_destroyableMapParts.empty())
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part && !part->m_destroyed)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
            {
                Polygon& polygon = m_polygons[part->m_polygons[i]];
                polygon.m_material = part->m_initiallyWalkable ? 0 : part->m_originalMaterial;
            }

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(!part->m_initiallyWalkable);
            }
            part->m_destroyed = true;
        }
    }
}

void QuadTree::restoreMapPart(int mapPartId, ClusterGraph* graph)
{
    if (mapPartId != -1)
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
            {
                Polygon& polygon = m_polygons[part->m_polygons[i]];
                polygon.m_material = part->m_initiallyWalkable ? part->m_originalMaterial : 0;
            }

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(part->m_initiallyWalkable);
            }
            part->m_destroyed = false;
        }
    }
}

void QuadTree::resetOriginalMaterialOfDestroyableMapPart(int mapPartId, ClusterGraph* graph)
{
    if (mapPartId != -1)
    {
        DestroyableMapPart* const part = m_destroyableMapParts[mapPartId];
        if (part)
        {
            for (unsigned int i = 0; i < part->m_polygons.size(); ++i)
                m_polygons[part->m_polygons[i]].m_material = part->m_originalMaterial;

            if (graph && !part->m_polygons.empty())
            {
                const unsigned int first = part->m_polygons[0];
                graph->m_clusters[m_polygons[first].m_cluster].m_walkable
                    = static_cast<unsigned char>(part->m_initiallyWalkable);
            }
            part->m_destroyed = false;
        }
    }
}

void QuadTree::resetDestroyableMapParts(ClusterGraph* graph)
{
    for (unsigned int mapPartId = 0; mapPartId < m_destroyableMapParts.size(); ++mapPartId)
        restoreMapPart(mapPartId, graph);
}

// QuadTree::undraw - , (const); inlined into the PE's clean.
// Hands the debug mesh to IDPManager::deleteMesh (ELF slot +0x50, PE +0x4C) when the draw-primitive
// manager exists; deleteMesh takes the handle by reference. The bool it answers is discarded.
void QuadTree::undraw() const
{
    if (CommonSingletonServer::m_iDPManager)
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteMesh(m_debugMesh);
}

} // namespace ai
} // namespace hfe
} // namespace dice
