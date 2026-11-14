// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIStrategicObjectManager operations.

#include "AIStrategicObjectManager.h"

#include "AIStrategicArea.h"                          // createFromControlPoint constructs one
#include "SAWayPoint.h"                               // the destructor and postLoadInitAreas own them
#include "../AIUtils/checkPointer.h"
#include "../AIUtils/DebugRendering/AIRenderUtil.h"   // addStrategicObject's null-guarded tail
#include "../Main/AIDllFrontier.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Core/Debug.h"
#include <cmath>
#include "../../../../Math/Mat4.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../../../../Core/ConsoleObjectMacro.h"      // the aiStrategicArea.* binding at the foot

namespace dice {
namespace hfe {
namespace ai {

AIStrategicObjectManager* AIStrategicObjectManager::m_instance = 0;

// "Closest" is the greatest virtual gravity
// starting at -1, not a distance calculation here. Strict comparison preserves
// first-wins ties and ignores unordered results. Both ports reload the selected
// pointer after gravity and the vector end on each iteration; neither guards nulls.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(
    const Vec3& position, int side) const
{
    AIStrategicObject* closest = 0;
    float best = -1.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        const double gravity = (*it)->gravity(position, side);
        if (gravity > best)
        {
            best = static_cast<float>(gravity);
            closest = *it;
        }
    }
    return closest;
}

// PE 1008E6A0. As above, among the objects whose status for the side is zero when `neutral`
// is set, or non-zero when it is clear. [INFERRED] parameter name.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(const Vec3& position, int side,
                                                              bool neutral) const
{
    AIStrategicObject* closest = 0;
    float best = -1.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        const float gravity = (*it)->gravity(position, side);
        const int status = static_cast<int>((*it)->getStatus(side));
        const bool candidate = neutral ? status == 0 : status != 0;
        if (candidate && gravity > best)
        {
            best = gravity;
            closest = *it;
        }
    }
    return closest;
}

// WPContainer - , each.
// mov eax, this
// test a2, a2
// jz ret ; -> this
// lea eax, [this+12] ; -> this+12
// so any non-zero selects the second list. Written literally.

std::vector<SAWayPoint*>& AIStrategicObjectManager::WPContainer::getWayPoints(int type)
{
    if (type)
        return m_wayPointsB;

    return m_wayPointsA;
}

const std::vector<SAWayPoint*>& AIStrategicObjectManager::WPContainer::getWayPoints(int type) const
{
    if (type)
        return m_wayPointsB;

    return m_wayPointsA;
}

// Initializes strategic-area collections and the active selection.

AIStrategicObjectManager::AIStrategicObjectManager()
    : m_areasInitialised(false)
    , m_b84(false)
    , m_wayPointsToDrawFirst(-1)
    , m_wayPointsToDrawSecond(-1)
{
    m_strategicObjects.resize(0);
    m_objectsByCategory.resize(0);

    if (!m_instance)
        m_instance = this;

    m_activeArea = m_strategicAreas.end();

    m_categoryNames.resize(0);
}

// Deletes owned areas, waypoints and bridges, and clears the singleton if it refers to this object.

AIStrategicObjectManager::~AIStrategicObjectManager()
{
    if (m_instance == this)
        m_instance = 0;

    for (std::map<std::string, AIStrategicArea*>::iterator area = m_strategicAreas.begin();
         area != m_strategicAreas.end(); )
    {
        if (area->second)
            delete area->second;

        m_strategicAreas.erase(area++);
    }

    for (std::map<std::pair<int, int>, WPContainer>::iterator entry = m_wayPoints.begin();
         entry != m_wayPoints.end(); ++entry)
    {
        std::vector<SAWayPoint*>& first = entry->second.getWayPoints(0);

        for (std::vector<SAWayPoint*>::iterator point = first.begin();
             point != first.end(); ++point)
        {
            if (*point)
                delete *point;
        }

        std::vector<SAWayPoint*>& second = entry->second.getWayPoints(1);

        for (std::vector<SAWayPoint*>::iterator point = second.begin();
             point != second.end(); ++point)
        {
            if (*point)
                delete *point;
        }
    }

    deleteBridges();
}

