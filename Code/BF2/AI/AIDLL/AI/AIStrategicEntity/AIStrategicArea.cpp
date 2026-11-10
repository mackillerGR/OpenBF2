// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining AIStrategicArea operations.

#include "AIStrategicArea.h"

#include <cmath>                                          // the two centre-to-corner distances

#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"                  // IAISettings::getNumberOfVehicleGroups
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/Strategy.h"                               // updateTemperature modifiers
#include "AIStrategicObjectManager.h"
#include "../AIBot/SquadManager.h"
#include "../AIBotBehaviour/CustomCurve.h"
#include "../AIStrategic/SAI.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h"       // IAIGame - the control-point queries
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"    // IAITerrain::getTerrainAltitude
#include "../../../../Math/Vec3.h"                         // vec3ToVec2XZ / vec2ToVec3XZ

namespace dice {
namespace hfe {
namespace ai {

// Creates a control-point area with an unset-height sentinel and one record per pathfinding map.

AIStrategicArea::AIStrategicArea(const std::string& name, unsigned int controlPointId, float radius)
    : AIStrategicObject(radius, -666.0f)
    , m_list268()
    , m_centre()
    , m_mapData(checkPointer(new MapData[IAIPathfinding::instance->getNumberOfMaps()]))
    , m_neighbours()
    , m_neighbouringObjects()
    , m_name(name)
    , m_objectTypeFlags(0)
    , m_controlPointId(controlPointId)
    , m_landingZones()
    , m_i340(0)
    , m_b358(false)
{
    // The tail. Everything above this point lands before the three list members are constructed, so
    // it cannot be body code; everything here lands after them, so it cannot be an initialiser.
    m_layer = -1;
    m_b404 = true;
    m_statusChanged = false;
    m_i416 = 0;
    m_i420 = 0;
    m_i424 = 0;
    m_fromControlPoint      = true;    // the phase selector - see the header
    m_orderPosesInitialised = false;
}

// PE 100E8840. Same member initialization as the control-point form except for the centre, the
// invalid control point id and the cleared phase selector, then the shared constructor tail.
AIStrategicArea::AIStrategicArea(const std::string& name, Vec2 firstCorner, Vec2 secondCorner,
                                 float height, float radius)
    : AIStrategicObject(radius, height)
    , m_list268()
    , m_centre(firstCorner)
    , m_mapData(checkPointer(new MapData[IAIPathfinding::instance->getNumberOfMaps()]))
    , m_neighbours()
    , m_neighbouringObjects()
    , m_name(name)
    , m_objectTypeFlags(0)
    , m_controlPointId(0xFFFFFFFFu)
    , m_landingZones()
    , m_i340(0)
    , m_b358(false)
{
    m_layer = -1;
    m_b404 = true;
    m_statusChanged = false;
    m_i416 = 0;
    m_i420 = 0;
    m_i424 = 0;
    m_fromControlPoint      = false;
    m_orderPosesInitialised = false;

    internalConstructorCall(firstCorner, secondCorner);
}

// PE 100E8A20.
void AIStrategicArea::setSide(int side)
{
    if (side == 1)
    {
        m_status[0] = StatusMap::Owned;
        m_status[2] = StatusMap::Owned;
        m_status[1] = StatusMap::Hostile;
        m_status[3] = StatusMap::Hostile;
    }
    else if (side == 2)
    {
        m_status[0] = StatusMap::Hostile;
        m_status[2] = StatusMap::Hostile;
        m_status[1] = StatusMap::Owned;
        m_status[3] = StatusMap::Owned;
    }
    else
    {
        m_status[2] = StatusMap::Neutral;
        m_status[3] = StatusMap::Neutral;
    }
}

// PE 100E0490. After initOrderPoses the navmesh query is made with the map's previous offset,
// not the new position, and only its island result is kept; preserved as shipped.
bool AIStrategicArea::setOrderPosition(int map, const Vec3& position)
{
    if (!m_orderPosesInitialised)
    {
        m_mapData[map].m_offset = position;
        return true;
    }

    Vec3 areaPosition;
    areaPosition.x = 0.0f;
    areaPosition.y = 0.0f;
    areaPosition.z = 0.0f;
    vec2ToVec3XZ(m_position[0], areaPosition);
    areaPosition.y = m_f148;

    unsigned int polygon = 0;
    unsigned int island  = 0;
    IAIPathfinding::instance->getPolygon(m_mapData[map].m_offset, map, polygon, island, 1);

    MapData& data = m_mapData[map];
    data.m_offset.x = position.x - areaPosition.x;
    data.m_offset.y = position.y - areaPosition.y;
    data.m_offset.z = position.z - areaPosition.z;
    data.m_island = island;
    return true;
}

// PE 100E53F0.
void AIStrategicArea::addNeighbour(AIStrategicObject* neighbour)
{
    const int count = static_cast<int>(m_neighbours.size());
    for (int i = 0; i < count; ++i)
    {
        if (m_neighbours[i] == neighbour)
            return;
    }
    m_neighbours.push_back(neighbour);
}

// PE 100E5500. Grows by resize and then assigns the new last element.
void AIStrategicArea::addNeighbouringObject(AIStrategicObject* object)
{
    const int count = static_cast<int>(m_neighbouringObjects.size());
    for (int i = 0; i < count; ++i)
    {
        if (m_neighbouringObjects[i] == object)
            return;
    }
    m_neighbouringObjects.resize(count + 1);
    m_neighbouringObjects[count] = object;
}

// Eleven hundred bytes: the per-map array, four lists, the name, and whatever the twenty-eight
// unwritten slots own. Not reachable before shutdown - `AIStrategicObjectManager`'s destructor is
// the only caller and it already carries a guarded seam for its area delete loop.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]
AIStrategicArea::~AIStrategicArea()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::~AIStrategicArea");
}

// Returns type code zero. [UNRESOLVED] Its meaning is not established as a StrategicObjectType.
int AIStrategicArea::getType() const
{
    return 0;
}

// - one load. The flag word an `.ai` script fills through
// `aiStrategicArea.addObjectTypeFlag`.
int AIStrategicArea::getObjectTypeFlags() const
{
    return m_objectTypeFlags;
}

// Returns twice the center-to-side-corner distance along X or Z.
// Side indices are one-based and must be valid.
float AIStrategicArea::getXExtent(int side) const
{
    const float half = m_position[side - 1].x - m_centre.x;
    return half + half;
}

float AIStrategicArea::getZExtent(int side) const
{
    const float half = m_position[side - 1].y - m_centre.y;
    return half + half;
}

// The query's own result and
// its fourth (active) output are discarded; the local is seeded true only to be passed along.
// Any assigned id answers true, whatever the engine wrote; the singleton is not null-checked.
bool AIStrategicArea::getControlPointPosAndRadius(Vec3& position, float& radius)
{
    bool active = true;
    if (m_controlPointId == static_cast<unsigned int>(-1))
        return false;
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
        ->getControlPointInfo(m_controlPointId, position, radius, active);
    return true;
}

// Resolves control-point position, height, and ownership after world objects are available.
void AIStrategicArea::postLoadInit()
{
    if (m_fromControlPoint)
    {
        IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);

        Vec3 controlPointPosition;
        controlPointPosition.x = 0.0f;
        controlPointPosition.y = 0.0f;
        controlPointPosition.z = 0.0f;

        float radius = 0.0f;
        bool  active = false;

        game->getControlPointInfo(m_controlPointId, controlPointPosition, radius, active);

        Vec2 flat;
        flat.x = 0.0f;
        flat.y = 0.0f;
        vec3ToVec2XZ(controlPointPosition, flat);

        m_centre = flat;
        m_centre.x -= radius;
        m_centre.y -= radius;

        m_f148 = controlPointPosition.y;

        internalConstructorCall(m_centre, flat);
        init();
    }

