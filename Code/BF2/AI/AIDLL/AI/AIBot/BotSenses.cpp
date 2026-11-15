// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotSenses operations.

#include "BotSenses.h"

#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "BotMain.h"
#include "SensingAgent.h"
#include "CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/IBotManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIAdapterTypes.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Core/Debug.h"
#include "../../../../Math/Mat4.h"

#include <algorithm>

namespace dice {
namespace hfe {
namespace ai {

// Creates the sight frustum and initializes sensing collections for the supplied bot.
BotSenses::BotSenses(BotMain* owner)
    : m_bot(owner)
    // the camera. See the block above.
    , m_frustum(checkPointer(new Frustum()))
    , m_double12(-1000000.0)
    , m_int32(0)
    , m_int36(0)
    , m_int40(0)
    , m_int88(0)
    , m_int92(0)
{
    // the constructor'S last statement - see the block above. The binary copies the list's head
    // pointer; in source that is the list's own end. The cursor at +80 is deliberately not here.
    m_cursor60 = m_list56.end();
}

// Clears sensed-object records and resets per-life sensing state.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
void BotSenses::reset()
{
    for (SensedObjectMap::iterator it = m_sensedObjects.begin(); it != m_sensedObjects.end(); )
    {
        // Retail here releases the object's `Information` through `informationManager` and then
        // deletes `it->second`. See the block above for why this tree cannot write it yet.
        BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::reset (sensed objects present)");
        m_sensedObjects.erase(it++);
    }
    m_sensedObjects.clear();

    m_map44.clear();
    m_list56.clear();
    m_cursor60 = m_list56.end();
    m_sortBuffer.clear();

    m_int40 = 0;
    m_int36 = 0;
    m_int32 = 0;
    m_double12 = -1000000.0;

    m_set64.clear();
    m_list76.clear();
    // The constructor never did this; the cursor at +80 becomes valid here for the first time.
    m_cursor80 = m_list76.end();

    for (std::list<FireEntry>::iterator it = m_list84.begin(); it != m_list84.end(); ++it)
    {
        // Retail here is `delete it->first;`. See the block above.
        BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::reset (fire objects present)");
    }
    m_list84.clear();

    m_int88 = 0;
    m_int92 = 0;

    m_map100.clear();
}

// Resets senses and deletes the frustum; member containers release their own storage.
BotSenses::~BotSenses()
{
    reset();
    delete m_frustum;
}

// [FRONTIER] Remaining sight, hearing and communication operations require implementation.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

SensedObject* BotSenses::getSensedObject(unsigned int handle) const
{
    const SensedObjectMap::const_iterator it = m_sensedObjects.find(handle);
    if (it == m_sensedObjects.end())
        return 0;
    return it->second;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] The record layouts are not recovered; these stop when
// the senses first hold a record. [RETIRE-WHEN: bot-senses-records] Recover FireObject and
// SensedObject and validate a bot that sees and hears an enemy.
const Vec3& FireObject::getPosition() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::FireObject::getPosition");
}

double FireObject::getTime() const
{
    BF2_AI_FRONTIER("dice::hfe::ai::FireObject::getTime");
}

// PE 101E1510. The sense point, part, first-sensed and last-seen times are left unset.
SensedObject::SensedObject(unsigned int handle, bool direct, bool sided)
    : m_handle(handle)
    , m_double40(-1000.0)
    , m_senseFlags((direct ? 0u : 8u) + (direct ? 0u : 0x10u) + (direct ? 2u : 1u) + (sided ? 4u : 0u))
    , m_lastSense(2)
    , m_lastUpdate(0.0)
    , m_positionTime(0.0)
    , m_contactAdded(false)
{
    m_senseNormal.x = m_senseNormal.y = m_senseNormal.z = 0.0f;
    m_position.x = m_position.y = m_position.z = 0.0f;
    m_lastPosition.x = m_lastPosition.y = m_lastPosition.z = 0.0f;
}

namespace {

// One entry per view kind and step of the view cycle: the sight cone's field of view and the
// camera-turn threshold below which the cycle restarts. PE 102ADAB8, writable. [INFERRED] names.
struct ViewSetting
{
    float fov;
    float threshold;
};
ViewSetting s_viewSettings[2][3] = {
    { { 1.7453293f, 0.0f }, { 1.0471976f, 0.0f }, { 0.5235988f, 0.0f } },
    { { 1.3089969f, 0.0f }, { 0.7853982f, 0.0f }, { 0.2617994f, 0.0f } },
};

// Enemy and neutral objects within view distance; a non-soldier view also needs a unit's
// cull distance, a soldier view lets units through only inside 300. Objects of type 0x15 must
// be physical and alive. PE vtable 10284428, PE 101B85D0. [INFERRED] name.
class SightPredicate : public AIObjectPredicate
{
public:
    bool predicate(IAIObject* object) override
    {
        const int side = object->getSide();
        if (!side)
        {
            const Vec3& at = object->getPosition();
            const float dx = at.x - m_position.x;
            const float dy = at.y - m_position.y;
            const float dz = at.z - m_position.z;
            return dz * dz + dy * dy + dx * dx < m_rangeSquared;
        }
        if (side == m_side)
            return false;
        if (object->isOfType(static_cast<InformationType>(0x15)))
        {
            if (!object->isPhysical())
                return false;
            if (!(object->getPhysical()->getHealth() > 0.0f))
                return false;
        }
        const Vec3& at = object->getPosition();
        const float dx = m_position.x - at.x;
        const float dy = m_position.y - at.y;
        const float dz = m_position.z - at.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (object->isUnit() && !m_soldierView)
        {
            if (!(distanceSquared < m_rangeSquared))
                return false;
            return object->getUnit()->getCullDistanceSqr() > distanceSquared;
        }
        if (!m_soldierView)
            return distanceSquared < m_rangeSquared;
        if (!(distanceSquared < m_rangeSquared))
            return false;
        return distanceSquared < 90000.0f;
    }