// Initializes areas and waypoints after level loading, before pathfinding-map initialization.
// Waypoint containers use an unordered area pair as their key.

void AIStrategicObjectManager::postLoadInitAreas()
{
    WPContainer prototype;

    for (std::map<std::string, AIStrategicArea*>::iterator entry = m_strategicAreas.begin();
         entry != m_strategicAreas.end(); ++entry)
    {
        AIStrategicArea* const area = entry->second;

        area->postLoadInit();

        for (std::vector<AIStrategicObject*>::iterator neighbour = area->m_neighbours.begin();
             neighbour != area->m_neighbours.end(); ++neighbour)
        {
            const int mine  = area->m_index;
            const int other = (*neighbour)->m_index;

            const int low  = (mine < other)  ? mine : other;
            const int high = (mine >= other) ? mine : other;

            m_wayPoints.insert(
                std::pair<std::pair<int, int>, WPContainer>(std::pair<int, int>(low, high),
                                                            prototype));
        }
    }

    for (std::map<std::pair<int, int>, WPContainer>::iterator entry = m_wayPoints.begin();
         entry != m_wayPoints.end(); ++entry)
    {
        std::vector<SAWayPoint*>& first = entry->second.getWayPoints(0);

        for (std::vector<SAWayPoint*>::iterator point = first.begin();
             point != first.end(); ++point)
        {
            (*point)->init();
        }

        std::vector<SAWayPoint*>& second = entry->second.getWayPoints(1);

        for (std::vector<SAWayPoint*>::iterator point = second.begin();
             point != second.end(); ++point)
        {
            (*point)->init();
        }
    }
}

// The emitted loop walks the raw node chain and frees each `BridgeSegment*` with a bare
// `operator delete` - no destructor call, which is what `delete` compiles to for a trivially
// destructible type and is how `BridgeSegment` was pinned as a plain record. It then calls
// `_List_base::__clear`, which in this libstdc++ both frees the nodes and re-links the header, so
// the source is `clear` and calling it twice (here and again from `~list`) is safe.

void AIStrategicObjectManager::deleteBridges()
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        delete *i;
    }

    m_bridgeSegments.clear();
}

// Tracks each announced AI-enabled bridge segment by object pointer. Lookup stops at
// the first match; unknown objects are ignored by update and removal operations.
void AIStrategicObjectManager::createBridgeSegment(IAIObject* object)
{
    BridgeSegment* const segment = new BridgeSegment;   // zeroes the position only - +4 stays unset
    segment->m_object = object;
    segment->m_destroyed = false;
    // The position is zeroed again, as its own statement: the PE stores it a second time outside
    // the new-expression's null-checked constructor block, the ELF copies a zeroed 12-byte temporary.
    const Vec3 origin = { 0.0f, 0.0f, 0.0f };
    segment->m_position = origin;
    m_bridgeSegments.push_back(segment);
}

// The record is freed with a bare delete (a plain struct), then its node is erased.
void AIStrategicObjectManager::removeBridgeSegment(IAIObject* object)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            delete *i;
            m_bridgeSegments.erase(i);
            return;
        }
    }
}

void AIStrategicObjectManager::bridgeSegmentDestroyed(IAIObject* object, int value)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            (*i)->m_destroyed = true;
            (*i)->m_i4 = value;
            return;
        }
    }
}