    initOrderPoses();

    if (hasObjectTypeFlag(ControlPoint))
    {
        if (m_controlPointId == 0xFFFFFFFFu)
        {
            IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);

            const int found = game->findControlPoint(m_centre, m_position[0]);
            m_controlPointId = static_cast<unsigned int>(found);

            if (found != -1)
            {
                const int owner = game->getOwnerOfControlPoint(m_controlPointId);

                if (owner == 0)
                {
                    m_status[0] = StatusMap::Neutral;
                    m_status[1] = StatusMap::Neutral;
                }
                else if (owner == 1)
                {
                    m_status[0] = StatusMap::Owned;
                    m_status[1] = StatusMap::Hostile;
                }
                else
                {
                    m_status[0] = StatusMap::Hostile;
                    m_status[1] = StatusMap::Owned;
                }
            }
        }
    }
}

// Initializes per-vehicle-group area arrays, with at least one element in each allocation.
// [FRONTIER] The destructor must release these arrays when its implementation is completed.
void AIStrategicArea::internalConstructorCall(const Vec2& topLeft, const Vec2& centre)
{
    // unused, and deliberately so - see the note above.
    (void)topLeft;

    m_landingZones.resize(0);

    m_position[0] = centre;
    m_position[1] = centre;

    m_i320 = 25;
    m_i324 = 25;
    m_sideEnabled[0] = true;
    m_sideEnabled[1] = true;

    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    const int vehicleGroups = settings->getNumberOfVehicleGroups();
    const int groups = vehicleGroups ? vehicleGroups : 1;

    m_flagsA = checkPointer(new bool[groups]);
    m_flagsB = checkPointer(new bool[groups]);

    m_listsA = checkPointer(new std::list<std::pair<float, AIStrategicObject*> >[groups]);
    m_listsB = checkPointer(new std::list<std::pair<float, AIStrategicObject*> >[groups]);

    m_mapsA = checkPointer(new std::map<AIStrategicObject*, float>[groups]);
    m_mapsB = checkPointer(new std::map<AIStrategicObject*, float>[groups]);
    m_mapsC = checkPointer(new std::map<AIStrategicObject*, float>[groups]);
    m_mapsD = checkPointer(new std::map<AIStrategicObject*, float>[groups]);

    m_distancesDirty[0] = true;
    m_distancesDirty[1] = true;

    for (int i = 0; i < groups; ++i)
    {
        m_flagsA[i] = true;
        m_flagsB[i] = true;
    }

    // Stores equal center-to-corner distances for the initial positions.
    // Each side retains its own value because setPosition can later move them independently.
    const float dx0 = m_centre.x - m_position[0].x;
    const float dy0 = m_centre.y - m_position[0].y;
    m_size[1] = std::sqrt(dx0 * dx0 + dy0 * dy0);

    const float dx1 = m_centre.x - m_position[1].x;
    const float dy1 = m_centre.y - m_position[1].y;
    m_size[2] = std::sqrt(dx1 * dx1 + dy1 * dy1);

    m_enemyTakeover[0] = false;
    m_enemyTakeover[1] = false;
    m_f408 = 1.0f;
    m_f412 = 1.0f;

    m_vehicleSearchRadius = m_size[1] + m_size[1];
}

