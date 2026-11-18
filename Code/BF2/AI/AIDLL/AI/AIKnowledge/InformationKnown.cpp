#include "InformationKnown.h"

#include <cmath>

#include "BFInformationEnvironment.h"
#include "IPIMobile.h"
#include "IPIPhysical.h"
#include "IPIUnit.h"
#include "InformationManager.h"
#include "../AIBotBehaviour/CustomCurve.h"                           // SCurve::calculate
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

// The base time is -1, so the first setTime always refreshes. The cached transformation is set to
// identity before the object's transformation overwrites it, as shipped.
InformationKnown::InformationKnown(IAIObject* object, unsigned int types, double time,
                                   float degeneration, float basicTemp, float allowedTimeDiff,
                                   int side)
    : Information(object, -1.0)
{
    m_timeStamp = time;
    m_degeneration = degeneration;
    m_lastPosition.x = 0.0f;
    m_lastPosition.y = 0.0f;
    m_lastPosition.z = 0.0f;
    for (int i = 0; i < 16; ++i)
        m_lastTransformation.m[i] = 0.0f;
    m_lastTransformation.m[15] = 1.0f;
    m_lastTransformation.m[10] = 1.0f;
    m_lastTransformation.m[5] = 1.0f;
    m_lastTransformation.m[0] = 1.0f;
    m_contacts = 0;
    m_knownBySide = side;
    m_allowedTimeDiff = allowedTimeDiff;
    m_objectTypeFlags = types;
    m_strength = basicTemp;
    m_basicTemp = basicTemp;
    m_side = m_object->getSide();
    m_lastPosition = m_object->getPosition();
    m_lastTransformation = m_object->getTransformation();
}

// The vptr and the base destructor, which destroys the plug-ins.
InformationKnown::~InformationKnown()
{
}

void InformationKnown::cacheLastKnown()
{
    m_lastPosition = m_object->getPosition();
    m_lastTransformation = m_object->getTransformation();
    if (m_mobile)
        m_mobile->setLostContact();
}

// A known information is sort 1.
int InformationKnown::getInformationSort()
{
    return 1;
}

// Before the allowed time difference has passed only the time stamp moves. Otherwise: without
// contact the cache is refreshed; the mobile plug-in gets the time; a primary object passes the
// time to its secondaries on its side; the side is re-read; a unit refreshes its physical plug-in
// (not null-checked, as shipped) and its strength modifier; every plug-in gets the time; and a
// primary object updates its position in the information environment of the holding side.
bool InformationKnown::setTime(double time)
{
    if (!(static_cast<double>(m_allowedTimeDiff) + m_time < time))
    {
        m_timeStamp = time;
        return false;
    }
    if (m_contacts == 0)
        cacheLastKnown();
    if (m_mobile)
        m_mobile->setTime(time);
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();                                             // +17
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->setTime(time);                                           // +3
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
    m_side = m_object->getSide();
    if (m_unit)
    {
        m_physical->setTime(time);
        m_unit->calculateStrMod(m_physical->getHealth());
    }
    setTimeOnPlugIns(time);
    m_time = time;
    if (isPrimary())
    {
        const bool isUnit = m_unit != nullptr;
        static_cast<IAIInformationEnvironmentModifiers*>(
            CommonSingletonServer::m_iaiInformationEnvironmentModifiers)
            ->addAndUpdatePosition(m_knownBySide, isUnit, m_object->getInformation(m_knownBySide));
    }
    m_timeStamp = time;
    return true;
}

// A zero security leaves the time stamp at zero; any other security moves the stamp back by
// 1 / (security * degeneration) through a second refresh, after forcing it with base time -1.
bool InformationKnown::setTime(double time, float security)
{
    if (security < 0.0f)
        security = 0.0f;
    else if (security > 1.0f)
        security = 1.0f;
    if (!(getSecurity() < security))                                            // +11
        return false;
    setTime(time);
    if (security == 0.0f)
    {
        m_timeStamp = 0.0;
        return true;
    }
    m_time = -1.0;
    return setTime(time - 1.0 / (static_cast<double>(security) * m_degeneration));
}

double InformationKnown::getTimeStamp() const
{
    return m_timeStamp;
}