// Only the flag is cleared; +4 keeps the value the destruction wrote.
void AIStrategicObjectManager::bridgeSegmentRepaired(IAIObject* object)
{
    for (std::list<BridgeSegment*>::iterator i = m_bridgeSegments.begin();
         i != m_bridgeSegments.end(); ++i)
    {
        if ((*i)->m_object == object)
        {
            (*i)->m_destroyed = false;
            return;
        }
    }
}
// [FRONTIER] Reset operations require the remaining strategic-area lifecycle behavior.
// [RETIRE-WHEN: aidll-02-skeleton] Implement and validate area resets and collection rebuilding.

namespace {

// The engine's Vec3 normalize as the AI image inlines it (PE 1006EEC0): a squared length already
// within FLT_EPSILON of one is left alone, a near-zero vector becomes zero.
void normalizeInPlace(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSq - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSq) < 1.1920929e-07f)
    {
        v.x = v.y = v.z = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSq);
    v.x *= scale;
    v.y *= scale;
    v.z *= scale;
}

// The side-one information of a bridge segment's object; null for a stale handle.
Information* segmentInformation(IAIObject* object)
{
    return informationManager->getInformation(object->getHandle(1));
}

} // namespace

// Marks every bridge segment intact and recomputes its direction: towards the first other segment
// whose centre lies within (2r + 5)^2 of this one's (r its physical radius), else the object's own
// third transform axis. PE 1008FA80; the direction role of m_position is [INFERRED].
void AIStrategicObjectManager::resetBridges()
{
    for (std::list<BridgeSegment*>::iterator it = m_bridgeSegments.begin(); it != m_bridgeSegments.end(); ++it)
    {
        BridgeSegment* const segment = *it;
        segment->m_destroyed = false;

        Information* const information = segmentInformation(segment->m_object);
        const Vec3 centre = information->getPosition();
        const float reach = information->getPhysical()->getRadius() * 2.0f + 5.0f;
        const float reachSq = reach * reach;

        bool found = false;
        for (std::list<BridgeSegment*>::iterator other = m_bridgeSegments.begin();
             other != m_bridgeSegments.end(); ++other)
        {
            const Vec3& position = segmentInformation((*other)->m_object)->getPosition();
            Vec3 delta;
            delta.x = position.x - centre.x;
            delta.y = position.y - centre.y;
            delta.z = position.z - centre.z;
            const float distanceSq = delta.y * delta.y + delta.z * delta.z + delta.x * delta.x;
            if (distanceSq > 0.0099999998f && distanceSq < reachSq)
            {
                normalizeInPlace(delta);
                segment->m_position = delta;
                found = true;
                break;
            }
        }
        if (!found)
        {
            const Mat4& transform = segment->m_object->getTransformation();
            segment->m_position.x = transform.m[8];
            segment->m_position.y = transform.m[9];
            segment->m_position.z = transform.m[10];
        }
    }
}

bool AIStrategicObjectManager::reset()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicObjectManager::reset");
}

// Lowers the initialised flag and restores every area's side statuses. PE 10090ED0.
bool AIStrategicObjectManager::resetAreas()
{
    m_areasInitialised = false;
    for (std::map<std::string, AIStrategicArea*>::iterator it = m_strategicAreas.begin();
         it != m_strategicAreas.end(); ++it)
        it->second->resetStatus();
    return true;
}

// The accessors that are two or three instructions. Each is transcribed from its own body; none of
// them needs a type this file does not have.
// `BF2AI::clear` destroys the object through this rather than through the
// static, which is the only reason it exists.
AIStrategicObjectManager* AIStrategicObjectManager::getInstance()
{
    return m_instance;
}

// - `return (char *)this + 4;`
const std::vector<AIStrategicObject*>& AIStrategicObjectManager::getStrategicObjects() const
{
    return m_strategicObjects;
}

// - the byte difference shifted right by two, i.e. the element count.
int AIStrategicObjectManager::getNumberOfStrategicObjects() const
{
    return (int)m_strategicObjects.size();
}

// Returns the category's object vector. The caller must supply a valid category index.
const std::vector<AIStrategicObject*>&
AIStrategicObjectManager::getStrategicObjectsOfCategory(int category) const
{
    return m_objectsByCategory[category];
}

