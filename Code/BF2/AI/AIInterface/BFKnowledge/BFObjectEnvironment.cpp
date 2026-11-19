// Engine object queries and information-grid updates used by the AI.
// [RETIRE-WHEN: aidll-01-adapters] Remove when the corresponding implementation is complete and validated.

#include "BFObjectEnvironment.h"

#include <set>

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods
#include <cstring>
#include "../../../Scene/Object/PlayerManager.h"   // isValidPlayerId's one call
#include "../../../Core/Ids.h"                     // the four container CIDs, IID_IPlayerControlObject
#include "../../../Game/Objects/IObject.h"         // getTemplate, getAIComponent
#include "../../../Game/Objects/IObjectTemplate.h" // getClassID
#include "../../../Game/Objects/IPlayer.h"         // the per-player tests and getId
#include "../../../Game/Objects/Game/PlayerControlObject.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Game/IGameLogic.h"                                  // gameLogic: getPcoMap, giveDamage
#include "../../../Game/Objects/GameLogicComp/IArmorGLComp.h"          // getHitPoints / getMaxHitPoints
#include "../../../Game/Objects/GameLogicComp/ArmorGLCompTemplate.h"   // getCanBeDestroyed
#include "BFTimer.h"                                                   // IBFTimer::getGameTime
#include "IAIObject.h"
#include "BFTerrain.h"                                                  // IAITerrain::intersectTerrain
#include "../../../Scene/Object/ObjectManager.h"                       // objectManager->intersectLine
#include "../../../Scene/Object/ObjectPredicator.h"
#include "../../../Physics/MaterialManager.h"
#include "../../../Physics/Material.h"
#include "../../../Game/Objects/Game/Soldier.h"                      // getCameras
#include "../../../Game/Objects/ICameraObject.h"
#include "IAIObjectPlugIn.h"

namespace dice {
namespace hfe {
namespace world {
extern IBase* materialManager;                      // Core/Globals.cpp
}

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IAIObjectEnvironment::IAIObjectEnvironment()
{
    if (!CommonSingletonServer::m_iaiObjectEnvironment)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IAIObjectEnvironment"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IAIObjectEnvironment::~IAIObjectEnvironment()
{
    if (CommonSingletonServer::m_iaiObjectEnvironment == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IAIObjectEnvironment"));
        BFSingletonServer::updateStaticMethods();
    }
}

// Zeroes the opaque vector storage, flag, and severe-damage timestamp separately.
// Leaves padding at +0x29..+0x2B untouched.
BFObjectEnvironment::BFObjectEnvironment()
{
    std::memset(m_opaque, 0, sizeof(m_opaque));  // +0x04 .. +0x27, the three vectors
    m_byte28 = 0;                                // +0x28, a byte
    m_lastSevereDamageTime = 0.0;                // +0x2C, a double
}

BFObjectEnvironment::~BFObjectEnvironment()
{
}

void BFObjectEnvironment::getWorldFrustumObjects(std::vector<IAIObject*>& out, const Frustum& frustum, AIObjectPredicate* filter) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getWorldFrustumObjects");
}

void BFObjectEnvironment::getWorldSphereObjects(std::vector<IAIObject*>& out, const Vec3& centre, float radius, AIObjectPredicate* filter) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getWorldSphereObjects");
}

void BFObjectEnvironment::getAllWorldObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAllWorldObjects");
}

void BFObjectEnvironment::getAllDestroyableObjects(std::vector<DestroyableObject>& out) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAllDestroyableObjects");
}

void BFObjectEnvironment::getWorldPlayerIds(std::vector<int>& out) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getWorldPlayerIds");
}

