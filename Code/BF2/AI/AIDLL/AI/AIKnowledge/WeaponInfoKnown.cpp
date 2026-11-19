#include "WeaponInfoKnown.h"

#include "../AIBotBehaviour/CustomCurve.h"                           // SCurve::calculate
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"                // IBFTimer::getGameTime
#include "../../../AIInterface/BFKnowledge/IWeapon.h"

namespace dice { namespace hfe { namespace ai {

namespace {

double gameTime()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

} // anonymous namespace

WeaponInfoKnown::WeaponInfoKnown(IWeapon* weapon)
    : WeaponInfo(weapon), m_shotsInMags()
{
    m_ammo = m_weapon->getAmmo();
    m_currentAmmo = m_weapon->getCurrentAmmo();
    m_readyToFire = m_weapon->isReadyToFire();
    m_heat = m_weapon->getHeat();
    m_timeOfFiring = m_weapon->getTimeOfFiring();
    m_timeStamp = gameTime();
    for (int i = 0; i < m_weapon->getNumberOfMags(); ++i)
        m_shotsInMags.push_back(m_weapon->getAmmoInMag(i));
}

WeaponInfoKnown::~WeaponInfoKnown()
{
}

bool WeaponInfoKnown::update()
{
    const double now = gameTime();
    if (!(now > m_timeStamp + 1.0))
        return true;
    m_ammo = m_weapon->getAmmo();
    m_currentAmmo = m_weapon->getCurrentAmmo();
    m_readyToFire = m_weapon->isReadyToFire();
    m_heat = m_weapon->getHeat();
    m_timeOfFiring = m_weapon->getTimeOfFiring();
    m_timeStamp = now;
    m_shotsInMags.clear();
    for (int i = 0; i < m_weapon->getNumberOfMags(); ++i)
        m_shotsInMags.push_back(m_weapon->getAmmoInMag(i));
    return true;
}

int WeaponInfoKnown::getShotsInMag(int magazine) { return m_shotsInMags[magazine]; }
int WeaponInfoKnown::getAmmo() const { return m_ammo; }
int WeaponInfoKnown::getCurrentAmmo() const { return m_currentAmmo; }
int WeaponInfoKnown::getNumberOfMags() const { return static_cast<int>(m_shotsInMags.size()); }
bool WeaponInfoKnown::isReadyToFire() const { return m_readyToFire; }
double WeaponInfoKnown::getTimeOfFiring() const { return m_timeOfFiring; }
float WeaponInfoKnown::getHeat() const { return m_heat; }
bool WeaponInfoKnown::usesHeat() const { return m_weapon->usesHeat(); }

float WeaponInfoKnown::getSecurity() const
{
    return getSecurity(gameTime());
}

// One minus the S-curve of the snapshot's age in units of five seconds.
float WeaponInfoKnown::getSecurity(double time) const
{
    return 1.0f - SCurve::calculate(static_cast<float>((time - m_timeStamp) * 0.2));
}

} } }