// - same shape, four bytes per `std::string`, again unchecked.
const std::string& AIStrategicObjectManager::getCategoryName(int category) const
{
    return m_categoryNames[category];
}

int AIStrategicObjectManager::getNumberOfCategories()
{
    return (int)m_categoryNames.size();
}

// - `return (char *)this + 32;`
const std::map<std::string, AIStrategicArea*>& AIStrategicObjectManager::getStrategicAreas() const
{
    return m_strategicAreas;
}

// Returns null while no area is active - `m_activeArea` starts at `end` and
// the constructor is the only thing that puts it there.
AIStrategicArea* AIStrategicObjectManager::getActiveArea()
{
    if (m_activeArea != m_strategicAreas.end())
        return m_activeArea->second;

    return 0;
}

// This one checks both ends, and returns null rather than clamping.
AIStrategicObject* AIStrategicObjectManager::getStrategicObject(int index) const
{
    if (index < 0)
        return 0;

    if (index >= (int)m_strategicObjects.size())
        return 0;

    return m_strategicObjects[index];
}

// Disables waypoint drawing by clearing the first key component; leaves the second unchanged.
void AIStrategicObjectManager::clearWaypointToDraw()
{
    m_wayPointsToDrawFirst = -1;
}

// Undraws every waypoint of both lists of every container. PE 1008F620.
void AIStrategicObjectManager::undrawWayPoints() const
{
    for (std::map<std::pair<int, int>, WPContainer>::const_iterator it = m_wayPoints.begin();
         it != m_wayPoints.end(); ++it)
    {
        const std::vector<SAWayPoint*>& first = it->second.getWayPoints(0);
        for (std::vector<SAWayPoint*>::const_iterator wp = first.begin(); wp != first.end(); ++wp)
            (*wp)->undraw();
        const std::vector<SAWayPoint*>& second = it->second.getWayPoints(1);
        for (std::vector<SAWayPoint*>::const_iterator wp = second.begin(); wp != second.end(); ++wp)
            (*wp)->undraw();
    }
}

// Returns the category id for an existing name. The caller must ensure that exact
// name exists; merely having a nonempty category map is insufficient.
int AIStrategicObjectManager::getCategoryFromName(const std::string& name) const
{
    return m_categories.find(name)->second;
}

// Adds an object to the category table and assigns its index. The object and category must be valid.
// Other category rows retain null entries at that index.
void AIStrategicObjectManager::addStrategicObject(AIStrategicObject* object)
{
    const int index = (int)m_strategicObjects.size();

    m_strategicObjects.resize(index + 1);
    m_strategicObjects[index] = object;
    object->m_index = index;

    for (int category = 0; category < (int)m_objectsByCategory.size(); ++category)
    {
        m_objectsByCategory[category].resize(index + 1);

        if (category == object->m_category)
            m_objectsByCategory[category][index] = object;
        else
            m_objectsByCategory[category][index] = 0;
    }

    if (AIRenderUtil::m_instance)
        AIRenderUtil::m_instance->updateNumberOfStrategicAreas();
}

// Initializes areas with explicit bounds. Control-point areas wait for postLoadInit().
void AIStrategicObjectManager::initAreas()
{
    for (std::map<std::string, AIStrategicArea*>::iterator i = m_strategicAreas.begin();
         i != m_strategicAreas.end(); ++i)
    {
        if (!i->second->m_fromControlPoint)
            i->second->init();
    }
}

