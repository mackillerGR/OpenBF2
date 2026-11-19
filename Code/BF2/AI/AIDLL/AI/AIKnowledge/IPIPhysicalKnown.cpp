#include "IPIPhysicalKnown.h"
#include "Information.h"
#include "../AIBotBehaviour/CustomCurve.h"                          // SCurve::calculate (static)
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"        // IAIObject, IAIObjectPhysical

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAIObjectPhysical* livePhysical(Information* information)
{
    return information->getAIObject()->getPhysical();
}

} // anonymous namespace

IPIPhysicalKnown::IPIPhysicalKnown(Information* information)
{
    m_information = information;
    m_touchingTerrain = false;
    m_terrainPosition.x = 0.0f;
    m_terrainPosition.y = 0.0f;
    m_terrainPosition.z = 0.0f;
    m_inAir = false;
    m_inWater = false;
    m_manDown = false;
}

IPIPhysicalKnown::~IPIPhysicalKnown()
{
}

void IPIPhysicalKnown::setTime()
{
    setTime(static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// Ignores the time. Without a positive maximum health the cached health is zero; the modifier is
// 0.5 + 0.5 * SCurve(health), as in IPIPhysicalReal, but here the absolute health is cached.
void IPIPhysicalKnown::setTime(double /*time*/)
{
    IAIObjectPhysical* const physical = livePhysical(m_information);
    m_manDown = physical->getManDown();
    if (physical->getMaxHealth() > 0.0f)
        m_health = physical->getHealth();
    else
        m_health = 0.0f;
    m_mod = 0.5f;
    m_mod = static_cast<float>(static_cast<double>(SCurve::calculate(m_health)) * 0.5 + m_mod);
    m_touchingTerrain = physical->isTouchingTerrain(m_terrainPosition);
    m_inAir = physical->isInAir();
    m_inWater = physical->isInWater();
}

float IPIPhysicalKnown::getHealth() const
{
    return m_health;
}

void IPIPhysicalKnown::setHealth(float health)
{
    m_health = health;
}

float IPIPhysicalKnown::getMaxHealth()
{
    return livePhysical(m_information)->getMaxHealth();
}

float IPIPhysicalKnown::getMod()
{
    return m_mod;
}

BoundingBox* IPIPhysicalKnown::getBoundingBox()
{
    return livePhysical(m_information)->getCurrentBoundingBox();
}

const LocalBoundingBox* IPIPhysicalKnown::getLocalBoundingBox()
{
    return livePhysical(m_information)->getLocalBoundingBox();
}

float IPIPhysicalKnown::getRadius()
{
    return livePhysical(m_information)->getRadius();
}

Vec3 IPIPhysicalKnown::getSphereLocalOffset() const
{
    return livePhysical(m_information)->getSphereLocalOffset();
}

float IPIPhysicalKnown::getSmallestRadius() const
{
    return livePhysical(m_information)->getSmallestRadius();
}

void IPIPhysicalKnown::getSubSpheres2d(std::vector<Vec2>& out) const
{
    livePhysical(m_information)->getSubSpheres2d(out);
}

float IPIPhysicalKnown::getArmour(Vec3& direction)
{
    return livePhysical(m_information)->getArmour(direction);
}

float IPIPhysicalKnown::getBestArmour()
{
    return livePhysical(m_information)->getBestArmour();
}

int IPIPhysicalKnown::getStrengthType()
{
    return livePhysical(m_information)->getStrType();
}

float IPIPhysicalKnown::getVolume()
{
    return livePhysical(m_information)->getVolume();
}

float IPIPhysicalKnown::getMass()
{
    return livePhysical(m_information)->getMass();
}

int IPIPhysicalKnown::getMaterial() const
{
    return livePhysical(m_information)->getMaterial();
}

float IPIPhysicalKnown::getManDownTimeLeft() const
{
    return livePhysical(m_information)->getManDownTimeLeft();
}

float IPIPhysicalKnown::getMaxManDownTime() const
{
    return livePhysical(m_information)->getMaxManDownTime();
}

bool IPIPhysicalKnown::getManDown() const
{
    return m_manDown;
}

bool IPIPhysicalKnown::isTouchingTerrain(Vec3& position) const
{
    position = m_terrainPosition;
    return m_touchingTerrain;
}

bool IPIPhysicalKnown::isInAir() const
{
    return m_inAir;
}

bool IPIPhysicalKnown::isInWater() const
{
    return m_inWater;
}

} // namespace ai
} // namespace hfe
} // namespace dice