// Counts the contact, tells the mobile plug-in, and passes it to a primary object's secondaries.
void InformationKnown::addContact()
{
    ++m_contacts;
    if (m_mobile)
        m_mobile->setHasContact();
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->addContact();
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
}

// The last contact caches the position and transformation; secondaries follow either way.
void InformationKnown::removeContact()
{
    if (m_contacts > 0)
    {
        if (--m_contacts == 0)
            cacheLastKnown();
    }
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->removeContact();
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
}

void InformationKnown::removeAllContacts()
{
    if (m_contacts > 0)
    {
        m_contacts = 0;
        cacheLastKnown();
    }
    if (isPrimary())
    {
        int iterator = -1;
        const int side = getSide();
        Information* secondary =
            informationManager->getInformation(getFirstSecondaryObject(iterator, side));
        while (secondary)
        {
            secondary->removeAllContacts();
            secondary = informationManager->getInformation(getSecondaryObject(iterator, side));
        }
    }
}

bool InformationKnown::hasContact() const
{
    return m_contacts > 0;
}

// One minus the S-curve of the age in units of the degeneration.
float InformationKnown::getSecurity(double time)
{
    return 1.0f - SCurve::calculate(static_cast<float>((time - m_timeStamp) / m_degeneration));
}

// Full security with contact, otherwise the decayed security now.
float InformationKnown::getSecurity()
{
    if (m_contacts > 0)
        return 1.0f;
    return getSecurity(
        static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// [CHOSEN] cast, as in InformationReal.
Vec3& InformationKnown::getPosition()
{
    if (m_contacts)
        return const_cast<Vec3&>(m_object->getPosition());
    return m_lastPosition;
}

float InformationKnown::getAltitude()
{
    if (m_contacts)
        return AerialHeightMap::m_instance->getAltitude(m_object->getPosition());
    return AerialHeightMap::m_instance->getAltitude(m_lastPosition);
}

// InformationReal::getAltitude(direction) from the live or the cached position.
float InformationKnown::getAltitude(const Vec3& direction)
{
    Vec3 probe;
    probe.x = 0.0f;
    probe.y = 0.0f;
    probe.z = 0.0f;
    const float length = static_cast<float>(std::sqrt(
        static_cast<double>(direction.x) * direction.x +
        static_cast<double>(direction.y) * direction.y +
        static_cast<double>(direction.z) * direction.z));
    if (m_contacts)
        probe = m_object->getPosition();
    else
        probe = m_lastPosition;
    float lowest = AerialHeightMap::m_instance->getAltitude(probe);

    const float stepX = static_cast<float>(direction.x * 0.1);
    const float stepY = static_cast<float>(direction.y * 0.1);
    const float stepZ = static_cast<float>(direction.z * 0.1);
    const double stepLength = length * 0.1;
    const float step = static_cast<float>(stepLength);
    float travelled = step;
    probe.x = static_cast<float>(static_cast<double>(stepX) + probe.x);
    probe.y = static_cast<float>(static_cast<double>(stepY) + probe.y);
    probe.z = static_cast<float>(static_cast<double>(stepZ) + probe.z);
    if (stepLength < length)
    {
        double next;
        do
        {
            probe.x = static_cast<float>(static_cast<double>(stepX) + probe.x);
            probe.y = static_cast<float>(static_cast<double>(stepY) + probe.y);
            probe.z = static_cast<float>(static_cast<double>(stepZ) + probe.z);
            const float altitude = AerialHeightMap::m_instance->getAltitude(probe);
            if (lowest >= altitude)
                lowest = altitude;
            next = static_cast<double>(travelled) + step;
            travelled = static_cast<float>(next);
        }
        while (next < length);
    }
    return lowest;
}

Mat4& InformationKnown::getTransformation()
{
    if (m_contacts)
        return const_cast<Mat4&>(m_object->getTransformation());
    return m_lastTransformation;
}

// The cached transformation's third row, even with contact.
Vec3 InformationKnown::getFaceing()
{
    Vec3 facing;
    facing.x = m_lastTransformation.m[8];
    facing.y = m_lastTransformation.m[9];
    facing.z = m_lastTransformation.m[10];
    return facing;
}

int InformationKnown::getSide() const
{
    return m_side;
}

} // namespace ai
} // namespace hfe
} // namespace dice