// Creates a strategic area from a named control point and selects it.
// Returns false and clears the active selection when creation is refused.
bool AIStrategicObjectManager::createFromControlPoint(const std::string& name,
                                                      unsigned controlPointId,
                                                      float radius,
                                                      const std::string& category)
{
    if (m_strategicAreas.find(name) != m_strategicAreas.end())
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    AIStrategicArea* const area = checkPointer(new AIStrategicArea(name, controlPointId, radius));
    if (!area)
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    if (getNumberOfCategories() <= 0)
        area->m_category = 0;
    else
        area->m_category = getCategoryFromName(category);

    AIStrategicObjectManager::getInstance()->addStrategicObject(area);

    m_strategicAreas[name] = area;
    m_areaNames[area] = name;
    m_activeArea = m_strategicAreas.find(name);
    return true;
}

// PE 1008E720. The object with the greatest positive gravity at the position among those that
// allow the equipment's vehicle group and do not expel its type; an area that carries the vehicle
// number in m_i340 is skipped unless the number is -1. Null when no gravity exceeds zero.
AIStrategicObject* AIStrategicObjectManager::getClosestObject(const Vec3& position,
                                                              int equipmentType,
                                                              int vehicleNumber, int side) const
{
    const int vehicleGroup = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)
                                 ->getVehicleGroup(equipmentType);
    AIStrategicObject* best = 0;
    float bestGravity = 0.0f;
    for (std::vector<AIStrategicObject*>::const_iterator it = m_strategicObjects.begin();
         it != m_strategicObjects.end(); ++it)
    {
        if (!(*it)->allowsVehicleGroup(vehicleGroup))
            continue;
        if ((*it)->expelsUnitType(equipmentType))
            continue;
        if ((*it)->getType() == 0 && vehicleNumber != -1
            && static_cast<AIStrategicArea*>(*it)->hasFlag340(vehicleNumber))
            continue;
        const float gravity = (*it)->gravity(position, side);
        if (gravity > bestGravity)
        {
            bestGravity = gravity;
            best = *it;
        }
    }
    return best;
}

// Creates an explicit-bounds strategic area and selects it. PE 10093990.
// Returns false and clears the active selection for a duplicate name or a failed allocation.
bool AIStrategicObjectManager::create(std::string name, Vec2 firstCorner, Vec2 secondCorner,
                                      float height, float radius, const std::string& category)
{
    if (m_strategicAreas.find(name) != m_strategicAreas.end())
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    AIStrategicArea* const area = checkPointer(
        new AIStrategicArea(name, firstCorner, secondCorner, height, radius));
    if (!area)
    {
        m_activeArea = m_strategicAreas.end();
        return false;
    }

    if (getNumberOfCategories() <= 0)
        area->m_category = 0;
    else
        area->m_category = getCategoryFromName(category);

    AIStrategicObjectManager::getInstance()->addStrategicObject(area);

    m_strategicAreas[name] = area;
    m_areaNames[area] = name;
    m_activeArea = m_strategicAreas.find(name);
    return true;
}

// Selects the named area; an unknown name leaves no area selected. PE 10091220.
bool AIStrategicObjectManager::setActive(std::string name)
{
    m_activeArea = m_strategicAreas.find(name);
    return m_activeArea != m_strategicAreas.end();
}

// Links the selected area to a named area on the same layer. PE 10090FB0.
// The selected area is not checked against end; callers run this after setActive.
bool AIStrategicObjectManager::addNeighbour(std::string name)
{
    const std::map<std::string, AIStrategicArea*>::iterator other = m_strategicAreas.find(name);
    if (other == m_strategicAreas.end())
        return false;

    AIStrategicArea* const active = m_activeArea->second;
    if (active->m_layer != other->second->m_layer)
        return false;

    active->addNeighbour(other->second);
    other->second->addNeighbouringObject(active);
    return true;
}

// Sets one type bit on the selected area and returns the new flags, or -1 when no area is
// selected or the type is the parser's unknown value. PE 10091150.
int AIStrategicObjectManager::addObjectTypeFlag(int type)
{
    if (m_activeArea == m_strategicAreas.end() || type == 27)
        return -1;

    AIStrategicArea* const area = m_activeArea->second;
    area->m_objectTypeFlags |= 1 << type;
    return area->m_objectTypeFlags;
}