// Raises the stored area height to the terrain height when the terrain is higher.
void AIStrategicArea::init()
{
    const IAITerrain* const terrain =
        static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);

    const float altitude = terrain->getTerrainAltitude(m_position[0].x, m_position[0].y);

    if (altitude > m_f148)
        m_f148 = altitude;

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;

    for (unsigned int i = 0; i < static_cast<unsigned int>(pathfinding->getNumberOfMaps()); ++i)
    {
        if (pathfinding->isMapUsed(static_cast<int>(i)))
        {
            const Vec3 orderPosition = getOrderPos(static_cast<int>(i), 1);

            unsigned int polygon = 0;
            unsigned int island  = 0;
            pathfinding->getPolygon(orderPosition, static_cast<int>(i), polygon, island, 1);
        }
    }
}

// Resolves order positions to the navmesh and converts them to area-relative offsets once.
void AIStrategicArea::initOrderPoses()
{
    Vec3 areaPosition;
    areaPosition.x = 0.0f;
    areaPosition.y = 0.0f;
    areaPosition.z = 0.0f;

    vec2ToVec3XZ(m_position[0], areaPosition);
    areaPosition.y = m_f148;

    IAIPathfinding* const pathfinding = IAIPathfinding::instance;

    for (int i = 0; i < pathfinding->getNumberOfMaps(); ++i)
    {
        MapData& data = m_mapData[i];

        unsigned int polygon = 0;
        unsigned int island  = 0;
        pathfinding->getPolygon(data.m_offset, i, polygon, island, 1);

        data.m_offset.x = data.m_offset.x - areaPosition.x;
        data.m_offset.y = data.m_offset.y - areaPosition.y;
        data.m_offset.z = data.m_offset.z - areaPosition.z;

        data.m_island = island;
    }

    m_orderPosesInitialised = true;
}

