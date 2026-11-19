#include "IPIArmamentKnown.h"

#include "WeaponInfo.h"
#include "WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// The stale mark is raised around the first build and cleared directly after it, as shipped.
IPIArmamentKnown::IPIArmamentKnown(Information* information)
    : IPIArmament(information)
{
    m_weaponsStale = true;
    updateWeaponsInternal(false);
    m_weaponsStale = false;
}

IPIArmamentKnown::~IPIArmamentKnown()
{
    for (std::vector<unsigned int>::iterator it = m_weapons.begin(); it != m_weapons.end(); ++it)
        WeaponInfoManager::m_instance->freeInformation(*it);
}

// The time itself is not passed on: each weapon info only gets its parameterless update.
void IPIArmamentKnown::setTime(double /*time*/)
{
    if (m_weaponsStale)
        rebuildWeapons();
    WeaponInfoManager* const manager = WeaponInfoManager::m_instance;
    for (unsigned int i = 0; i < m_weapons.size(); ++i)
    {
        const unsigned int handle = m_weapons[i];
        if (handle == static_cast<unsigned int>(-1))
            continue;
        WeaponInfo* const weapon = manager->getInformation(handle);
        if (weapon)
            weapon->update();
    }
}

unsigned int IPIArmamentKnown::getWeapon(int index)
{
    if (m_weaponsStale)
        rebuildWeapons();
    return m_weapons[index];
}

std::vector<unsigned int>& IPIArmamentKnown::getWeapons()
{
    if (m_weaponsStale)
        rebuildWeapons();
    return m_weapons;
}

void IPIArmamentKnown::updateWeapons()
{
    m_weaponsStale = true;
}

void IPIArmamentKnown::rebuildWeapons()
{
    updateWeaponsInternal(false);
    m_weaponsStale = false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