// Stores an order position on the selected area for a named pathfinding map. PE 100912D0.
bool AIStrategicObjectManager::setOrderPosition(std::string mapName, const Vec3& position)
{
    if (m_activeArea == m_strategicAreas.end() || !IAIPathfinding::instance)
        return false;

    const int map = IAIPathfinding::instance->getMapIdFromName(mapName);
    if (map == -1)
        return false;

    return m_activeArea->second->setOrderPosition(map, position);
}

// Adds a waypoint between two named areas for a named map. PE 10094E20.
// The pair key is ordered by area index; unknown names and identical areas are reported errors.
void AIStrategicObjectManager::addWayPoint(const Vec3& position, const std::string& mapName,
                                           const std::string& firstArea,
                                           const std::string& secondArea)
{
    const std::map<std::string, AIStrategicArea*>::iterator first =
        m_strategicAreas.find(firstArea);
    if (first == m_strategicAreas.end())
    {
        Debug(DebugTypeError, __FILE__, 1144, "")
            << firstArea << " is not the name of a valid strategic area.";
        return;
    }

    const std::map<std::string, AIStrategicArea*>::iterator second =
        m_strategicAreas.find(secondArea);
    if (second == m_strategicAreas.end())
    {
        Debug(DebugTypeError, __FILE__, 1151, "")
            << secondArea << " is not the name of a valid strategic area.";
        return;
    }

    AIStrategicArea* low;
    AIStrategicArea* high;
    if (first->second->m_index < second->second->m_index)
    {
        low = first->second;
        high = second->second;
    }
    else
    {
        low = second->second;
        high = first->second;
    }

    if (low == high)
    {
        Debug(DebugTypeError, __FILE__, 1171, "")
            << "Must have two different strategic areas when adding waypoints.";
        return;
    }

    const std::pair<std::map<std::pair<int, int>, WPContainer>::iterator, bool> entry =
        m_wayPoints.insert(std::make_pair(std::make_pair(low->m_index, high->m_index),
                                          WPContainer()));

    const int map = IAIPathfinding::instance->getMapIdFromName(mapName);
    SAWayPoint* const wayPoint = checkPointer(new SAWayPoint(position, map));
    entry.first->second.getWayPoints(map).push_back(wayPoint);
}

// Console bindings for strategic-area configuration.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

namespace {

const char* const kAiStrategicAreaConsoleTag = "aiStrategicArea";

void consoleAiStrategicAreaCreateFromControlPoint(const std::string& name,
                                                  unsigned int controlPointId,
                                                  float radius)
{
    // No null check on the singleton, matching every other console forwarder in this DLL:
    // `BF2AI::init` builds the manager as step 17 during `initDll`, and `GameLogic::load` runs
    // `StrategicAreas.ai` long afterwards. The `isObjectActive` test below is the real guard.
    AIStrategicObjectManager::getInstance()->createFromControlPoint(
        name, controlPointId, radius, std::string());   // [CHOSEN] empty category - see above
}

AIStrategicObjectManager* consoleAiStrategicAreaManager()
{
    return AIStrategicObjectManager::getInstance();
}

// The property bindings are active only while an area is selected, so these never see null.
AIStrategicArea* consoleAiStrategicAreaActive()
{
    return AIStrategicObjectManager::getInstance()->getActiveArea();
}

bool consoleAiStrategicAreaHasActive()
{
    return AIStrategicObjectManager::getInstance() != 0
        && AIStrategicObjectManager::getInstance()->getActiveArea() != 0;
}

bool consoleAiStrategicAreaCreate(const std::string& name, Vec2 firstCorner, Vec2 secondCorner,
                                  float height, float radius, const std::string& category)
{
    return consoleAiStrategicAreaManager()->create(name, firstCorner, secondCorner, height, radius,
                                                   category);
}

bool consoleAiStrategicAreaSetActive(const std::string& name)
{
    return consoleAiStrategicAreaManager()->setActive(name);
}

bool consoleAiStrategicAreaAddNeighbour(const std::string& name)
{
    return consoleAiStrategicAreaManager()->addNeighbour(name);
}

int consoleAiStrategicAreaAddObjectTypeFlag(AIStrategicObject::StrategicObjectType type)
{
    return consoleAiStrategicAreaManager()->addObjectTypeFlag(type);
}

bool consoleAiStrategicAreaSetOrderPosition(const std::string& map, const Vec3& position)
{
    return consoleAiStrategicAreaManager()->setOrderPosition(map, position);
}

void consoleAiStrategicAreaAddWayPoint(const Vec3& position, const std::string& map,
                                       const std::string& firstArea, const std::string& secondArea)
{
    consoleAiStrategicAreaManager()->addWayPoint(position, map, firstArea, secondArea);
}

void consoleAiStrategicAreaSetLayer(int layer)            { consoleAiStrategicAreaActive()->m_layer = layer; }
void consoleAiStrategicAreaSetSide(int side)              { consoleAiStrategicAreaActive()->setSide(side); }
void consoleAiStrategicAreaSetVehicleSearchRadius(float r)
{
    consoleAiStrategicAreaActive()->m_vehicleSearchRadius = r;
}

} // anonymous namespace