// Returns the area position plus the selected map's delta; map -1 omits the delta.
// The second parameter is unused; [UNRESOLVED] its original purpose is unknown.
Vec3 AIStrategicArea::getOrderPos(int map, int unused) const
{
    (void)unused;

    Vec3 position;
    position.x = m_position[0].x;
    position.y = m_f148;
    position.z = m_position[0].y;

    if (map != -1)
    {
        const MapData& data = m_mapData[map];
        position.x += data.m_offset.x;
        position.y += data.m_offset.y;
        position.z += data.m_offset.z;
    }

    return position;
}

// Tests a StrategicObjectType bit position in the object flags.
bool AIStrategicArea::hasObjectTypeFlag(StrategicObjectType type) const
{
    return ((m_objectTypeFlags >> type) & 1) != 0;
}

// Remaining virtual work keeps named stops. Live strategic ticks now reach the recovered update;
// later dirty/assignment/lifetime behavior remains separately bounded.
// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: aidll-02-skeleton]

// PE 100E18F0. Scales both temperatures for the side by the strategy's modifier for every type
// flag the area carries among the first 21, then by the status modifier (an owned area counts as
// Front when a hostile neighbour exists, otherwise Safe) and by its specific-object modifier.
// The scaling compounds on every call. Returns the first temperature.
float AIStrategicArea::updateTemperature(const Strategy* strategy, int side)
{
    const StatusMap::Status status = m_status[side - 1];
    const float* const modifiers = strategy->getStrategicObjectsModifiers(status);

    for (int type = 0; type < 21; ++type)
    {
        if (hasObjectTypeFlag(static_cast<StrategicObjectType>(type)))
        {
            m_f164[side - 1]     = m_f164[side - 1]     * modifiers[type];
            m_f164[side - 1 + 2] = m_f164[side - 1 + 2] * modifiers[type];
        }
    }

    switch (status)
    {
    case StatusMap::Owned:
    {
        const int type = hasNeighbourWithStatus(StatusMap::Hostile, side) ? Front : Safe;
        m_f164[side - 1]     = m_f164[side - 1]     * modifiers[type];
        m_f164[side - 1 + 2] = m_f164[side - 1 + 2] * modifiers[type];
        break;
    }
    case StatusMap::Hostile:
        m_f164[side - 1]     = m_f164[side - 1]     * modifiers[Enemy];
        m_f164[side - 1 + 2] = modifiers[Enemy] * m_f164[side - 1 + 2];
        break;
    case StatusMap::Neutral:
        m_f164[side - 1]     = m_f164[side - 1]     * modifiers[NeutralType];
        m_f164[side - 1 + 2] = modifiers[NeutralType] * m_f164[side - 1 + 2];
        break;
    default:
        break;
    }

    m_f164[side - 1]     = strategy->getSpecificObjectModifier(m_index) * m_f164[side - 1];
    m_f164[side - 1 + 2] = strategy->getSpecificObjectModifier(m_index) * m_f164[side - 1 + 2];

    AIStrategicObjectManager::getInstance();
    return m_f164[side - 1];
}