    /* +0x04 */ int   m_side;
    /* +0x08 */ Vec3  m_position;
    /* +0x14 */ float m_rangeSquared;
    /* +0x18 */ bool  m_soldierView;
};

bool closerFirst(const std::pair<float, unsigned int>& a, const std::pair<float, unsigned int>& b)
{
    return a.first < b.first;
}

float distanceSquared(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // anonymous namespace

// PE 101B8AA0.
bool BotSenses::isCloseBehind(Information* equipment, IAIObject* object)
{
    Information* const primary = informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
    if (!primary)
        return false;
    const Mat4& transform = primary->getTransformation();
    if (!object)
        return false;
    Information* const information = informationManager->getInformation(object->getInformation(equipment->getSide()));
    if (!information || !information->isPrimary())
        return false;
    const Vec3& at = object->getPosition();
    const float dx = transform.m[12] - at.x;
    const float dy = transform.m[13] - at.y;
    const float dz = transform.m[14] - at.z;
    if (!(dz * dz + dy * dy + dx * dx < 40000.0f))
        return false;
    return dz * transform.m[10] + dy * transform.m[9] + dx * transform.m[8] > 0.0f;
}

// PE 101BD8F0. A first sighting creates the record (reported until seen directly), stores the
// object's position and joins the seen list; a direct sighting also adds a contact, reporting
// a new one over the radio, while a reported one alerts the sensing agent. A record already
// in the seen list is refreshed only when it was reported and is now seen directly.
void BotSenses::addSeen(unsigned int handle, const Vec3& normal, unsigned int part,
                        Information* info, bool direct, bool clear)
{
    const int side = m_bot->getSide();
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (info->setTime(now))
        info->getAIObject()->addInformationToGrid(side);

    std::map<unsigned int, TimedList::iterator>::iterator seen = m_map44.find(handle);
    if (seen == m_map44.end())
    {
        SensedObject* object;
        SensedObjectMap::iterator found = m_sensedObjects.find(handle);
        if (found == m_sensedObjects.end())
        {
            object = checkPointer(new SensedObject(handle, true, info->getSide() != 0));
            object->m_firstSensed = now;
            object->m_senseFlags |= 8;
            object->m_lastSense = 8;
            m_sensedObjects.insert(SensedObjectMap::value_type(handle, object));
        }
        else
        {
            object = found->second;
        }
        object->m_senseFlags |= 2;
        object->m_lastSense = 2;
        object->m_position = info->getPosition();
        object->m_positionTime = now;
        object->m_lastPosition = info->getPosition();
        object->m_lastUpdate = now;
        if (direct)
        {
            object->m_senseFlags &= ~8u;
            if (!object->m_contactAdded)
            {
                if (!info->hasContact())
                    m_bot->getCommunicationAgent()->reportContact(info);
                info->addContact();
                object->m_contactAdded = true;
            }
        }
        else
        {
            m_bot->getSensingAgent()->setAlerted();
        }
        if (!clear)
        {
            object->m_senseFlags |= 0x40;
            object->m_lastSense = 0x40;
        }
        object->m_lastSeen = now;
        object->m_senseNormal = normal;
        object->m_part = part;
        m_list56.push_back(TimedObject(now, object));
        m_map44[handle] = --m_list56.end();
        return;
    }

    SensedObject* const object = seen->second->second;
    if (!(object->m_senseFlags & 8) || !direct)
        return;
    TimedList::iterator node = seen->second;
    if (m_cursor60 == node)
        ++m_cursor60;
    m_list56.erase(node);
    if (!object->m_contactAdded)
    {
        info->addContact();
        object->m_contactAdded = true;
    }
    object->m_senseNormal = normal;
    object->m_part = part;
    object->m_position = info->getPosition();
    object->m_positionTime = now;
    object->m_lastPosition = info->getPosition();
    object->m_senseFlags &= ~8u;
    object->m_lastUpdate = now;
    m_list56.push_back(TimedObject(now, object));
    seen->second = --m_list56.end();
}

// PE 101BDDC0.
void BotSenses::see(const double& slice)
{
    IBFTimer* const timer = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const double start = timer->getUpdateTime();
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    m_bot->getName();
    const Mat4& camera = *static_cast<const Mat4*>(m_bot->getPlayer()->getCameraTransformation());
    m_narrowView = (equipment->getObjectTypeFlags() >> 22) & 1;
    if (m_int36 == 3)
    {
        m_bot->setQuadSeen();
        m_int36 = 0;
        const Mat4& previous = m_frustum->m_transformation;
        const float turn = camera.m[10] * previous.m[10] + camera.m[9] * previous.m[9] +
                           camera.m[8] * previous.m[8];
        if (m_int32 == 0)
            m_int32 = 1;
        else if (m_int32 == 1)
            m_int32 = 2;
        else
            m_int32 = 0;
        if (!(turn > s_viewSettings[m_narrowView][m_int32].threshold))
            m_int32 = 0;
    }
    IAIPlayer* const player = m_bot->getPlayer();
    player->getFieldOfView();
    const float viewDistance = player->getViewDistance();
    m_frustum->setPerspective(s_viewSettings[m_narrowView][0].fov, 1.0f, 0.01f, viewDistance, camera);

    switch (m_int36)
    {
    case 0:
    {
        m_vector124.resize(0);
        const int side = m_bot->getSide();
        const unsigned int flags = equipment->getObjectTypeFlags();
        const bool soldierView = ((flags >> 4) & 1) || ((flags >> 5) & 1);
        SightPredicate predicate;
        predicate.m_side = side;
        predicate.m_position = equipment->getPosition();
        predicate.m_soldierView = soldierView;
        predicate.m_rangeSquared = viewDistance * viewDistance;
        const float near = equipment->getPhysical()->getRadius() * 3.0f;
        const float nearSquared = near * near;
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
            ->getWorldPlayerOccupiedObjects(m_vector124, &predicate);
        m_sortBuffer.resize(0);
        const Vec3& position = equipment->getPosition();
        for (std::vector<IAIObject*>::iterator it = m_vector124.begin(); it != m_vector124.end(); ++it)
        {
            IAIObject* const object = *it;
            Information* const information =
                informationManager->getInformation(object->getInformation(m_bot->getSide()));
            const float radius = object->getBoundingRadius();
            if (!m_frustum->inside(object->getPosition(), radius))
            {
                bool add;
                if (((equipment->getObjectTypeFlags() >> 21) & 1) ||
                    (information && !((information->getObjectTypeFlags() >> 21) & 1)))
                {
                    add = !(distanceSquared(position, object->getPosition()) > nearSquared);
                }
                else
                {
                    add = soldierView && isCloseBehind(equipment, object);
                }
                if (!add)
                    continue;
            }
            const unsigned int handle = object->forceGetInformation(side, false);
            const Vec3& at = object->getPosition();
            m_sortBuffer.push_back(std::make_pair(distanceSquared(position, at), handle));
            if (object->isUnit())
            {
                IAIObjectUnit* const unit = object->getUnit();
                if (unit->hasExposedSoldier() && unit->isOccupied())
                {
                    IAIObject* const occupier = unit->getOccupier();
                    const unsigned int occupierHandle = occupier->forceGetInformation(side, false);
                    m_sortBuffer.push_back(
                        std::make_pair(distanceSquared(occupier->getPosition(), position), occupierHandle));
                }
            }
            else
            {
                Debug(DebugTypeError, "AI/AIDLL/AI/AIBot/BotSenses.cpp", 413, "")
                    << object->getName() << " has no unit.";
            }
            int iterator;
            for (IAIObject* child = object->getFirstSecondaryObject(iterator); child;
                 child = object->getSecondaryObject(iterator))
            {
                IAIObjectUnit* const unit = child->getUnit();
                if (!unit || !unit->hasExposedSoldier() || !unit->isOccupied())
                    continue;
                IAIObject* const occupier = unit->getOccupier();
                if (!occupier)
                    continue;
                const unsigned int occupierHandle = occupier->forceGetInformation(side, false);
                m_sortBuffer.push_back(
                    std::make_pair(distanceSquared(occupier->getPosition(), position), occupierHandle));
            }
        }
        m_int40 = 0;
        m_double12 = timer->getGameTime();
        m_int36 = 1;
        if (timer->getUpdateTime() - start > slice)
            return;
    }
    // fall through
    case 1:
        equipment->getPosition();
        std::sort(m_sortBuffer.begin(), m_sortBuffer.end(), closerFirst);
        m_int36 = 2;
        if (timer->getUpdateTime() - start > slice)
            return;
    // fall through
    case 2:
    {
        const Vec3& position = equipment->getPosition();
        // The trace ignores the equipment's primary object, the target and an exposed soldier.
        // Once a target has none, the list stays at two entries for the rest of the run.
        std::vector<IAIObject*> ignore;
        ignore.push_back(equipment->getAIObject()->getPrimaryObject());
        ignore.push_back(0);
        ignore.push_back(0);
        IBotManager* const manager = IBotManager::instance;
        const bool render = manager->getRenderBotSensing();
        const int side = m_bot->getSide();
        IAIObjectEnvironment* const environment =
            static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
        double elapsed = timer->getUpdateTime() - start;
        equipment->getPhysical()->getRadius();
        while (m_int40 < static_cast<int>(m_sortBuffer.size()) && elapsed < slice)
        {
            const unsigned int handle = m_sortBuffer[m_int40].second;
            Information* information = 0;
            if (m_map44.find(handle) == m_map44.end())
                information = informationManager->getInformation(handle);
            if (information && information->isRootObject())
            {
                IAIObject* const object = information->getAIObject();
                Bot* const targetBot = manager->getBot(object);
                const bool targetIsBot = targetBot ? targetBot->getLodLevel() >= 1 : false;
                object->getBoundingRadius();
                ignore[1] = object;
                bool exposed = false;
                if (IPIUnit* const unit = information->getUnit())
                {
                    if ((information->getObjectTypeFlags() >> 21) & 1)
                    {
                        Information* const occupied =
                            informationManager->getInformation(unit->getOccupiedObject(m_bot->getSide()));
                        if (occupied && occupied->getUnit() && occupied->getUnit()->hasExposedSoldier())
                        {
                            if (ignore.size() > 2)
                                ignore[2] = occupied->getAIObject();
                            exposed = true;
                        }
                    }
                    else if (unit->hasExposedSoldier())
                    {
                        Information* const occupier =
                            informationManager->getInformation(unit->getOccupier(m_bot->getSide()));
                        if (occupier)
                        {
                            if (ignore.size() > 2)
                                ignore[2] = occupier->getAIObject();
                            exposed = true;
                        }
                    }
                }
                if (!exposed)
                    ignore.resize(2);

                Vec3 eye;
                eye.x = camera.m[12];
                eye.y = camera.m[13] - 0.1f;
                eye.z = camera.m[14];
                const Vec3& at = object->getPosition();
                const float distance = std::sqrt(distanceSquared(position, at));
                const float samplesFloat = distance > 0.0f
                    ? object->getBoundingRadius() / distance * 30.0f : 1e14f;
                int samples = static_cast<int>(samplesFloat);
                if (samples < 1)
                    samples = 1;
                else if (samples > 10)
                    samples = 10;
                unsigned int part = static_cast<unsigned int>(-1);
                bool clear = false;
                Vec3 normal;
                bool visible = false;
                for (int i = 0; i < samples; ++i)
                {
                    const bool soldier = ((information->getObjectTypeFlags() >> 21) & 1) != 0;
                    Vec3 target;
                    target.x = target.y = target.z = 0.0f;
                    normal.x = normal.y = normal.z = 0.0f;
                    if (!soldier && i == 0 && samples != 1)
                        target = object->getPosition();
                    else if (soldier)
                        environment->pickSoldierRandomSensePosition(target, normal, object, part);
                    else
                        environment->pickRandomSensePosition(target, normal, eye, object);
                    visible = !environment->collideLineWithWorld(eye, target, ignore, targetIsBot,
                                                                 0.8f, true, clear);
                    if (render)
                    {
                        // The sight line goes to the debug renderer (PE 10080660).
                        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: ai-debug-render]
                        // Recover the renderer and validate with bot sensing rendering enabled.
                        BF2_AI_FRONTIER("dice::hfe::ai::BotSenses::see (sensing render)");
                    }
                    if (visible)
                        break;
                }
                if (visible)
                {
                    Information* vehicle = equipment;
                    if (!vehicle->isPrimary())
                        vehicle = informationManager->getInformation(vehicle->getPrimaryObject(m_bot->getSide()));
                    if (Bot* const driver = manager->getBot(vehicle->getAIObject()))
                        driver->getSensingAgent()->getSenses()->addSeen(
                            handle, normal, part, information, driver == static_cast<Bot*>(m_bot), clear);
                    int iterator = -1;
                    for (unsigned int seat = vehicle->getFirstSecondaryObject(iterator, m_bot->getSide());;
                         seat = vehicle->getSecondaryObject(iterator, m_bot->getSide()))
                    {
                        Information* const seatInformation = informationManager->getInformation(seat);
                        if (!seatInformation)
                            break;
                        if (Bot* const passenger = manager->getBot(seatInformation->getAIObject()))
                            passenger->getSensingAgent()->getSenses()->addSeen(
                                handle, normal, part, information, passenger == static_cast<Bot*>(m_bot), clear);
                    }
                }
            }
            ++m_int40;
            elapsed = timer->getUpdateTime() - start;
        }
        if (m_int40 >= static_cast<int>(m_sortBuffer.size()))
        {
            m_int36 = 3;
            m_int40 = 0;
        }
        if (!(timer->getUpdateTime() - start > slice))
            timer->getUpdateTime();
        return;
    }
    case 3:
        timer->getUpdateTime();
        return;
    default:
        return;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