BF2_CONSOLE_OBJECT("aiStrategicArea", 2, 0)
// Registration order follows the PE console-class constructors.
BF2_CONSOLE_METHOD_5_6_RET_ACTIVE(create, consoleAiStrategicAreaCreate, bool,
                                  AIStrategicObjectManager::getInstance() != 0,
                                  std::string, Vec2, Vec2, float, float, std::string, std::string(),
                                  kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_3(createFromControlPoint, consoleAiStrategicAreaCreateFromControlPoint,
                     AIStrategicObjectManager::getInstance() != 0,
                     std::string, unsigned int, float, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setActive, consoleAiStrategicAreaSetActive, bool,
                                AIStrategicObjectManager::getInstance() != 0,
                                std::string, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addNeighbour, consoleAiStrategicAreaAddNeighbour, bool,
                                AIStrategicObjectManager::getInstance() != 0,
                                std::string, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_PROPERTY_FN(layer, int, consoleAiStrategicAreaHasActive(),
                        consoleAiStrategicAreaActive()->m_layer,
                        consoleAiStrategicAreaSetLayer, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addObjectTypeFlag, consoleAiStrategicAreaAddObjectTypeFlag, int,
                                AIStrategicObjectManager::getInstance() != 0,
                                AIStrategicObject::StrategicObjectType, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_PROPERTY_FN(vehicleSearchRadius, float, consoleAiStrategicAreaHasActive(),
                        consoleAiStrategicAreaActive()->m_vehicleSearchRadius,
                        consoleAiStrategicAreaSetVehicleSearchRadius, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setOrderPosition, consoleAiStrategicAreaSetOrderPosition, bool,
                                AIStrategicObjectManager::getInstance() != 0,
                                std::string, Vec3, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_4(addWayPoint, consoleAiStrategicAreaAddWayPoint,
                     AIStrategicObjectManager::getInstance() != 0,
                     Vec3, std::string, std::string, std::string, kAiStrategicAreaConsoleTag)
BF2_CONSOLE_METHOD_1_ACTIVE(setSide, consoleAiStrategicAreaSetSide, consoleAiStrategicAreaHasActive(),
                            int, kAiStrategicAreaConsoleTag)

// Handed to the console registry by AIModule::init, beside the `ai` and `aiSettings` lists.
ConsoleObjectList& aiStrategicAreaConsoleObjects()
{
    return consoleBindingList(kAiStrategicAreaConsoleTag);
}

} // namespace ai
} // namespace hfe
} // namespace dice