// Parameter types
// are mangled; neither body materializes a return. Names of local aggregates are inferred.
void AIStrategicArea::update(int side, ISAI* sai, bool& changed)
{
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    m_assignedSquadStrength[side - 1] = 0;
    std::list<AIStrategicObject*> objects;
    m_assignedSquadStrength[side - 1] = SquadManager::m_instance->getSquadStrengths(
        side, m_index, static_cast<StrategicStrengthType>(m_attack[side - 1] ? 0 : 1));

    Vec2 upper;
    const double dx = static_cast<double>(m_position[side - 1].x) - m_centre.x;
#if defined(_MSC_VER)
    // [PLATFORM] PE spills only the Y difference before adding position again.
    const float dy = m_position[side - 1].y - m_centre.y;
#else
    const double dy = static_cast<double>(m_position[side - 1].y) - m_centre.y;
#endif
    upper.x = static_cast<float>(dx + m_position[side - 1].x);
    upper.y = static_cast<float>(static_cast<double>(dy) + m_position[side - 1].y);

    float total = 0.0f, withoutUnits = 0.0f;
    float friendly = 0.0f, enemy = 0.0f, ratio = 0.0f;
    int friendlyUnits = 0, enemyUnits = 0;
    SampleHelper battleSamples(6), strategicSamples(2);
    float battle[6], strategic[2];
    int battleCounts[6], strategicCounts[2];
    int secondary = -1;
    {
        std::vector<unsigned int> handles;
        battleSamples.clear(battle, battleCounts);
        strategicSamples.clear(strategic, strategicCounts);
        m_resourceNeed[side - 1] = 0;
        IAIInformationEnvironment* const environment =
            static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, 0, true, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, 0, false, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, side, true, nullptr);
        environment->getObjectsWithinRectangle(handles, m_centre, upper, side, false, nullptr);

        // The two original record arms do the same sampling, except the root-only flag
        // suppression. Keep selected unit/physical/array receivers across their callbacks.
        const auto sampleInformation = [&](Information* information, bool root)
        {
            const double returnedSecurity = information->getSecurity();
            const float security = static_cast<float>(returnedSecurity);
#if defined(_MSC_VER)
            const double weighted = static_cast<double>(security) * information->getStrength();
#else
            const double weighted = returnedSecurity * information->getStrength();
#endif
            IPIUnit* const unit = information->getUnit();
            total = static_cast<float>(static_cast<double>(total) + weighted);
            if (!unit)
            {
                withoutUnits = static_cast<float>(weighted + withoutUnits);
                return;
            }

#if defined(_MSC_VER)
            // [PLATFORM] PE sums all six floats wide, then stores once. ELF stores each sum.
            const float* const values = unit->getBattleStrengths();
            const double sum = ((((static_cast<double>(values[0]) + values[1])
                + values[2]) + values[3]) + values[4]) + values[5];
            const float strength = static_cast<float>(sum);
#else
            float strength = 0.0f;
            for (int i = 0; i < 6; ++i)
                strength = unit->getBattleStrengths()[i] + strength;
#endif
            if (!unit->isOccupied())
                return;
            if (sai->isFriend(information->getSide()))
            {
                if (!root || (information->getObjectTypeFlags() & 0x10u) == 0)
                    ++friendlyUnits;
                friendly = static_cast<float>(static_cast<double>(security) * strength + friendly);
            }
            else if (sai->isEnemy(information->getSide()))
            {
                if (!root || (information->getObjectTypeFlags() & 0x10u) == 0)
                    ++enemyUnits;
                enemy = static_cast<float>(static_cast<double>(security) * strength + enemy);
                IPIPhysical* const physical = information->getPhysical();
#if defined(_MSC_VER)
                float* const input = unit->getBattleStrengths();
#endif
                const int type = physical->getStrengthType();
#if !defined(_MSC_VER)
                float* const input = unit->getBattleStrengths();
#endif
                battleSamples.addSample(input, type, battle, battleCounts);
                strategicSamples.addSample(unit->getStrategicStrengths(),
                    m_status[side - 1] != 0, strategic, strategicCounts);
                m_resourceNeed[side - 1] += static_cast<int>(
                    unit->getStrategicStrengths()[m_status[side - 1] == 0]);
            }
        };

#if defined(_MSC_VER)
        InformationManager* manager = handles.empty() ? nullptr : informationManager;
#endif
        auto end = handles.end();
        for (auto it = handles.begin(); it != end; ++it)
        {
#if !defined(_MSC_VER)
            InformationManager* manager = informationManager;
            Information* const root = manager->getInformation(*it);
#else
            // PE tests the handle's index before touching the captured manager's pool.
            Information* const root = (*it & 0xffffu) ? manager->getInformation(*it) : nullptr;
#endif
            if (root)
            {
                sampleInformation(root, true);
#if !defined(_MSC_VER)
                manager = informationManager;
#endif
                unsigned int handle = root->getFirstSecondaryObject(secondary, side);
                for (;;)
                {
#if defined(_MSC_VER)
                    // [PLATFORM] PE captures after secondary callbacks and carries this
                    // manager to the next root; ELF captures before each callback.
                    manager = informationManager;
                    Information* const current = (handle & 0xffffu)
                        ? manager->getInformation(handle) : nullptr;
#else
                    Information* const current = manager->getInformation(handle);
#endif
                    if (!current)
                        break;
                    sampleInformation(current, false);
#if !defined(_MSC_VER)
                    manager = informationManager;
#endif
                    handle = root->getSecondaryObject(secondary, side);
                }
                end = handles.end();
            }
#if defined(_MSC_VER)
            end = handles.end();
#endif
        }
        const double denominator = static_cast<double>(friendly) + enemy;
        if (denominator > 0.0)
            ratio = static_cast<float>(friendly / denominator);
    } // The query vector dies before the old-status capture and output stores.

    const StatusMap::Status oldStatus = m_status[side - 1];
    m_f164[side - 1] = total + m_size[0];
    m_f164[side + 1] = withoutUnits + m_size[0];
    m_friendlyRatio[side - 1] = ratio;
    m_enemyBattleStrength[side - 1] = enemy;
    m_friendlyBattleStrength[side - 1] = friendly;
    if (m_controlPointId != 0xffffffffu)
    {
        const int owner = game->getOwnerOfControlPoint(m_controlPointId);
        m_status[side - 1] = owner == side ? StatusMap::Owned
            : owner == 0 ? StatusMap::Neutral : StatusMap::Hostile;
    }
    else if (friendlyUnits > 0 || enemyUnits > 0)
    {
        if (m_sideEnabled[side - 1] && friendlyUnits > 0 && enemyUnits == 0)
            m_status[side - 1] = StatusMap::Owned;
        else if (m_sideEnabled[side == 1] && enemyUnits > 0 && friendlyUnits == 0)
            m_status[side - 1] = StatusMap::Hostile;
    }

    // Reread the ID/status after the owner callback. Owned->Neutral skips both the
    // takeover reset and getter; all other CP transitions query even when not owned.
    if (m_controlPointId != 0xffffffffu)
    {
        if (oldStatus == StatusMap::Owned && m_status[side - 1] == StatusMap::Neutral)
            changed = true;
        else
        {
            m_enemyTakeover[side - 1] = false;
            const double counter = getControlPointTakeOverCounter();
            if (m_status[side - 1] == StatusMap::Owned && counter < 0.9f)
            {
                m_enemyTakeover[side - 1] = true;
                m_status[side - 1] = StatusMap::Neutral;
            }
            changed = oldStatus == StatusMap::Owned
                ? m_status[side - 1] != StatusMap::Owned : m_status[side - 1] == StatusMap::Owned;
        }
    }
    else
    {
        changed = false;
        if ((oldStatus == StatusMap::Owned && m_status[side - 1] == StatusMap::Hostile)
            || (oldStatus == StatusMap::Hostile && m_status[side - 1] == StatusMap::Owned))
            changed = true;
    }
    m_statusChanged = changed;
    m_statistics[side - 1]->sample(battle, battleCounts);
    m_statistics[side + 1]->sample(strategic, strategicCounts);

    const float count = static_cast<float>(m_resourceNeed[side - 1]);
    const float floored = count > 1.0f ? count : 1.0f;
    const bool owned = m_status[side - 1] == StatusMap::Owned;
    const float input = owned ? m_needCurveInput[side - 1] : m_friendlyRatio[side - 1];