// BF2.exe 81C890. The AI objects of living, upright players' vehicles. A player in a secondary
// seat contributes the vehicle's primary object instead, once, and only when nobody occupies
// it; those follow the directly occupied objects, in pointer order.
void BFObjectEnvironment::getWorldPlayerOccupiedObjects(std::vector<IAIObject*>& out, AIObjectPredicate* filter) const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    std::set<IAIObject*> primaries;

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;

        if (!player || !player->getIsAlive() || player->getIsManDown() || !player->getVehicle())
            continue;

        IAIObject* aiObject = static_cast<IAIObject*>(player->getVehicle()->getAIComponent());

        if (!aiObject)
        {
            world::IObject* occupying = 0;
            if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
            {
                occupying = static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (!occupying)
                    continue;
                aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            else if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
            {
                // The parachute's occupant is not null-checked.
                world::IPlayerControlObject* control = static_cast<world::IPlayerControlObject*>(
                    player->getVehicle()->queryInterface(world::IID_IPlayerControlObject));
                aiObject = static_cast<IAIObject*>(control->getOccupingObject()->getAIComponent());
            }
            else
            {
                if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate ||
                    (*it)->getVehicle()->getTemplate()->getClassID() == world::CID_GrapplingHookRopeContainerTemplate)
                    occupying = static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (!occupying)
                    continue;
                aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            if (!aiObject)
                continue;
        }

        if (aiObject->isPrimary())
        {
            if (filter && !filter->predicate(aiObject))
                continue;
            out.push_back(aiObject);
        }
        else
        {
            primaries.insert(aiObject->getPrimaryObject());
        }
    }

    for (std::set<IAIObject*>::iterator it = primaries.begin(); it != primaries.end(); ++it)
    {
        IAIObject* const primary = *it;
        if (primary->getUnit()->isOccupied())
            continue;
        if (filter && !filter->predicate(primary))
            continue;
        out.push_back(primary);
    }
}

// Collects AI objects matching the supplied predicate within the requested region.
void BFObjectEnvironment::getWorldPlayerOccupiedObjectsWithIds(std::vector<std::pair<int, IAIObject*> >& out, AIObjectPredicate* filter) const
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;

        if (!player || !player->getIsAlive() || player->getIsManDown() || !player->getVehicle())
            continue;

        IAIObject* aiObject = static_cast<IAIObject*>(player->getVehicle()->getAIComponent());

        if (!aiObject)
        {
            if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
            {
                world::IObject* occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }
            else if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
            {
                world::IPlayerControlObject* control = static_cast<world::IPlayerControlObject*>(
                    player->getVehicle()->queryInterface(world::IID_IPlayerControlObject));
                aiObject = static_cast<IAIObject*>(control->getOccupingObject()->getAIComponent());
            }
            else if ((*it)->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate ||
                     (*it)->getVehicle()->getTemplate()->getClassID() == world::CID_GrapplingHookRopeContainerTemplate)
            {
                world::IObject* occupying =
                    static_cast<world::PlayerControlObject*>(player->getVehicle())->getOccupingObject();
                if (occupying)
                    aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
            }

            if (!aiObject)
                continue;
        }

        if (filter)
        {
            if (filter->predicate(aiObject))
                out.push_back(std::make_pair((*it)->getId(), aiObject));
        }
        else
        {
            out.push_back(std::make_pair((*it)->getId(), aiObject));
        }
    }
}

IAIObject* BFObjectEnvironment::getLocalPlayerObject() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getLocalPlayerObject");
}

bool BFObjectEnvironment::getLocalPlayerPosition(Vec3& position) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getLocalPlayerPosition");
}

bool BFObjectEnvironment::getLocalPlayerCameraTransformation(Mat4& transform) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getLocalPlayerCameraTransformation");
}

int BFObjectEnvironment::getLocalPlayerId() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getLocalPlayerId");
}

const std::vector<int>& BFObjectEnvironment::getActivePlayerIds() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getActivePlayerIds");
}

// Returns whether PlayerManager resolves the id to a player.
bool BFObjectEnvironment::isValidPlayerId(int id) const
{
    return world::playerManager->getPlayerFromId(id) != 0;
}

int BFObjectEnvironment::getNextPlayerId(int id) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getNextPlayerId");
}

namespace {

// The AI object an engine object stands for (Client PE 0x8182A0): its own AI component, or for a
// ladder, parachute, zipline or grappling-hook container without one, the occupant's. The class id
// is re-read for each test, as shipped. [INFERRED] name.
IAIObject* aiObjectForCollision(world::IObject* object)
{
    IAIObject* aiObject = static_cast<IAIObject*>(object->getAIComponent());
    if (aiObject)
        return aiObject;
    if (object->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_ParachuteTemplate)
    {
        world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        world::IObject* const occupying = control->getOccupingObject();
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();
        if (occupying)
            aiObject = static_cast<IAIObject*>(occupying->getAIComponent());
    }
    if (object->getTemplate()->getClassID() == world::CID_GrapplingHookRopeContainerTemplate)
    {
        world::IObject* const occupying =
            static_cast<world::PlayerControlObject*>(object)->getOccupingObject();
        if (occupying)
            return static_cast<IAIObject*>(occupying->getAIComponent());
    }
    return aiObject;
}

// Adapts an AIObjectPredicate to the object manager's line test (Client vtable 0x947BC4). An
// engine object without an AI object answers m_noAIObjectResult; see-through surfaces are skipped.
// [INFERRED] names.
class AIObjectCollisionPredicator : public world::ObjectPredicator
{
public:
    explicit AIObjectCollisionPredicator(AIObjectPredicate* filter)
        : m_filter(filter), m_noAIObjectResult(true)
    {
    }

