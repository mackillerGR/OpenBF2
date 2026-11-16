// [FRONTIER] Remaining methods stop by name until implemented.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BFInformationEnvironment operations.

#include "BFInformationEnvironment.h"

#include <string>

#include "AIInformationGrid.h"                             // the grids the constructor builds
#include "Information.h"
#include "InformationManager.h"
#include "ObjectOccupancy.h"
#include "UnitPredicate.h"
#include "../AIBot/BotMain.h"
#include "../AIUtils/checkPointer.h"                       // inlined four times by the constructor
#include "../../../../Core/ClassManager.h"
#include "../../../../Core/SmartItf.h"           // the classManager global
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"                      // getNSides, getInformationGridDimension
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"        // getWorldSizeX / getWorldSizeZ

namespace dice {
namespace hfe {
namespace ai {

// Registers query and modifier interfaces in their respective singleton rosters.
IAIInformationEnvironment::IAIInformationEnvironment()
{
    if (!AISingletonServer::m_iaiInformationEnvironment)
        classManager->registerSimpleSingleton(std::string("IAIInformationEnvironment"), this);
}

IAIInformationEnvironment::~IAIInformationEnvironment()
{
    if (AISingletonServer::m_iaiInformationEnvironment == this)
        classManager->unregisterSimpleSingleton(std::string("IAIInformationEnvironment"));
}

IAIInformationEnvironmentModifiers::IAIInformationEnvironmentModifiers()
{
    if (!CommonSingletonServer::m_iaiInformationEnvironmentModifiers)
        classManager->registerSimpleSingleton(std::string("IAIInformationEnvironmentModifiers"), this);
}

IAIInformationEnvironmentModifiers::~IAIInformationEnvironmentModifiers()
{
    if (CommonSingletonServer::m_iaiInformationEnvironmentModifiers == this)
        classManager->unregisterSimpleSingleton(std::string("IAIInformationEnvironmentModifiers"));
}

// Creates two spatial grids for each side, including neutral side zero.
// Grid bounds run from zero to the terrain dimensions.
BFInformationEnvironment::BFInformationEnvironment()
{
    const IAITerrain* const terrain =
        static_cast<const IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const IAISettings* const settings =
        static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

    for (int side = 0; side < settings->getNSides() + 1; ++side)
    {
        // Queries terrain dimensions and grid settings separately for each grid allocation.
        // Preserve the repeated virtual calls instead of sharing cached values.
        {
            // Not centred on the origin - see the block above, and `AIInformationGrid::getMinMax`
            // which divides by the maximum corner alone and so requires this rectangle to start at
            // the origin.
            // `Vec2` is a plain aggregate here, with no two-argument constructor; the binary sets
            // the four floats field by field and so does this.
            Vec2 min;
            min.x = 0.0f;
            min.y = 0.0f;

            Vec2 max;
            max.x = static_cast<float>(terrain->getWorldSizeX());
            max.y = static_cast<float>(terrain->getWorldSizeZ());

            // One number, asked for twice - see the block above. The ELF evaluates the third
            // argument before the second, which is GCC pushing right to left; unobservable, because
            // it is one function called twice.
            m_gridsA.push_back(checkPointer(
                new AIInformationGrid(side, settings->getInformationGridDimension(),
                                      settings->getInformationGridDimension(), min, max)));
        }

        {
            Vec2 min;
            min.x = 0.0f;
            min.y = 0.0f;

            Vec2 max;
            max.x = static_cast<float>(terrain->getWorldSizeX());
            max.y = static_cast<float>(terrain->getWorldSizeZ());

            m_gridsB.push_back(checkPointer(
                new AIInformationGrid(side, settings->getInformationGridDimension(),
                                      settings->getInformationGridDimension(), min, max)));
        }
    }
}

// [FRONTIER] Unimplemented environment queries and modifiers stop by name.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

BFInformationEnvironment::~BFInformationEnvironment()
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFInformationEnvironment::~BFInformationEnvironment");
}

void BFInformationEnvironment::getFriendlyUnits(std::vector<unsigned int>&, const Vec3&, Bot*, float) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFInformationEnvironment::getFriendlyUnits");
}

namespace {

// PE 10191B90: a unit of the given side or of none. [INFERRED] name.
class FriendlyOrNeutralPredicate : public IAIInformationGridPredicate
{
public:
    explicit FriendlyOrNeutralPredicate(int side) : m_side(side) {}
    bool includeInformation(Information* information) const override
    {
        return information->getUnit()
               && (information->getSide() == m_side || information->getSide() == 0);
    }
private:
    int m_side;
};

} // anonymous namespace

// PE 10192E80. Releases the output's storage first; nothing more without equipment.
void BFInformationEnvironment::getFriendlyAndNeutralUnits(std::vector<unsigned int>& out,
                                                          const Vec3& position, Bot* bot,
                                                          float radius) const
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    std::vector<unsigned int>().swap(out);
    if (!equipment)
        return;
    FriendlyOrNeutralPredicate friendly(bot->getSide());
    getObjectsWithinRadius(out, position, radius, bot->getSide(), true, &friendly);
}