#if defined(_MSC_VER)
    double need = (2.0 - static_cast<double>(SCurve::calculate(input))) * floored;
    if (!owned)
        need *= 1.25;
    m_resourceNeed[side - 1] = static_cast<int>(need);
#else
    // [PLATFORM] ELF scales the base first and narrows the final product before cvttss2si.
    const float base = owned ? floored : floored * 1.25f;
    const float need = static_cast<float>((2.0 - static_cast<double>(SCurve::calculate(input))) * base);
    m_resourceNeed[side - 1] = static_cast<int>(need);
#endif
}

int AIStrategicArea::getChangedAttackDefence(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::getChangedAttackDefence");
}

// Full pair : fresh game/id after the flag callback.
float AIStrategicArea::getControlPointTakeOverCounter()
{
    if (hasObjectTypeFlag(ControlPoint))
        return static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->
            getControlPointTakeOverCounter(m_controlPointId);
    return 1.0f;
}

bool AIStrategicArea::isEnemyTakingOverBase(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::isEnemyTakingOverBase");
}

// PE 100DEAB0. Zero unless owned; otherwise (2 - SCurve(need input)) times the enemy battle
// strength plus temperature, raised by 1.5 next to a hostile area or 1.25 next to a neutral one.
float AIStrategicArea::calculateDefenseNeed(int side)
{
    if (m_status[side - 1] != StatusMap::Owned)
        return 0.0f;

    const float need = (2.0f - SCurve::calculate(m_needCurveInput[side - 1]))
                     * (m_enemyBattleStrength[side - 1] + m_f164[side - 1]);
    if (hasNeighbourWithStatus(StatusMap::Hostile, side))
        return static_cast<float>(need * 1.5);
    if (hasNeighbourWithStatus(StatusMap::Neutral, side))
        return static_cast<float>(need * 1.25);
    return need;
}