    virtual bool predicate(world::IObject* object)
    {
        IAIObject* const aiObject = aiObjectForCollision(object);
        if (!aiObject)
            return m_noAIObjectResult;
        if (m_filter)
            return m_filter->predicate(aiObject);
        return true;
    }

    virtual unsigned int getFlags() const
    {
        return 0x1000000u;
    }

    virtual bool performCulling(world::IObject*)
    {
        return true;
    }

    // The body ProjectileVsTargetPredicator::predicateSurface also has (Client PE 0x4EBD70).
    virtual bool predicateSurface(int surface)
    {
        world::Material* const material = static_cast<world::MaterialManager*>(world::materialManager)
            ->getMaterialPtr(static_cast<unsigned int>(surface));
        return !material->getIsSeeThrough();
    }

private:
    AIObjectPredicate* m_filter;
    bool m_noAIObjectResult;
};

} // anonymous namespace

// Terrain first; a miss asks collideLineWithObjects, whose answer is reduced to a bool.
bool BFObjectEnvironment::collideLineWithWorld(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit)
{
    Vec3 terrainHit;
    terrainHit.x = 0.0f;
    terrainHit.y = 0.0f;
    terrainHit.z = 0.0f;
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    float distance;
    if (static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
            ->intersectTerrain(from, to, terrainHit, distance, normal))
        return true;
    return collideLineWithObjects(from, to, ignore, a, b, c, hit) != 0;
}

// Objects first, through the object manager with the filter adapted (none without a filter);
// a miss tests the terrain. The hit point goes to hit, or to a local when hit is null.
bool BFObjectEnvironment::collideLineWithWorld(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, Vec3* hit)
{
    Vec3 localHit;
    localHit.x = 0.0f;
    localHit.y = 0.0f;
    localHit.z = 0.0f;
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    if (!hit)
        hit = &localHit;
    Vec3 direction;
    direction.x = to.x - from.x;
    direction.y = to.y - from.y;
    direction.z = to.z - from.z;
    AIObjectCollisionPredicator predicator(filter);
    world::IObject* hitObject;
    int hitPart;
    if (world::objectManager->intersectLine(hitObject, *hit, normal, hitPart, from, direction,
                                            filter ? &predicator : nullptr,
                                            true, false, false, true, 0u))
        return true;
    float distance;
    return static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
        ->intersectTerrain(from, to, *hit, distance, normal);
}

bool BFObjectEnvironment::getMaxHeight(const Vec3& from, const Vec3& to, AIObjectPredicate* filter, float& height)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getMaxHeight");
}

bool BFObjectEnvironment::hasIndirectLineOfFire(IWeapon* weapon, const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::hasIndirectLineOfFire");
}

void BFObjectEnvironment::pickRandomSensePosition(Vec3& position, Vec3& normal, Vec3 hint, IAIObject* object)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::pickRandomSensePosition");
}

namespace {

// The shared body of the two soldier sense queries (one folded function in the PE, Client
// 0x81A5C0): a soldier's sense position is its first camera's absolute position, and the normal
// slot receives that camera's relative position. The camera object is not null-checked.
bool soldierSensePosition(Vec3& position, Vec3& normal, IAIObject* object)
{
    if (!object->getObject())
        return false;
    if (!object->isOfType(ITSoldier))
        return false;
    world::IObject* const engineObject = object->getObject();
    // [CHOSEN] const_cast: the tree declares the template pointer const, queryImplementation not.
    if (!const_cast<world::IObjectTemplate*>(engineObject->getTemplate())
             ->queryImplementation(world::CID_SoldierTemplate))
        return false;
    std::map<int, world::ICameraObject*>* const cameras =
        static_cast<world::Soldier*>(engineObject)->getCameras();
    world::ICameraObject* const camera = cameras->begin()->second;
    world::IObject* const cameraObject =
        camera ? static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject)) : nullptr;
    position = cameraObject->getAbsolutePosition();
    normal = cameraObject->getRelativePosition();
    return true;
}

} // anonymous namespace

// Ignores part: every part answers the soldier's camera position.
bool BFObjectEnvironment::pickSoldierRandomSensePosition(Vec3& position, Vec3& normal, IAIObject* object, unsigned int& /*part*/)
{
    return soldierSensePosition(position, normal, object);
}