void BFInformationEnvironment::getCover(const Bot&, std::vector<unsigned int>&) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFInformationEnvironment::getCover");
}

namespace {

// The query predicates the environment builds on the stack. Each is a one-slot class at PE
// 10282860..1028286C; names [INFERRED].

// PE 10191BF0: both predicates hold.
class BothPredicate : public IAIInformationGridPredicate
{
public:
    BothPredicate(IAIInformationGridPredicate* first, IAIInformationGridPredicate* second)
        : m_first(first), m_second(second) {}
    bool includeInformation(Information* information) const override
    {
        return m_first->includeInformation(information) && m_second->includeInformation(information);
    }
private:
    IAIInformationGridPredicate* m_first;
    IAIInformationGridPredicate* m_second;
};

// PE 10191C50: anything but the given information.
class OtherThanPredicate : public IAIInformationGridPredicate
{
public:
    explicit OtherThanPredicate(Information* excluded) : m_excluded(excluded) {}
    bool includeInformation(Information* information) const override
    {
        return m_excluded != information;
    }
private:
    Information* m_excluded;
};

// PE 10191CA0: a unit of another side that is occupied.
class OccupiedEnemyPredicate : public IAIInformationGridPredicate
{
public:
    explicit OccupiedEnemyPredicate(int side) : m_side(side) {}
    bool includeInformation(Information* information) const override
    {
        return information->getUnit() && m_side != information->getSide()
               && isOccupiedObject(information->getHandle(), m_side);
    }
private:
    int m_side;
};

} // anonymous namespace

// PE 10192160. Units within 75 m of the position, other than the bot's equipment; 0 without
// equipment.
float BFInformationEnvironment::getHardware(std::vector<unsigned int>& out, const Vec3& position,
                                            Bot* bot) const
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (!equipment)
        return 0.0f;
    return getHardware(out, position, 75.0f, bot->getSide(), equipment);
}

// PE 101921D0. Units within the radius on both grids, excluding the given information when
// there is one; returns the radius.
float BFInformationEnvironment::getHardware(std::vector<unsigned int>& out, const Vec3& position,
                                            float radius, int side, Information* excluded) const
{
    UnitPredicate units;
    OtherThanPredicate other(excluded);
    BothPredicate both(&units, &other);
    getObjectsWithinRadius(out, position, radius, side, true,
                           excluded ? static_cast<IAIInformationGridPredicate*>(&both) : &units);
    return radius;
}

// PE 10192230. Occupied enemy units around the bot; a negative radius means 600 m for
// equipment with flag bit 4 and 75 m otherwise.
void BFInformationEnvironment::getEnemyObjects(std::vector<unsigned int>& out, Bot* bot,
                                               float radius) const
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (radius < 0.0f)
        radius = ((equipment->getObjectTypeFlags() >> 4) & 1) ? 600.0f : 75.0f;
    OccupiedEnemyPredicate enemies(bot->getSide());
    getObjectsWithinRadius(out, radius, *bot, true, &enemies);
}

// PE 101922D0. Around the bot's equipment, when it has one.
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out, float radius,
                                                     const Bot& bot, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    Bot& mutableBot = const_cast<Bot&>(bot);
    Information* const equipment = informationManager->getInformation(mutableBot.getCurrentEquipment());
    if (equipment)
        getObjectsWithinRadius(out, equipment->getPosition(), radius, mutableBot.getSide(), which, keep);
}