// PE 100E0300. Only a non-owned area next to an owned one has value: its temperature scaled by
// (1 + need input), raised by 1.5 when its only foreign neighbours are neutral or 1.25 when it has
// none, and weighted 100 / distance to the closest friendly squad leader when that distance lies
// strictly between 1 and 100000. The attack cost is subtracted in every case.
float AIStrategicArea::calculateAttackValue(int side)
{
    float value;
    if (m_status[side - 1] == StatusMap::Owned || !hasNeighbourWithStatus(StatusMap::Owned, side))
    {
        value = 0.0f;
        return value - calculateAttackCost(side);
    }

    value = (m_needCurveInput[side - 1] + 1.0f) * m_f164[side - 1];
    if (!hasNeighbourWithStatus(StatusMap::Hostile, side))
    {
        if (hasNeighbourWithStatus(StatusMap::Neutral, side))
            value = static_cast<float>(value * 1.5);
        else
            value = static_cast<float>(value * 1.25);
    }

    Vec3 position;
    position.x = m_position[0].x;
    position.y = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                     ->getTerrainAltitude(m_position[0].x, m_position[0].y);
    position.z = m_position[0].y;
    const float distance = SquadManager::m_instance->getDistanceToClosestSquad(position, side);
    if (!(distance > 1.0f) || distance >= 100000.0f || distance != distance)
    {
        value = 0.0f;
        return value - calculateAttackCost(side);
    }

    value = 1.0f / distance * value * 100.0f;
    return value - calculateAttackCost(side);
}

// PE 100DEB30. The second strategic-statistics bucket (-1 with fewer than two buckets), scaled
// by (1.5 - half the need input) and by one plus the friendly ratio.
float AIStrategicArea::calculateAttackCost(int side)
{
    const EnemyStatistics* const statistics = m_statistics[side + 1];
    const float value = statistics->m_size <= 1 ? -1.0f : statistics->m_strength[1];
    return value * (1.5f - m_needCurveInput[side - 1] * 0.5f) * (m_friendlyRatio[side - 1] + 1.0f);
}

bool AIStrategicArea::isNeighbour(AIStrategicObject*) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::isNeighbour");
}

// Native vector scan, captured end, no null/side guards; first matching status wins.
bool AIStrategicArea::hasNeighbourWithStatus(StatusMap::Status status, int side) const
{
    const auto end = m_neighbours.end();
    for (auto it = m_neighbours.begin(); it != end; ++it)
    {
        if ((*it)->getStatus(side) == status)
            return true;
    }
    return false;
}

bool AIStrategicArea::isNeighbouringObject(AIStrategicObject*) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::isNeighbouringObject");
}

// Applies the same scan to the second neighbor vector.
bool AIStrategicArea::hasNeighbouringObjectWithStatus(StatusMap::Status status, int side) const
{
    const auto end = m_neighbouringObjects.end();
    for (auto it = m_neighbouringObjects.begin(); it != end; ++it)
    {
        if ((*it)->getStatus(side) == status)
            return true;
    }
    return false;
}

bool AIStrategicArea::hasObjectTypeFlags(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::hasObjectTypeFlags");
}

int AIStrategicArea::getTacticalStatus(int) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::getTacticalStatus");
}

bool AIStrategicArea::isAreaTakeable()
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::isAreaTakeable");
}

// PE 100E7DC0. The dirty flag is left as found.
void AIStrategicArea::updateDistances(int side)
{
    if (!m_distancesDirty[side - 1])
        return;
    m_distances[side - 1].clear();
    const int groups =
        static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfVehicleGroups();
    if (groups == 0)
        return;
    for (int group = 0; group < groups; ++group)
    {
        updateDistancesForGroup(side, group);
        updateRoutesForGroup(side, group);
    }
}

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: area-distance-groups] Recover PE 100E7C60.
void AIStrategicArea::updateDistancesForGroup(int, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::updateDistancesForGroup");
}

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: area-distance-groups] Recover PE 100E4980.
void AIStrategicArea::updateRoutesForGroup(int, int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::updateRoutesForGroup");
}

namespace {

// Shared squad-collection step: an owned area (or, with an enemy takeover under way, an owned or
// neutral one) is defended; anything else is attacked.
void orderCollectedSquad(AIStrategicArea* area, ISAI* sai, const Vec3& position,
                         SquadCoordinator* squad, StatusMap::Status status, bool takeover)
{
    CommanderInterface* const commander = sai->getCommanderInterface();
    if ((status == StatusMap::Owned && !takeover)
        || (takeover && (status == StatusMap::Owned || status == StatusMap::Neutral)))
        commander->giveDefendOrder(position, squad);
    else
        commander->giveAttackOrder(position, squad);
    (void)area;
}

} // anonymous namespace