// The same folded body as pickSoldierRandomSensePosition. [INFERRED] identical source bodies.
bool BFObjectEnvironment::getSoldierSensePosition(Vec3& position, Vec3& normal, IAIObject* object, unsigned int /*part*/)
{
    return soldierSensePosition(position, normal, object);
}

int BFObjectEnvironment::getHeightAndNormalForObject(IAIObject* object, const Vec3& position, float& height, Vec3& normal, float& slope)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getHeightAndNormalForObject");
}

void BFObjectEnvironment::getVehicleSpawnerPositions(std::vector<Vec3>& out, unsigned char team) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getVehicleSpawnerPositions");
}

IAIObject* BFObjectEnvironment::getObjectFromName(const std::string& name)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getObjectFromName");
}

int BFObjectEnvironment::getAllStaticCollisionMeshes(std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAllStaticCollisionMeshes");
}

int BFObjectEnvironment::getAllStaticCollisionMeshesWithinBoundary(const Vec2& min, const Vec2& max, std::list<MeshAndBoundingBox>& meshes, std::list<WaterMesh>& water, std::map<const IAIObject*, DestroyableAddOnMesh>& addOns, std::list<MeshAndBoundingBox>& more, std::pair<Vec2, Vec2>& bounds, unsigned char team) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAllStaticCollisionMeshesWithinBoundary");
}

bool BFObjectEnvironment::isAsleep(unsigned int id) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::isAsleep");
}

int BFObjectEnvironment::getTargetObjects(std::vector<TargetData>& out, int team) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getTargetObjects");
}

// ELF, 10 bytes. Empty in the dedicated image.
void BFObjectEnvironment::loadOvergrowthCollision(const std::string& path) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::loadOvergrowthCollision");
}

bool BFObjectEnvironment::initAiCombatAreas() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::initAiCombatAreas");
}

bool BFObjectEnvironment::isAiPositionInsideCombatArea(const Vec3& position) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::isAiPositionInsideCombatArea");
}

int BFObjectEnvironment::getAiCombatAreaLayer() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAiCombatAreaLayer");
}

int BFObjectEnvironment::getAICombatArea(std::list<Vec2>& out) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::getAICombatArea");
}

// Updates the AI object environment during the simulation tick.
void BFObjectEnvironment::damageSeverelyDamagedVehicles()
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    if (m_lastSevereDamageTime > now)
        m_lastSevereDamageTime = 0.0;

    if (!(now - m_lastSevereDamageTime > 4.0))
        return;

    m_lastSevereDamageTime = now;

    std::map<unsigned int, world::IObject*>* const pcos = gameLogic->getPcoMap();   // +0x198

    for (std::map<unsigned int, world::IObject*>::iterator it = pcos->begin(); it != pcos->end(); ++it)
    {
        world::IObject* const object = it->second;

        if (object->getTemplate()->getClassID() == world::CID_SoldierTemplate)
            continue;

        world::IPlayerControlObject* pco = 0;
        for (world::IObject* o = object; o; o = o->getParent())
        {
            pco = static_cast<world::IPlayerControlObject*>(
                o->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }

        if (pco->isOccupied())                                                        // +0x18
            continue;

        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(object->getArmorComponent());          // +0x3C
        world::ArmorGLCompTemplate* const armorTemplate = static_cast<world::ArmorGLCompTemplate*>(
            object->getTemplate()->getComponentFromClassID(world::CID_ArmorGLCompTemplate));

        if (!armor || !armorTemplate)
            continue;

        const float maxHitPoints = armor->getMaxHitPoints();                         // +0x84

        if (!armorTemplate->getCanBeDestroyed())                                      // +0x154
            continue;

        if (armor->getHitPoints() / maxHitPoints < 0.5f)                              // +0x3C
        {
            Vec3 position;  position.x = 0.0f;  position.y = 0.0f; position.z = 0.0f;
            Vec3 direction; direction.x = 0.0f; direction.y = 1.0f; direction.z = 0.0f;

            gameLogic->giveDamage(object, maxHitPoints * 0.1f, 0, 0, 0, -1, 0, 0,     // +0xE4
                                  position, direction, -1, false, true, false);
        }
    }
}

int BFObjectEnvironment::collideLineWithObjects(const Vec3& from, const Vec3& to, const std::vector<IAIObject*>& ignore, bool a, float b, bool c, bool& hit)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFObjectEnvironment::collideLineWithObjects");
}

} // namespace ai
} // namespace hfe
} // namespace dice