// flatten XZ, then delegate
// virtually to the Vec2 overload (ELF +0x28 / PE +0x1C).
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out,
                                                     const Vec3& position, float radius,
                                                     int side, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    vec3ToVec2XZ(position, flat);
    getObjectsWithinRadius(out, flat, radius, side, which, keep);
}

// Query side zero first
// then look up the requested side after that callback. Both calls remain when
// side is zero. The grid query is const; output is appended to, never cleared.
void BFInformationEnvironment::getObjectsWithinRadius(std::vector<unsigned int>& out,
                                                     const Vec2& position, float radius,
                                                     int side, bool which,
                                                     IAIInformationGridPredicate* keep) const
{
    getInformationGrid(0, which)->getInformationWithinCircle(out, position, radius, keep);
    getInformationGrid(side, which)->getInformationWithinCircle(out, position, radius, keep);
}

// Query neutral side zero first
// then select the requested side after that callback. Both calls borrow the same corners
// output and predicate. Side zero is queried twice when requested; output is never cleared.
void BFInformationEnvironment::getObjectsWithinRectangle(std::vector<unsigned int>& out,
                                                        const Vec2& min, const Vec2& max,
                                                        int side, bool which,
                                                        IAIInformationGridPredicate* keep) const
{
    getInformationGrid(0, which)->getInformationWithinRectangle(out, min, max, keep);
    getInformationGrid(side, which)->getInformationWithinRectangle(out, min, max, keep);
}

void BFInformationEnvironment::getObjectsWithinFrustum(std::vector<unsigned int>&, const Frustum&,
                                                      int, bool, IAIInformationGridPredicate*) const
{
    BF2_AI_FRONTIER("dice::hfe::ai::BFInformationEnvironment::getObjectsWithinFrustum");
}

void BFInformationEnvironment::getAllInformation(std::vector<unsigned int>& out, int side, bool which,
                                                 IAIInformationGridPredicate* keep) const
{
    // Const environment access still invokes the mutable
    // grid sweep, including stale-record removal; it does not use the read-only overload.
    getInformationGrid(side, which)->getAllInformation(out, keep, nullptr);
}

void BFInformationEnvironment::getAllInformation(std::vector<unsigned int>& out, int side, bool which,
                                                 IAIInformationGridPredicate* keep,
                                                 IAIInformationGridPredicate* cull)
{
    // No side/grid guard and no clearing of the output.
    getInformationGrid(side, which)->getAllInformation(out, keep, cull);
}

// the side's grid (unit or other, by `which`) takes the handle
// IAIInformationGrid +2 (ELF +0x08, PE +0x04). No side guard, as in getInformationGrid. The PE
// inlines the grid lookup; the ELF calls it.
void BFInformationEnvironment::addInfo(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->addInfo(handle);
}

// the same, through +6 addAndUpdatePosition (ELF +0x18, PE
// +0x14).
void BFInformationEnvironment::addAndUpdatePosition(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->addAndUpdatePosition(handle);
}

// The grid's +5 updatePosition (PE +0x10); its result is discarded. PE 10192B40.
void BFInformationEnvironment::updatePosition(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->updatePosition(handle);
}

// The information drops all contacts before the grid forgets the handle. The handle is looked up
// without a null check, as in retail. PE 10192B80.
void BFInformationEnvironment::removeInfo(int side, bool which, unsigned int handle)
{
    informationManager->getInformation(handle)->removeAllContacts();
    getInformationGrid(side, which)->removeInfo(handle);
}

// As removeInfo, through the grid's safe removal with no predicate. PE 10192BF0.
void BFInformationEnvironment::safeRemoveInfo(int side, bool which, unsigned int handle)
{
    informationManager->getInformation(handle)->removeAllContacts();
    getInformationGrid(side, which)->safeRemoveInfo(handle, 0);
}

// The grid's safe removal alone: the information may already be gone. PE 10192C60.
void BFInformationEnvironment::destroyRemoveInfo(int side, bool which, unsigned int handle)
{
    getInformationGrid(side, which)->safeRemoveInfo(handle, 0);
}

// Selects one of two grid sets by which, then indexes it by side.
// The caller must supply a valid side; no bounds check is performed.
IAIInformationGrid* BFInformationEnvironment::getInformationGrid(int side, bool which) const
{
    return which ? m_gridsB[side] : m_gridsA[side];
}

} // namespace ai
} // namespace hfe
} // namespace dice