// PE 100E8600. Collects the nearest available squads until the side's assigned strength exceeds
// its need; true when the need is met.
bool AIStrategicArea::collectSquadResources(int side, ISAI* sai)
{
    if (m_assignedSquadStrength[side - 1] >= m_resourceNeed[side - 1])
        return true;

    int strength = 0;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    vec2ToVec3XZ(getPosition(side), position);
    position.y = m_f148;
    SquadManager* const squads = SquadManager::m_instance;
    updateDistances(side);

    while (m_assignedSquadStrength[side - 1] <= m_resourceNeed[side - 1])
    {
        SquadCoordinator* const squad =
            squads->getClosestAvailableSquad(this, position, side, m_attack[side - 1], &strength);
        if (!squad)
            break;
        orderCollectedSquad(this, sai, position, squad, m_status[side - 1],
                            m_enemyTakeover[side - 1]);
        m_assignedSquadStrength[side - 1] += strength;
    }
    return m_assignedSquadStrength[side - 1] >= m_resourceNeed[side - 1];
}

// PE 100E8740. Collects up to `amount` more squads regardless of need; returns how many came.
int AIStrategicArea::collectAdditionalSquadResources(int amount, int side, ISAI* sai)
{
    int collected = 0;
    int strength = 0;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    vec2ToVec3XZ(getPosition(side), position);
    SquadManager* const squads = SquadManager::m_instance;
    position.y = m_f148;
    updateDistances(side);
    if (amount <= 0)
        return 0;

    while (collected < amount)
    {
        SquadCoordinator* const squad =
            squads->getClosestAvailableSquad(this, position, side, m_attack[side - 1], &strength);
        if (!squad)
            break;
        orderCollectedSquad(this, sai, position, squad, m_status[side - 1],
                            m_enemyTakeover[side - 1]);
        m_assignedSquadStrength[side - 1] += strength;
        ++collected;
    }
    return collected;
}

int AIStrategicArea::releaseSurplus(int)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::releaseSurplus");
}

// Both delegations stay virtual;
// MSVC groups the Vec2/Vec3 overload slots in the opposite order to the ELF.
float AIStrategicArea::gravity(const Vec3& position, int side)
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(position, flat);
    return gravity(flat, side);
}

// The position is read directly
// using side-1. x87 keeps the arithmetic wide until the float call argument.
float AIStrategicArea::gravity(const Vec2& position, int side)
{
    const double dx = static_cast<double>(position.x) - m_position[side - 1].x;
    const double dz = static_cast<double>(position.y) - m_position[side - 1].y;
    const double squaredDistance = dx * dx + dz * dz;
    double distance = std::sqrt(squaredDistance);
#if !defined(_MSC_VER)
    // [PLATFORM] ELF retries an unordered fsqrt through sqrtf, with its input
    // narrowed to float. PE uses the hardware result without this CRT fallback.
    if (distance != distance)
        distance = std::sqrt(static_cast<float>(squaredDistance));
#endif
    return gravity(static_cast<float>(distance), side);
}

bool AIStrategicArea::retainBot(int, int, ISAI*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::retainBot");
}

// Marks the side's distances dirty and sets every vehicle group's flag for that side; with no
// vehicle groups the array's single element is set. PE 100DED70.
void AIStrategicArea::setAsDirty(int side)
{
    m_distancesDirty[side - 1] = true;
    bool* const flags = (side == 1) ? m_flagsA : m_flagsB;
    const int groups = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfVehicleGroups();
    if (groups == 0)
    {
        flags[0] = true;
        return;
    }
    for (int i = 0; i < groups; ++i)
        flags[i] = true;
}

int AIStrategicArea::orderBot(int, const std::list<AIStrategicObject*>&, int, ISAI*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::AIStrategicArea::orderBot");
}

// The size uses side, unlike the
// position's side-1 index above. Unordered distance takes the division arm.
float AIStrategicArea::gravity(float distance, int side)
{
    if (distance <= 1.0f)
        return m_size[side];
    const double wideDistance = distance;
    return static_cast<float>(m_size[side] / (wideDistance * wideDistance));
}

} // namespace ai
} // namespace hfe
} // namespace dice
