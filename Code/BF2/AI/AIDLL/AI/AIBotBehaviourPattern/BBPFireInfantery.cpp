// [INFERRED] the class comment; the source path is attested by the image's __FILE__ string.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSenses.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPConditions.h"
#include "../AIBotActionPlan/BAPStatements.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AIRandom.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <cmath>
#include <vector>

#include "../../../../Core/Debug.h"

namespace dice {
namespace hfe {
namespace ai {

BBPFireInfantery::BBPFireInfantery(int behaviourId)
{
    m_behaviourId = behaviourId;
    const int bots = configuredBotCount();
    FireInfanteryTarget* const targets = new FireInfanteryTarget[bots];
    for (int i = 0; i < bots; ++i)
    {
        FireInfanteryTarget& t = targets[i];
        t.m_bool0C = false;
        t.m_bool14 = false;
        t.m_bool15 = false;
        t.m_bool16 = false;
        t.m_bool17 = false;
        t.m_target = static_cast<unsigned int>(-1);
        t.m_weapon = -1;
        t.m_int08 = -1;
        t.m_vec18.x = t.m_vec18.y = t.m_vec18.z = 0.0f;
        t.m_bool24 = false;
        t.m_bool25 = false;
        t.m_bool26 = false;
        t.m_vec28.x = t.m_vec28.y = t.m_vec28.z = 0.0f;
        t.m_float34 = 0.0f;
    }
    m_targetState = checkPointer(targets);
    m_aim = checkPointer(new FireInfanteryAim[configuredBotCount()]);
}

BBPFireInfantery::~BBPFireInfantery()
{
    delete[] m_targetState;
    delete[] m_aim;
}

int BBPFireInfantery::getFireType()
{
    return 5;
}

// PE 10105300.
void BBPFireInfantery::setPositionTarget(Bot* bot, int designator, const Vec3& position, int weapon)
{
    const unsigned int id = bot->getBotId();
    m_hasPlan[id] = true;
    m_targets[id] = static_cast<unsigned int>(-1);
    m_weapons[id] = weapon;
    m_designators[id] = designator;
    m_positions[id] = position;
    m_aim[id].m_state = 0;
    m_aim[id].m_subState = 0;
    m_aim[id].m_stateTime = 0.0;
    m_aim[id].m_stateDuration = 10.0;
    m_targetState[id].m_target = static_cast<unsigned int>(-1);
}

FireInfanteryAim::FireInfanteryAim()
    : m_state(0)
    , m_history()
    , m_subState(0)
    , m_stateTime(0.0)
    , m_stateDuration(10.0)
    , m_int58(-1)
    , m_int5C(-1)
{
    for (int k = 0; k < 3; ++k)
        m_zero30[k] = 0;
    m_aimPosition.x = m_aimPosition.y = m_aimPosition.z = 0.0f;
    m_aimOffset.x = m_aimOffset.y = m_aimOffset.z = 0.0f;
}

// PE 10103FC0. The flags are not used. A bot without a plan also restarts its aim.
void BBPFireInfantery::setObjectTarget(Bot* bot, unsigned int target, int weapon, bool, bool)
{
    const unsigned int id = bot->getBotId();
    m_targetChanged[id] = target != m_targets[id];
    m_targets[id] = target;
    m_weapons[id] = weapon;
    if (!m_hasPlan[id])
    {
        m_aim[id].m_state = 0;
        m_aim[id].m_subState = 0;
        m_aim[id].m_stateTime = 0.0;
        m_aim[id].m_stateDuration = 10.0;
        m_targetState[id].m_target = static_cast<unsigned int>(-1);
    }
    m_hasPlan[id] = true;
}

namespace {

const float kNormaliseFloor = 1e-4f;        // PE 1027EB38
const double kAimForget = 5.0;              // PE 1027EB60: seconds before a stale aim restarts
const float kHistoryLength = 8.0f;          // PE 1027EC40
const float kRepeatScale = 0.1f;            // PE 1027B220
const float kRepeatFloor = 1e-5f;           // PE 1027C16C
const float kMovingScale = 0.2f;            // PE 1027E4DC
const float kHoldSkill = 0.3f;              // PE 1027E470
const float kStandOff = 5.0f;               // PE 1027B20C
const float kOverlapShare = 0.75f;          // PE 1027E474
const float kRadiusMargin = 2.0f;           // PE 1027BECC
const float kAimMoved = 10.0f;              // PE 1027B228
const float kRangeMargin = 3.0f;            // PE 1027E3A0

const IBFTimer* fireTimer()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
}

// PE 101053A0: the distance between two points in the horizontal plane.
float horizontalDistance(Vec3 a, Vec3 b)
{
    const float dx = b.x - a.x;
    const float dz = b.z - a.z;
    return std::sqrt(dz * dz + dx * dx);
}

// PE 101053D0: the first candidate offered becomes the default choice; a candidate is taken with
// the given probability.
bool offerChoice(int& choice, int candidate, float probability)
{
    if (choice == 0)
        choice = candidate;
    const double roll = static_cast<double>(AIRandom::m_instance->next()) * 2.3283064365386963e-10;
    if (roll < probability) {
        choice = candidate;
        return true;
    }
    return false;
}

// PE 10105430: for a weapon that is out of range, a position at the right distance on the line
// to the target (snapped to the bot's navigation mesh) and a look point ten metres before the
// target. False when the target is already within range.
bool chooseFirePosition(Vec3& position, Vec3& lookPoint, Information* own, IPIMobile* mobile,
                        int equipmentType, WeaponInfo* weapon, Information* target,
                        const Vec3& targetPosition)
{
    const Vec3& ownPosition = own->getPosition();
    Vec3 toward = {targetPosition.x - ownPosition.x, targetPosition.y - ownPosition.y,
                   targetPosition.z - ownPosition.z};
    const float length = std::sqrt(toward.z * toward.z + toward.y * toward.y +
                                   toward.x * toward.x);
    if (length > kNormaliseFloor) {
        const float inverse = 1.0f / length;
        toward.x *= inverse;
        toward.y *= inverse;
        toward.z *= inverse;
    }
    lookPoint.x = targetPosition.x - toward.x * kAimMoved;
    lookPoint.y = targetPosition.y - toward.y * kAimMoved;
    lookPoint.z = targetPosition.z - toward.z * kAimMoved;
    const float beyond = length - kAimMoved;

    Vec3 from;
    if (weapon->getMinRange() > beyond) {
        const float back = weapon->getMinRange() + kRangeMargin - beyond;
        const Vec3& origin = own->getPosition();
        from.x = origin.x - toward.x * back;
        from.y = origin.y - toward.y * back;
        from.z = origin.z - toward.z * back;
    } else {
        if (weapon->getMaxRange() >= beyond)
            return false;
        // The shipped body measures the approach from the minimum range here too.
        const float ahead = beyond + kRangeMargin - weapon->getMinRange();
        const Vec3& origin = own->getPosition();
        from.x = toward.x * ahead + origin.x;
        from.y = toward.y * ahead + origin.y;
        from.z = toward.z * ahead + origin.z;
    }
    unsigned int quadTree;
    unsigned int polygon;
    IAIPathfinding::instance->findClosestValidPolygon(
        quadTree, polygon, position, mobile->getVehicleNumber(), from, target->getPosition(), 10,
        equipmentType);
    return true;
}

// Shared opening of the generators: resets the bot's script debugger and clears the script
// variables. [CHOSEN, not recovered] Retail inlines this into each generate.
void releaseScriptState(Bot* bot)
{
    if (bot->getDebugger()) {
        // Retail calls the debugger's third slot; the debugger type is not recovered.
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-debugger]
        BF2_AI_FRONTIER("dice::hfe::ai::PlanGenerator (script debugger reset)");
    }
    bot->getScriptContext()->clear();
}

void dropPlan(Bot* bot)
{
    if (bot->getPlan()) {
        if (BotScript* const plan = bot->getPlan())
            delete plan;
        bot->setPlan(0);
    }
}

} // anonymous namespace

// PE 1010C110. A per-bot state machine over the firing actions (1 aim and fire bursts, 2 single
// shots, 4 reposition into range, 5 close in) keeps the current fire plan while it still matches
// the target, weapon and static-attack setting; otherwise it decides whether the current action
// is finished, picks the next one at random weighted against recent repeats and the line of fire,
// fills the bot's target state for it and asks the script builder for a plan. A vehicle target
// with a sensed occupant is engaged through the occupant.
BotScript* BBPFireInfantery::generate(Bot* bot)
{
    const unsigned int id = bot->getBotId();
    FireInfanteryTarget* const plan = &m_targetState[id];
    FireInfanteryAim* const aim = &m_aim[id];
    if (!m_hasPlan[id]) {
        releaseScriptState(bot);
        dropPlan(bot);
        return 0;
    }

    // The equipment lookup is not checked, as shipped.
    Information* const own = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = own->getArmament();
    IPIPhysical* const physical = own->getPhysical();
    IPIControlInfo* const control = own->getControlInfo();
    IAIPlayer* const player = bot->getPlayer();
    if (!armament || !physical || !control || !player) {
        m_hasPlan[id] = false;
        return bot->getPlan();
    }
    const int equipmentType = own->getUnit()->getEquipmentType();
    const Vec3& ownPosition = own->getPosition();
    IPIMobile* const mobile = own->getMobile();
    bool aimWhileDriving = false;
    if (mobile) {
        aimWhileDriving = (control->getAimControls() & control->getDriveControls()) == 0;
        mobile->getSector();   // the result is unused, as shipped
    }

    m_holdFire = !(bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId
                   && !(aiSettings()->getBotSkill() < kHoldSkill));

    // Keep a running fire plan that still matches.
    int planWeapon = -1;
    if (bot->hasPlan() && bot->getPlan()->getId() == m_behaviourId
        && bot->getPlan()->getType() == BAPSTInfoWrapper
        && static_cast<BAPWrapper*>(bot->getPlan())->getWrapperIndex() == 0) {
        BAPFireWrapper* const running = static_cast<BAPFireWrapper*>(bot->getPlan());
        if (running->m_target == m_targets[id] && running->m_flag2C == m_flagA) {
            planWeapon = running->m_weapon;
            if (planWeapon == m_weapons[id]
                && static_cast<BAPFireWrapper*>(bot->getPlan())->m_useStaticAttack
                       == static_cast<BotMain*>(bot)->getUseStaticAttack())
                return bot->getPlan();
        }
    }

    // The target, or the sensed occupant of a target that always exposes one.
    unsigned int targetHandle = m_targets[id];
    Information* target = informationManager->getInformation(targetHandle);
    IPIUnit* const unit = target ? target->getUnit() : 0;
    Information* vehicle = 0;
    const SensedObject* sensed = 0;
    if (unit && unit->getAlwaysTargetSoldier()) {
        Information* const occupant =
            informationManager->getInformation(unit->getOccupiedObject(bot->getSide()));
        if (occupant) {
            sensed = static_cast<const SensedObject*>(
                bot->getSensedObjectFromHandle(unit->getOccupiedObject(bot->getSide())));
            if (sensed) {
                vehicle = target;
                targetHandle = unit->getOccupiedObject(bot->getSide());
                target = occupant;
            }
        }
    }
    IPIPhysical* const targetPhysical = target ? target->getPhysical() : 0;
    if (!sensed)
        sensed = static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(targetHandle));
    if (!target || !targetPhysical || !sensed) {
        m_hasPlan[id] = false;
        return bot->getPlan();
    }

    Bot* const targetBot = IBotManager::instance->getBot(target->getAIObject());
    const bool remoteTarget = targetBot && static_cast<int>(targetBot->getLodLevel()) >= 1;
    const Vec3 targetPosition = target->getPosition();
    if (target->getMobile())
        target->getMobile()->getSector();   // the result is unused, as shipped

    SensedObject* const record = const_cast<SensedObject*>(sensed);
    if (!(record->m_senseFlags & 0x20)) {
        record->m_int4C = 0;
        for (int k = 0; k < 4; ++k)
            record->m_unresolved48[k] = 0;
        record->m_senseFlags |= 0x20;
        record->m_lastSense = 0x20;
    }

    std::vector<IAIObject*> ignore;
    ignore.push_back(own->getAIObject());
    ignore.push_back(target->getAIObject());
    if (vehicle)
        ignore.push_back(vehicle->getAIObject());

    const Vec3 offset = {targetPosition.x - ownPosition.x, targetPosition.y - ownPosition.y,
                         targetPosition.z - ownPosition.z};
    const float distance = std::sqrt(offset.z * offset.z + offset.y * offset.y +
                                     offset.x * offset.x);
    Vec3 direction = offset;
    const float directionLength = std::sqrt(direction.z * direction.z + direction.y * direction.y +
                                            direction.x * direction.x);
    if (directionLength > kNormaliseFloor) {
        const float inverse = 1.0f / directionLength;
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
    }

    const int weaponIndex = m_weapons[id];
    WeaponInfo* weapon = 0;
    WeaponInfo* singleShotWeapon = 0;
    WeaponInfo* previousWeapon = 0;
    if (weaponIndex != -1) {
        weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));
        singleShotWeapon =
            WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));
    }
    if (planWeapon != -1)
        previousWeapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(planWeapon));
    if (weaponIndex == -1) {
        record->m_int4C = -100;
        return bot->getPlan();
    }

    Vec3 aimPoint = {0.0f, 0.0f, 0.0f};
    {
        const float* const m = target->getTransformation().m;
        const Vec3& in = sensed->m_senseNormal;
        const float z = in.x * m[2] + in.z * m[10] + in.y * m[6] + m[14];
        const float y = in.x * m[1] + in.z * m[9] + in.y * m[5] + m[13];
        const float x = in.z * m[8] + in.y * m[4] + in.x * m[0] + m[12];
        aimPoint.x = x;
        aimPoint.y = y;
        aimPoint.z = z;
    }
    bot->getPlayer()->getCameraTransformation();   // the result is unused, as shipped

    const double sinceLast = fireTimer()->getGameTime() - aim->m_lastTime;
    if (plan->m_target != targetHandle || sinceLast > kAimForget) {
        aim->m_state = 0;
        aim->m_history.clear();
        aim->m_stateTime = 0.0;
        aim->m_int58 = -1;
        aim->m_stateDuration = 0.0;
        aim->m_int5C = -1;
    }
    float positionDistance = 0.0f;
    if (plan->m_bool17)
        positionDistance = horizontalDistance(ownPosition, plan->m_vec18);

    // Is the current action finished? `replan` asks for a new plan; a reset returns to state 0.
    bool replan = false;
    bool reset = false;
    const bool hasMobile = own->getMobile() != 0;
    const int sub = aim->m_subState;
    switch (aim->m_state) {
    case 0:
        replan = true;
        reset = true;
        break;
    case 1:
    case 2:
    case 4: {
        WeaponInfo* const current = aim->m_state == 1 ? weapon : singleShotWeapon;
        if (!current) {
            reset = true;
            break;
        }
        if (sub == 0)
            break;
        if (aim->m_state == 1 && sub == 2 && hasMobile) {
            if (!previousWeapon)
                replan = true;
            else if (!plan->m_bool14) {
                replan = true;
                reset = true;
            } else if (!(previousWeapon->getMinRange() + 1.0f >= distance))
                replan = true;
            break;
        }
        if (sub == 1 && hasMobile) {
            if (aim->m_state == 4) {
                if (positionDistance < plan->m_float34)
                    replan = true;
                break;
            }
            if (!previousWeapon) {
                replan = true;
                break;
            }
            if (!plan->m_bool14) {
                if (!plan->m_bool17)
                    break;
                const Vec3 moved = {targetPosition.x - aim->m_aimPosition.x,
                                    targetPosition.y - aim->m_aimPosition.y,
                                    targetPosition.z - aim->m_aimPosition.z};
                if (!(std::sqrt(moved.z * moved.z + moved.y * moved.y + moved.x * moved.x)
                      <= kAimMoved)) {
                    replan = true;
                    reset = true;
                    break;
                }
                if (positionDistance < plan->m_float34) {
                    replan = true;
                    break;
                }
            }
            // Within the weapon's range: a new plan.
            if (previousWeapon->getMinRange() >= distance)
                break;
            if (!(previousWeapon->getMaxRange() <= distance))
                replan = true;
            break;
        }
        if (sub != 3)
            break;
        if (!previousWeapon) {
            replan = true;
            reset = true;
            break;
        }
        // Out of range ends the action: the burst checks the maximum always and the minimum
        // without a mobile; the single shot checks both only without a mobile; repositioning
        // checks neither.
        bool outOfRange = false;
        if (aim->m_state == 1) {
            if (!hasMobile && previousWeapon->getMinRange() >= distance)
                outOfRange = true;
            else if (previousWeapon->getMaxRange() <= distance)
                outOfRange = true;
        } else if (aim->m_state == 2 && !hasMobile) {
            if (previousWeapon->getMinRange() >= distance)
                outOfRange = true;
            else if (previousWeapon->getMaxRange() <= distance)
                outOfRange = true;
        }
        if (outOfRange) {
            replan = true;
            reset = true;
            break;
        }
        // The shots of the action are spent, the target is dead or the weapon is empty.
        bool spent;
        if (plan->m_int08 == -1)
            spent = targetPhysical->getHealth() <= 0.0f;
        else
            spent = !(player->getNumShotsFired(previousWeapon->getWeapon()) - aim->m_tick
                      < plan->m_int08);
        if (!spent)
            spent = previousWeapon->getAmmo() == 0;
        if (spent) {
            replan = true;
            aim->m_history.push_front(aim->m_state);
            reset = true;
        }
        break;
    }
    case 5:
        if (sub == 0 && !hasMobile)
            break;
        if (sub == 1 && positionDistance < plan->m_float34) {
            replan = true;
            aim->m_history.push_front(aim->m_state);
            reset = true;
        }
        break;
    default:
        Debug(DebugTypeError, "AI/AIDLL/AI/AIBotBehaviourPattern/BBPFireInfantery.cpp", 1047, "")
            << "I don't belive we should be here!";
        break;
    }
    if (reset) {
        aim->m_state = 0;
        aim->m_subState = 0;
    }

    while (static_cast<float>(aim->m_history.size()) > kHistoryLength)
        aim->m_history.pop_back();
    int counts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (std::list<int>::const_iterator it = aim->m_history.begin(); it != aim->m_history.end(); ++it)
        ++counts[*it];

    // Choose the next action.
    int choice = 0;
    bool chosen = false;
    const int actions[7] = {1, 2, 3, 4, 7, 6, 5};
    const bool timing = fireTimer()->getGameTime() - aim->m_stateTime < aim->m_stateDuration;
    const bool firing = aim->m_state == 2 || aim->m_state == 3 || aim->m_state == 4;
    const bool aiming = aim->m_state == 1;
    const bool closing = (aim->m_state == 5 || aim->m_state == 7 || aim->m_state == 6)
        && own->getMobile() != 0;
    const bool idle = aim->m_state == 0;
    const bool firedRecently = counts[4] + counts[3] + counts[2] > 0
        && (aim->m_history.front() == 2 || aim->m_history.front() == 3
            || aim->m_history.front() == 4);
    const bool mayFire = aim->m_state < 2 && !firedRecently;
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    for (int i = 0; i < 7 && !chosen; ++i) {
        const int action = actions[i];
        if (action == 0)
            break;
        switch (action) {
        case 1: {
            if (!weapon || aiming)
                break;
            float probability = 1.0f;
            if (counts[1] > 0) {
                const float repeats = static_cast<float>(counts[1]) * kRepeatScale;
                if (repeats > kRepeatFloor)
                    probability = 1.0f / repeats;
            }
            if (closing)
                probability *= kMovingScale;
            if (firing && aim->m_subState == 3)
                probability = 0.0f;
            chosen = offerChoice(choice, action, probability);
            break;
        }
        case 2: {
            if (!singleShotWeapon || timing || (!closing && !idle) || (!mayFire && weapon))
                break;
            bool clear = remoteTarget;
            if (!clear)
                clear = environment->hasIndirectLineOfFire(
                    singleShotWeapon->getWeapon(), singleShotWeapon->getPosition(), aimPoint, ignore);
            chosen = offerChoice(choice, action, clear ? 0.5f : 0.05f);
            break;
        }
        case 4: {
            if (!singleShotWeapon || timing || (!closing && !idle) || (!mayFire && weapon))
                break;
            Vec3 before = {targetPosition.x - direction.x * kStandOff,
                           targetPosition.y - direction.y * kStandOff,
                           targetPosition.z - direction.z * kStandOff};
            if (!remoteTarget
                && !environment->hasIndirectLineOfFire(singleShotWeapon->getWeapon(),
                                                       singleShotWeapon->getPosition(), before,
                                                       ignore))
                break;
            chosen = offerChoice(choice, action, 0.5f);
            break;
        }
        default:
            break;
        }
    }
    if (chosen || aim->m_state == 0) {
        aim->m_state = choice;
        aim->m_subState = 0;
        replan = true;
    }

    aim->m_lastTime = fireTimer()->getGameTime();
    FireInfanteryTarget next = *plan;
    next.m_target = targetHandle;
    next.m_vec18 = targetPosition;

    // Fill the target state for the chosen action.
    bool startTimer = false;
    switch (aim->m_state) {
    case 0:
        next.m_bool17 = false;
        next.m_bool25 = false;
        next.m_weapon = -1;
        break;
    case 1: {
        // Aim and fire bursts: the burst size, the stand-off when too close.
        bool burst = false;
        bool standOff = false;
        switch (aim->m_subState) {
        case 0:
            if (mobile) {
                next.m_bool16 = aimWhileDriving;
                next.m_int08 = 0;
                next.m_bool26 = false;
                next.m_bool25 = true;
                next.m_bool15 = false;
                next.m_bool14 = true;
                next.m_bool17 = false;
                next.m_bool24 = true;
                aim->m_subState = 1;
                aim->m_aimPosition = targetPosition;
            }
            burst = true;
            break;
        case 1:
            burst = replan;
            standOff = true;
            break;
        case 2:
            standOff = true;
            break;
        case 3:
            standOff = true;
            break;
        default:
            break;
        }
        if (burst) {
            next.m_weapon = weaponIndex;
            if (weapon->getBurst()) {
                float count = static_cast<float>(weapon->getMaxAmmoInMag());
                if (count >= 1.0f)
                    count = count < 10.0f ? count : 10.0f;
                else
                    count = 10.0f;
                next.m_int08 = static_cast<int>(AIRandom::m_instance->nextRange(1.0f, count));
            } else {
                next.m_int08 = static_cast<int>(AIRandom::m_instance->nextRange(2.0f, 5.0f));
            }
            next.m_bool16 = aimWhileDriving;
            next.m_bool17 = false;
            next.m_bool26 = false;
            next.m_bool15 = false;
            next.m_bool25 = true;
            next.m_bool14 = mobile != 0;
            aim->m_subState = 3;
            aim->m_aimPosition = targetPosition;
            aim->m_tick = player->getNumShotsFired(weapon->getWeapon());
            aim->m_double60 = fireTimer()->getGameTime();
            replan = false;
            standOff = true;
        }
        if (!standOff)
            break;
        if (aim->m_subState != 2) {
            // Sub-states 1 and 3, and a fresh burst, pass the fire permission first.
            if (!bot->isFireAllowed())
                next.m_int10 = 1;
            if (replan)
                aim->m_subState = 0;
            else
                next.m_bool15 = false;
        }
        const float tooClose = weapon->getMinRange() + 1.0f;
        if (mobile
            && !(tooClose * tooClose
                 <= offset.z * offset.z + offset.y * offset.y + offset.x * offset.x)) {
            const float backOff = weapon->getMinRange() + weapon->getMinRange() + kStandOff;
            const Vec3 fromLast = {targetPosition.x - plan->m_vec28.x,
                                   targetPosition.y - plan->m_vec28.y,
                                   targetPosition.z - plan->m_vec28.z};
            if (aim->m_subState == 2
                && backOff * kOverlapShare < fromLast.z * fromLast.z + fromLast.y * fromLast.y
                                                 + fromLast.x * fromLast.x) {
                aim->m_aimOffset = plan->m_vec18;
                next.m_vec18 = plan->m_vec18;
            } else {
                next.m_vec18.x = 1.0f;
                next.m_vec18.y = 0.0f;
                next.m_vec18.z = 1.0f;
                aim->m_aimOffset.x = 1.0f;
                aim->m_aimOffset.y = 0.0f;
                aim->m_aimOffset.z = 1.0f;
                next.m_bool15 = true;
                aim->m_subState = 2;
            }
            replan = false;
        } else if (aim->m_subState == 2) {
            aim->m_subState = 0;
            next.m_bool15 = false;
            replan = true;
        }
        break;
    }
    case 2:
        // Single shots.
        if (aim->m_subState == 0) {
            if (mobile) {
                next.m_bool24 = true;
                aim->m_subState = 1;
                aim->m_aimPosition = targetPosition;
            }
        } else if (aim->m_subState == 1) {
            if (!replan)
                break;
        } else {
            if (aim->m_subState == 3 && replan)
                aim->m_subState = 0;
            break;
        }
        next.m_weapon = weaponIndex;
        next.m_bool16 = aimWhileDriving;
        next.m_bool14 = mobile != 0;
        next.m_int08 = 1;
        next.m_bool17 = false;
        next.m_bool26 = false;
        next.m_bool25 = false;
        aim->m_subState = 3;
        aim->m_aimPosition = targetPosition;
        startTimer = true;
        break;
    case 4: {
        // Reposition into range, then fire single shots at the position.
        bool atPosition = false;
        if (aim->m_subState == 0) {
            if (!mobile) {
                atPosition = true;
            } else {
                next.m_weapon = weaponIndex;
                next.m_int08 = 0;
                next.m_bool26 = false;
                next.m_bool14 = false;
                next.m_bool16 = false;
                next.m_bool25 = false;
                const bool found = chooseFirePosition(next.m_vec18, next.m_vec28, own, mobile,
                                                      equipmentType, singleShotWeapon, target,
                                                      targetPosition);
                next.m_bool17 = found;
                next.m_bool24 = false;
                aim->m_subState = 1;
                aim->m_aimPosition = targetPosition;
                replan = !found;
                atPosition = replan;
            }
        } else if (aim->m_subState == 1) {
            atPosition = replan;
        } else if (aim->m_subState == 3) {
            if (replan)
                aim->m_subState = 0;
        }
        if (!atPosition)
            break;
        next.m_weapon = weaponIndex;
        next.m_int08 = 1;
        next.m_bool26 = true;
        next.m_bool14 = false;
        next.m_bool16 = false;
        next.m_bool25 = false;
        aim->m_subState = 3;
        aim->m_aimPosition = targetPosition;
        startTimer = true;
        break;
    }
    case 5:
        // Close in on the target.
        if (aim->m_subState == 0) {
            replan = true;
            if (!mobile) {
                aim->m_subState = 0;
                break;
            }
            target->getPosition();   // copied to an unused local, as shipped
            next.m_weapon = weaponIndex;
            next.m_bool16 = aimWhileDriving;
            next.m_bool14 = true;
            next.m_bool25 = true;
            next.m_int08 = 0;
            next.m_bool26 = false;
            next.m_bool17 = false;
            next.m_bool24 = false;
            aim->m_subState = 1;
            aim->m_aimPosition = targetPosition;
            replan = false;
        } else if (aim->m_subState == 1 && replan) {
            aim->m_subState = 0;
        }
        break;
    default:
        break;
    }
    if (startTimer) {
        aim->m_tick = player->getNumShotsFired(singleShotWeapon->getWeapon());
        aim->m_stateTime = fireTimer()->getGameTime();
        aim->m_stateDuration = AIRandom::m_instance->nextRange(5.0f, 10.0f);
        aim->m_double60 = fireTimer()->getGameTime();
        replan = false;
    }

    if (!bot->isFireAllowed())
        next.m_int10 = 1;
    if (replan) {
        dropPlan(bot);
        return 0;
    }
    bot->getSquadMember()->clearRequest();
    next.m_float34 = physical->getRadius() + kRadiusMargin;
    *plan = next;
    return buildScript(bot, control);
}

namespace {

// Component-wise equality within FLT_EPSILON. PE 100D2910.
bool isSamePosition(const Vec3& a, const Vec3& b)
{
    return std::fabs(a.x - b.x) <= 1.1920929e-07f && std::fabs(a.y - b.y) <= 1.1920929e-07f
        && std::fabs(a.z - b.z) <= 1.1920929e-07f;
}

// The shared zero vector. PE 101E5390.
const Vec3& zeroVector()
{
    static const Vec3 zero = {0.0f, 0.0f, 0.0f};
    return zero;
}

// PE 1006EEC0, the conditional Vec3 normaliser (file-local copies live in other TUs too).
void normaliseVector(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f)) {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
}

float lengthSq(const Vec3& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

// One degree in radians. PE 102A9CC0.
const float kDegree = 0.0174532924f;

// A one-input mask built as shipped: the shift is a 32-bit int (so only the low five bits of the
// input count) and the result is sign-extended to 64 bits.
unsigned long long inputMask32(int input)
{
    return static_cast<unsigned long long>(static_cast<long long>(1 << (input & 31)));
}

} // anonymous namespace

// PE 101069D0.
BotScript* BBPFireInfantery::buildGuidedFire(Bot* bot, FireInfanteryTarget* plan,
                                             BotScriptValue<WeaponInfo*>* weaponValue,
                                             BotScriptValue<unsigned int>* /*targetValue*/,
                                             BotScriptValue<Vec3>* lookPosition, WeaponInfo* weapon,
                                             Information* /*own*/, IPIControlInfo* control, Information* target,
                                             IPIPhysical* /*targetPhysical*/, int guidance)
{
    BAPContext* const context = bot->getScriptContext();
    const unsigned int targetHandle = plan->m_target;
    BAPParallel* const sequence = checkPointer(new BAPParallel(m_behaviourId, false, 14, false));

    // Without the target's information the look falls back to the stored fire position, with a
    // dead zone of half a degree.
    BAPALook* look;
    if (target)
        look = checkPointer(new BAPALookAtObject(bot, targetHandle, 0.0f, checkPointer(new BAPConFalse),
                                                 control->getLookControls(), m_behaviourId, lookPosition, 0.0f));
    else
        look = checkPointer(new BAPALookAtPosition(m_positions[bot->getBotId()], bot, kDegree * 0.5f,
                                                   checkPointer(new BAPConFalse), control->getLookControls(),
                                                   m_behaviourId));
    sequence->addStatement(look);

    // Release the trigger once the magazine is empty, a lock-on weapon has lost its lock, or the
    // look strays beyond five degrees (guidance 2) or three degrees (otherwise).
    BAPConAnd* const release = checkPointer(new BAPConAnd);
    const int noShots = 0;
    release->addCondition(checkPointer(
        new BAPConMagazine(weaponValue, context->createVariable<int>(noShots, 0), false)));
    if (guidance == 2 || guidance == 1)
        release->addCondition(checkPointer(
            new BAPConNot(checkPointer(new BAPConWeaponLocked(weaponValue, targetHandle)))));
    const float window = guidance == 2 ? 5.0f * kDegree : kDegree * 3.0f;
    release->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConLookWithin(look, window, false)))));

    BAPTrigger* const fire = checkPointer(new BAPTrigger(weapon->getWeaponFire(), checkPointer(new BAPConTrue),
                                                         release, inputMask32(weapon->getWeaponFire()),
                                                         m_behaviourId, 0.0, true));
    if (weapon->getWeaponZoom() != 0x40) {
        BAPConFalse* const zoomRelease = new BAPConFalse;
        BAPConTrue* const zoomUntil = new BAPConTrue;
        BAPTrigger* const zoom = new BAPTrigger(weapon->getWeaponZoom(), checkPointer(zoomUntil),
                                                checkPointer(zoomRelease), inputMask32(weapon->getWeaponZoom()),
                                                m_behaviourId, 0.0, false);
        BAPConNot* const notZoomed = new BAPConNot(checkPointer(new BAPConWeaponZoomed));
        sequence->addStatement(checkPointer(new BAPIf(checkPointer(notZoomed), checkPointer(zoom), 0,
                                                      m_behaviourId, true, false)));
    }
    sequence->addStatement(fire);
    return sequence;
}

// PE 10106E90. The opening repeats the fire plan's (weapon, variables, reset), but the weapon index
// is not checked against -1 and the result of the movement decision differs: the bot closes in
// while it cannot fire and otherwise keeps the target between half the range and the minimum.
// There is no squad share of the target at the close, as shipped.
BotScript* BBPFireInfantery::buildGuidedSoldierScript(Bot* bot, IPIControlInfo* controlArgument,
                                                      const Vec3& firePosition)
{
    BotMain* const main = static_cast<BotMain*>(bot);
    FireInfanteryTarget* const plan = &m_targetState[bot->getBotId()];
    if (bot->getDebugger()) {
        // Retail calls the debugger's third slot; the debugger type is not recovered.
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-debugger]
        BF2_AI_FRONTIER("dice::hfe::ai::BBPFireInfantery::buildGuidedSoldierScript (script debugger reset)");
    }
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    main->setTime276(fireTimer()->getGameTime());
    dropPlan(bot);

    // The equipment lookup is not checked, as shipped.
    Information* const own = informationManager->getInformation(bot->getCurrentEquipment());
    IPIArmament* const armament = own->getArmament();
    IPIControlInfo* const control = own->getControlInfo();
    if (!armament || !own->getPhysical() || !control)
        return 0;
    IPIMobile* const mobile = own->getMobile();
    if (mobile)
        mobile->isTurnable();         // the results are unused, as shipped
    control->getAimControls();
    control->getDriveControls();
    const Vec3& ownPosition = own->getPosition();
    const unsigned int targetHandle = plan->m_target;
    Information* const target = informationManager->getInformation(targetHandle);
    IPIPhysical* const targetPhysical = target ? target->getPhysical() : 0;
    if (!target || !targetPhysical)
        return 0;

    const int weaponIndex = m_weapons[bot->getBotId()];
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));
    armament->setActiveWeapon(weaponIndex, bot->getSide());

    BotScriptValue<WeaponInfo*>* const weaponValue = context->createVariable<WeaponInfo*>(weapon, 0);
    BotScriptValue<unsigned int>* const targetValue = context->createVariable<unsigned int>(targetHandle, 0);
    const bool falseValue = false;
    BotScriptValue<bool>* const aimValid = context->createVariable<bool>(falseValue, 0);
    Vec3 initial = {0.0f, 1.0f, 0.0f};
    BotScriptValue<Vec3>* const aimDirection = context->createVariable<Vec3>(initial, 0);
    initial.x = initial.y = initial.z = 0.0f;
    BotScriptValue<Vec3>* const lookPosition = context->createVariable<Vec3>(initial, 0);

    const unsigned int id = m_behaviourId;
    BAPSerial* const root = checkPointer(new BAPSerial(id, 0));
    if (weapon->getWeaponActivate() != 0x40)
        root->addStatement(checkPointer(new BAPChangeWeapon(bot, weaponIndex, id)));
    BAPParallel* const sequence = checkPointer(new BAPParallel(id, true, -1, false));
    root->addStatement(sequence);

    const bool ownSoldier = (own->getObjectTypeFlags() >> 21) & 1;
    if (ownSoldier) {
        BAPOverrideSensingAgent* const sensing = new BAPOverrideSensingAgent;
        BAPConNot* const notSprinting = new BAPConNot(checkPointer(new BAPConSprinting));
        sequence->addStatement(checkPointer(new BAPIf(checkPointer(notSprinting), checkPointer(sensing), 0,
                                                      id, true, false)));
    } else {
        sequence->addStatement(checkPointer(new BAPOverrideSensingAgent));
    }
    sequence->addStatement(checkPointer(new BAPResetControls(true, true, true, id)));
    if (!ownSoldier) {
        // The first argument is the bot pointer in retail, stored and never read; on a 64-bit
        // port only its low half fits. [PLATFORM]
        sequence->addStatement(new BAPUseFlareOnMissileLock(
            static_cast<int>(reinterpret_cast<std::size_t>(bot)), 0x800000000000ULL, id));
    }
    const int guidance = weapon->getWeaponGuidanceType();

    // Near a control point the bot may also fire from its approach. The equipment's mobile part
    // is not null-checked here, as shipped.
    BAPConShared* areaShared = 0;
    AIStrategicArea* const area = static_cast<AIStrategicArea*>(
        AIStrategicObjectManager::getInstance()->getClosestObject(ownPosition, bot->getSide(), false));
    Vec3 approach = {0.0f, 0.0f, 0.0f};
    if (area && area->m_controlPointId != static_cast<unsigned int>(-1)) {
        area->getControlPointApproach(area, bot->getSide(), mobile->getVehicleNumber(),
                                      own->getUnit()->getEquipmentType(), ownPosition, approach);
        const float radius = area->m_size[bot->getSide()];
        const Vec2& centre = area->getPosition(bot->getSide());
        const Vec3 offset = {centre.x - approach.x, 0.0f, centre.y - approach.z};
        if (radius * radius > lengthSq(offset)) {
            BAPConAnd* const atArea = checkPointer(new BAPConAnd);
            atArea->addCondition(checkPointer(new BAPConStrategicStatus(area, bot->getSide(), false)));
            atArea->addCondition(checkPointer(new BAPConPositionDistance(approach, 40.0f, true)));
            areaShared = checkPointer(new BAPConShared(atArea));
        }
    }

    BAPConShared* const canFire = checkPointer(new BAPConShared(
        checkPointer(new BAPConCanFireAt(targetHandle, controlArgument != 0))));
    BAPConAnd* const ready = checkPointer(new BAPConAnd);
    ready->addCondition(canFire->share());
    if (areaShared) {
        BAPConOr* const either = checkPointer(new BAPConOr);
        either->addCondition(checkPointer(new BAPConObjectDistance(
            targetHandle, weapon->getMinRange() * 0.9f, weapon->getMaxRange() * 1.1f, true, false)));
        either->addCondition(areaShared->share());
        ready->addCondition(either);
    } else {
        ready->addCondition(checkPointer(new BAPConObjectDistance(
            targetHandle, weapon->getMinRange() * 0.9f, weapon->getMaxRange() * 1.1f, true, false)));
    }

    BotScript* fire = 0;
    if (m_weapons[bot->getBotId()] != -1 && plan->m_int08)
        fire = buildGuidedFire(bot, plan, weaponValue, targetValue, lookPosition, weapon, own, control, target,
                               targetPhysical, guidance);
    BotScript* const look = checkPointer(new BAPALookAtObject(bot, targetHandle, 0.0f, checkPointer(new BAPConFalse),
                                                              control->getLookControls(), id, lookPosition, 0.0f));
    BAPIf* const fireIf = checkPointer(new BAPIf(ready, fire, look, id, true, false));

    BAPParallel* const body = checkPointer(new BAPParallel(id, 4, false));
    body->addStatement(checkPointer(new BAPEvaluateCondition(canFire, id)));
    body->addStatement(checkPointer(new BAPLookAtWrapper(targetHandle, lookPosition, true, guidance == 3,
                                                         aimDirection, aimValid)));

    BotScript* move = 0;
    if (mobile) {
        BAPConOr* const moveWhen = checkPointer(new BAPConOr);
        moveWhen->addCondition(checkPointer(new BAPConNot(canFire->share())));
        moveWhen->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConFireAllowed))));

        bool targetMobile = target->getMobile() != 0;
        if (!targetMobile && !target->isPrimary())
            targetMobile = informationManager->getInformation(target->getPrimaryObject(bot->getSide()))
                               ->getMobile() != 0;

        // Unable to fire: close in, then forget the target and end the plan.
        BotScript* closeIn;
        if (targetMobile) {
            const float nearest = weapon->getMinRange() * 1.1f;
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.5f;
            closeIn = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, 0.0f, mobile->getMaxSpeed(), slow,
                                                              0, drive, id, 0, nearest, 1));
        } else {
            const float nearest = weapon->getMinRange() * 1.1f;
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            closeIn = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0, drive,
                                                        id, 0.5f, nearest));
        }
        BAPSerial* const closeInPlan = checkPointer(new BAPSerial(id, 300));
        closeInPlan->addStatement(closeIn);
        closeInPlan->addStatement(checkPointer(new BAPForgetTarget(plan->m_target)));
        closeInPlan->addStatement(checkPointer(new BAPPlanEnd(id, 0, 0)));

        // Able to fire: farther than half the range or nearer than the minimum, move to 0.85 of it.
        BAPConObjectDistance* const tooFar = checkPointer(
            new BAPConObjectDistance(targetHandle, weapon->getMaxRange() * 0.5f, true, false, true));
        BAPConOr* const outOfBand = checkPointer(new BAPConOr);
        outOfBand->addCondition(tooFar);
        outOfBand->addCondition(checkPointer(
            new BAPConObjectDistance(targetHandle, weapon->getMinRange(), true, false, false)));
        BotScript* keep;
        if (targetMobile) {
            const float nearest = std::max(15.0f, weapon->getMinRange() * 1.1f);   // PE 10086630
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.5f;
            const float fast = mobile->getMaxSpeed();
            keep = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, weapon->getMaxRange() * 0.85f, fast,
                                                           slow, 0, drive, id, checkPointer(new BAPConFalse),
                                                           nearest, 2));
        } else {
            const float nearest = std::max(15.0f, weapon->getMinRange() * 1.1f);
            const float band = weapon->getMaxRange() * 0.85f;
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            keep = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0, drive, id,
                                                     band, nearest));
        }
        BAPIf* const keepIf = checkPointer(new BAPIf(outOfBand, keep, 0, id, false, false));
        move = checkPointer(new BAPIf(moveWhen, closeInPlan, keepIf, id, true, false));
    }

    // Reload while the magazine is empty and no projectile of the bot's own is within 1000.
    const int noShots = 0;
    BAPConMagazine* const empty = checkPointer(
        new BAPConMagazine(weaponValue, context->createVariable<int>(noShots, 0), false));
    BAPTrigger* const reload = checkPointer(new BAPTrigger(0x20, checkPointer(new BAPConTrue),
                                                           checkPointer(new BAPConFalse), 1ULL << 32, id, 0.0,
                                                           false));
    BAPConProjectileNear* const projectileNear = checkPointer(new BAPConProjectileNear(armament, 1000.0f));
    BAPConAnd* const reloadWhen = checkPointer(new BAPConAnd);
    reloadWhen->addCondition(empty);
    reloadWhen->addCondition(checkPointer(new BAPConNot(projectileNear)));
    body->addStatement(checkPointer(new BAPIf(reloadWhen, reload, 0, id, true, false)));

    if (areaShared) {
        const unsigned long long drive = control->getDriveControls();
        const float slow = mobile->getMaxSpeed() * 0.75f;
        BAPMoveToFinding* const toArea = checkPointer(new BAPMoveToFinding(
            bot, approach, mobile->getMaxSpeed(), slow, 0, drive, id, 0.0f, 0.0f));
        body->addStatement(checkPointer(new BAPIf(areaShared, toArea, move, id, false, false)));
    } else {
        body->addStatement(move);
    }
    body->addStatement(fireIf);
    if (ownSoldier)
        body->addStatement(checkPointer(new BAPMergeSoldierInputs(id)));

    BAPConAnd* const alive = checkPointer(new BAPConAnd);
    alive->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConObjectHealthBelow(targetHandle, 0.0f)))));
    sequence->addStatement(checkPointer(new BAPWhile(alive, body, false, id)));

    const int wrapperWeapon = weapon ? m_weapons[bot->getBotId()] : -1;
    BAPFireWrapper* const wrapper = checkPointer(new BAPFireWrapper(
        bot, targetHandle, m_flagA, plan->m_bool15, wrapperWeapon, -1, root, id));
    wrapper->m_owner = this;
    return wrapper;
}

// PE 10106230.
BotScript* BBPFireInfantery::buildDirectFire(Bot* bot, FireInfanteryTarget* plan,
                                             BotScriptValue<WeaponInfo*>* weaponValue,
                                             BotScriptValue<unsigned int>*, BotScriptValue<bool>* aimValid,
                                             BotScriptValue<Vec3>* aimDirection,
                                             BotScriptValue<Vec3>* aimPosition, BotScriptValue<Vec3>*,
                                             WeaponInfo* weapon, Information* own, IPIControlInfo* control,
                                             Information* target, IPIPhysical*, int guidance)
{
    BotMain* const main = static_cast<BotMain*>(bot);
    BAPContext* const context = bot->getScriptContext();
    const unsigned int targetHandle = plan->m_target;
    const bool sniper = weapon->getWeaponZoom() != 0x40 && main->getSpawnKit() == 1
                        && weapon->getMaxRange() > 100.0f;
    const bool splash = weapon->getExplosionRadius() != 0.0f && guidance == 0;
    const SensedObject* const sensed =
        static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(targetHandle));
    const bool ownSoldier = (own->getObjectTypeFlags() >> 21) & 1;
    const bool targetSoldier = (target->getObjectTypeFlags() >> 21) & 1;
    const bool splashOnSoldier = !ownSoldier && targetSoldier && guidance == 0
                                 && weapon->getExplosionRadius() != 0.0f;
    const bool high = main->getSpawnKit() != 1 && !weapon->getIsThrown()
                      && target->getPosition().y - own->getPosition().y > 30.0f;
    // The sensed record is not null-checked, as shipped.
    const bool reported = main->getUseStaticAttack() && splash && targetSoldier
                          && ((sensed->m_senseFlags & 8) || high || splashOnSoldier);

    BAPParallel* const root = checkPointer(new BAPParallel(m_behaviourId, false, 14, false));
    Vec3 zero;
    BotScript* aim;
    if (plan->m_bool26) {
        BotScriptValue<Vec3>* const position = context->createVariable<Vec3>(plan->m_vec28, 0);
        zero.x = zero.y = zero.z = 0.0f;
        aim = checkPointer(new BAPAimAtPosition(bot, position, zero, checkPointer(new BAPConFalse),
                                                control->getAimControls(), m_behaviourId, aimValid,
                                                aimDirection));
    } else {
        zero.x = zero.y = zero.z = 0.0f;
        aim = checkPointer(new BAPAimAtObject(bot, targetHandle, zero, checkPointer(new BAPConFalse),
                                              control->getAimControls(), m_behaviourId, false, aimValid,
                                              aimDirection));
    }
    root->addStatement(aim);

    float deviation = weapon->getAllowedDeviation();
    if (splash && targetSoldier && !ownSoldier)
        deviation = 1.0f;

    BAPCondition* solution;
    if (aimValid)
        solution = checkPointer(new BAPConFireSolution(targetHandle, aimValid, aimDirection, aimPosition,
                                                       deviation, !weapon->getBurst(), sniper,
                                                       weapon->getMaxRange() < 5.0f));
    else
        solution = checkPointer(new BAPConFireSolution(targetHandle, static_cast<BAPAimAt*>(aim), deviation,
                                                       !weapon->getBurst(), sniper,
                                                       weapon->getMaxRange() < 5.0f));

    BAPConAnd* const ready = checkPointer(new BAPConAnd);
    if (weapon->usesHeat())
        ready->addCondition(checkPointer(new BAPConShared(
            checkPointer(new BAPConWeaponHeat(weaponValue, 0.8f, 0.5f)))));
    if (reported) {
        if (splashOnSoldier) {
            ready->addCondition(solution);
            ready->addCondition(checkPointer(
                new BAPConMuzzleImpactNear(targetHandle, aimPosition, aimDirection, 3.5f)));
        } else {
            ready->addCondition(checkPointer(new BAPConLobPathClear(targetHandle, aimPosition, aimDirection)));
            ready->addCondition(solution);
        }
    } else {
        ready->addCondition(solution);
    }

    float hold = 0.0f;
    if (m_holdFire && weapon->getMaxRange() > 10.0f) {
        const float spread = static_cast<float>(
            static_cast<double>(AIRandom::m_instance->next()) * 2.3283064365386963e-10 + 1.5);
        hold = (1.0f - aiSettings()->getBotSkill()) * spread;
    }

    BotScript* fire;
    if (weapon->getBurst())
        fire = checkPointer(new BAPTriggerContinously(weapon->getWeaponFire(), ready, 0, 0,
                                                      inputMask32(weapon->getWeaponFire()), m_behaviourId,
                                                      static_cast<double>(hold)));
    else
        fire = checkPointer(new BAPTrigger(weapon->getWeaponFire(), ready, 0,
                                           inputMask32(weapon->getWeaponFire()), m_behaviourId,
                                           static_cast<double>(hold), true));

    if (weapon->getWeaponZoom() != 0x40) {
        BAPConFalse* const release = new BAPConFalse;
        BAPConTrue* const until = new BAPConTrue;
        BAPTrigger* const zoom = new BAPTrigger(weapon->getWeaponZoom(), checkPointer(until),
                                                checkPointer(release),
                                                inputMask32(weapon->getWeaponZoom()), m_behaviourId, 0.0,
                                                false);
        BAPConNot* const notZoomed = new BAPConNot(checkPointer(new BAPConWeaponZoomed));
        root->addStatement(checkPointer(new BAPIf(checkPointer(notZoomed), checkPointer(zoom), 0,
                                                  m_behaviourId, true, false)));
    }
    root->addStatement(fire);
    return root;
}

// PE 10108090. Only the plan for a soldier firing a non-heavy weapon at a known target from a
// mobile equipment (bool14 set, bool16 clear) is written; the other branches stop at named
// frontiers. Shared opening: weapon choice, variables, reset, the decision to hold position;
// shared closing: the fire wrapper and the squad's share of the target.
BotScript* BBPFireInfantery::buildScript(Bot* bot, IPIControlInfo* controlArgument)
{
    BotMain* const main = static_cast<BotMain*>(bot);
    FireInfanteryTarget* const plan = &m_targetState[bot->getBotId()];
    if (bot->getDebugger()) {
        // Retail calls the debugger's third slot; the debugger type is not recovered.
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bap-debugger]
        BF2_AI_FRONTIER("dice::hfe::ai::BBPFireInfantery::buildScript (script debugger reset)");
    }
    BAPContext* const context = bot->getScriptContext();
    context->clear();
    main->setTime276(fireTimer()->getGameTime());
    dropPlan(bot);

    const unsigned int equipmentHandle = bot->getCurrentEquipment();
    // The equipment lookup is not checked, as shipped.
    Information* const own = informationManager->getInformation(equipmentHandle);
    IPIArmament* const armament = own->getArmament();
    IPIPhysical* const physical = own->getPhysical();
    IPIControlInfo* const control = own->getControlInfo();
    if (!armament || !physical || !control)
        return 0;
    IPIMobile* const mobile = own->getMobile();
    if (mobile)
        mobile->isTurnable();         // the results are unused, as shipped
    control->getAimControls();
    control->getDriveControls();
    const Vec3& ownPosition = own->getPosition();
    const unsigned int targetHandle = plan->m_target;
    Information* const target = informationManager->getInformation(targetHandle);
    IPIPhysical* const targetPhysical = target ? target->getPhysical() : 0;
    if (!target || !targetPhysical)
        return 0;

    WeaponInfo* weapon = 0;
    if (m_weapons[bot->getBotId()] != -1) {
        weapon = WeaponInfoManager::m_instance->getInformation(
            armament->getWeapon(m_weapons[bot->getBotId()]));
        armament->setActiveWeapon(m_weapons[bot->getBotId()], bot->getSide());
    }

    bool targetMobile = target->getMobile() != 0;
    if (!targetMobile && !target->isPrimary())
        targetMobile = informationManager->getInformation(target->getPrimaryObject(bot->getSide()))
                           ->getMobile() != 0;

    // A mobile equipment against a fixed target looks for a spot thirty metres short of it, on
    // the equipment's side.
    Vec3 firePosition = {0.0f, 0.0f, 0.0f};
    bool firePositionValid = true;
    if (mobile && !targetMobile) {
        const Vec3& targetPosition = target->getPosition();
        Vec3 shortOf = {ownPosition.x - targetPosition.x, ownPosition.y - targetPosition.y,
                       ownPosition.z - targetPosition.z};
        normaliseVector(shortOf);
        shortOf.x = shortOf.x * 30.0f + targetPosition.x;
        shortOf.y = shortOf.y * 30.0f + targetPosition.y;
        shortOf.z = shortOf.z * 30.0f + targetPosition.z;
        unsigned int quadTree;
        unsigned int polygon;
        firePositionValid = IAIPathfinding::instance->findClosestValidPolygon(
            quadTree, polygon, firePosition, mobile->getVehicleNumber(), targetPosition, shortOf, 10,
            own->getUnit()->getEquipmentType());
    }

    // The weapon is not null-checked here, as shipped.
    const int guidance = weapon->getWeaponGuidanceType();
    if (guidance && ((own->getObjectTypeFlags() >> 21) & 1)) {
        return buildGuidedSoldierScript(bot, controlArgument, firePosition);
    }

    BotScriptValue<WeaponInfo*>* const weaponValue = context->createVariable<WeaponInfo*>(weapon, 0);
    BotScriptValue<unsigned int>* const targetValue = context->createVariable<unsigned int>(targetHandle, 0);
    const bool falseValue = false;
    BotScriptValue<bool>* const aimValid = context->createVariable<bool>(falseValue, 0);
    Vec3 initial = {0.0f, 1.0f, 0.0f};
    BotScriptValue<Vec3>* const aimDirection = context->createVariable<Vec3>(initial, 0);
    initial.x = initial.y = initial.z = 0.0f;
    BotScriptValue<Vec3>* const aimPosition = context->createVariable<Vec3>(initial, 0);
    initial.x = initial.y = initial.z = 0.0f;
    BotScriptValue<Vec3>* const lookPosition = context->createVariable<Vec3>(initial, 0);

    BAPSerial* const root = checkPointer(new BAPSerial(m_behaviourId, 0));
    if (m_weapons[bot->getBotId()] != -1 && weapon->getWeaponActivate() != 0x40)
        root->addStatement(checkPointer(new BAPChangeWeapon(bot, m_weapons[bot->getBotId()], m_behaviourId)));
    BAPParallel* const sequence = checkPointer(new BAPParallel(m_behaviourId, true, -1, false));
    root->addStatement(sequence);
    sequence->addStatement(checkPointer(new BAPResetControls(true, true, true, m_behaviourId)));

    const bool heavy = weapon->getWeaponType() == 4;
    const int antiAircraft = armament->isAntiAircraft();
    const bool targetVehicle = (target->getObjectTypeFlags() >> 4) & 1;
    bool noMove = (targetVehicle && antiAircraft == 2) || !firePositionValid;
    // Only the aim-while-moving branch reads this.
    const bool antiAircraftAtVehicle = targetVehicle && antiAircraft == 1;
    if (weapon->getMaxRange() < 10.0f)
        noMove = false;

    SquadMember* const member = bot->getSquadMember();
    Vec3 leaderPosition = {0.0f, 0.0f, 0.0f};
    bool hasLeaderPosition = false;
    if (member && member->getSquad() && member->getSquad()->isMissionStatusTwo()
        && member->getSquad()->getMemberOrderPos(member, leaderPosition))
        hasLeaderPosition = true;
    const bool ownSoldier = (own->getObjectTypeFlags() >> 21) & 1;
    const bool targetSoldier = (target->getObjectTypeFlags() >> 21) & 1;
    const bool targetHuman = IBotManager::instance->getBotFromPlayerId(target->getPlayerId()) == 0;

    // A fresh target behind the bot, away from its squad position, keeps the bot in place.
    if (!noMove && m_targetChanged[bot->getBotId()] && mobile && hasLeaderPosition
        && !isSamePosition(leaderPosition, zeroVector())) {
        const Vec3 toLeader = {leaderPosition.x - ownPosition.x, leaderPosition.y - ownPosition.y,
                               leaderPosition.z - ownPosition.z};
        const Vec3& targetPosition = target->getPosition();
        const Vec3 toTarget = {targetPosition.x - ownPosition.x, targetPosition.y - ownPosition.y,
                               targetPosition.z - ownPosition.z};
        const float reach = (ownSoldier || targetSoldier) ? 30.0f : 50.0f;
        if (reach * reach < lengthSq(toTarget)
            && toTarget.z * toLeader.z + toTarget.y * toLeader.y + toTarget.x * toLeader.x < 0.0f)
            noMove = true;
    }

    if (!mobile) {
        // ---- A fixed equipment (PE 1010BC79) ----
        // Without a weapon or a fire mode there is no plan. Otherwise: the sensing override, a
        // reload while the magazine is empty (for a weapon without automatic reload and other
        // than a single magazine), the aim or look wrapper, and the fire sequence while aimed.
        if (m_weapons[bot->getBotId()] == -1 || !plan->m_int08)
            return 0;
        if (ownSoldier) {
            BAPOverrideSensingAgent* const sensing = new BAPOverrideSensingAgent;
            BAPConNot* const notSprinting = new BAPConNot(checkPointer(new BAPConSprinting));
            sequence->addStatement(checkPointer(new BAPIf(checkPointer(notSprinting), checkPointer(sensing), 0,
                                                          m_behaviourId, true, false)));
        } else {
            sequence->addStatement(checkPointer(new BAPOverrideSensingAgent));
        }
        BAPParallel* const body = checkPointer(new BAPParallel(m_behaviourId, 7, false));
        if (!weapon->hasAutoReload() && weapon->getNumberOfMags() != 1) {
            const int noShots = 0;
            BAPConMagazine* const empty = checkPointer(
                new BAPConMagazine(weaponValue, context->createVariable<int>(noShots, 0), false));
            BAPTrigger* const reload = checkPointer(new BAPTrigger(0x20, checkPointer(new BAPConTrue),
                                                                   checkPointer(new BAPConFalse), 1ULL << 32,
                                                                   m_behaviourId, 0.0, false));
            body->addStatement(checkPointer(new BAPIf(empty, reload, 0, m_behaviourId, true, false)));
        }
        BotScript* fire;
        if (!guidance) {
            body->addStatement(checkPointer(new BAPAimingWrapper(aimDirection, aimValid, aimPosition,
                                                                 equipmentHandle, targetHandle, true)));
            fire = buildDirectFire(bot, plan, weaponValue, targetValue, aimValid, aimDirection, aimPosition,
                                   lookPosition, weapon, own, control, target, targetPhysical, 0);
        } else {
            body->addStatement(checkPointer(new BAPLookAtWrapper(targetHandle, lookPosition, true, guidance == 3,
                                                                 aimDirection, aimValid)));
            fire = buildGuidedFire(bot, plan, weaponValue, targetValue, lookPosition, weapon, own, control,
                                   target, targetPhysical, guidance);
        }
        BAPConAimOnTarget* const aimed = checkPointer(
            new BAPConAimOnTarget(targetHandle, equipmentHandle, aimValid, aimDirection, false));
        body->addStatement(checkPointer(new BAPIf(aimed, fire, 0, m_behaviourId, true, false)));
        sequence->addStatement(body);
        return finishScript(bot, plan, weapon, targetHandle, root);
    }
    if (!plan->m_bool14) {
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bbp-fire-infantery-builder]
        // Recover the plan without a known target (PE 1010B93F).
        BF2_AI_FRONTIER("dice::hfe::ai::BBPFireInfantery::buildScript (no known target)");
    }
    if (plan->m_bool16) {
        // ---- Aim while moving (PE 101087BA..10109B52) ----
        // A target whose vehicle has no valid map position, or a bot's (not a human's) soldier
        // target, keeps the bot in place.
        if (!noMove) {
            IPIMobile* const targetMobileInfo = target->getMobile();
            if (targetMobileInfo) {
                const IPIMobile::ValidPos& valid =
                    targetMobileInfo->getLastValidPosition(mobile->getVehicleNumber());
                if (valid.quadTree == static_cast<unsigned int>(-1)
                    || valid.polygon == static_cast<unsigned int>(-1))
                    noMove = true;
                else if (!targetHuman && targetSoldier)
                    noMove = true;
            } else if (!targetHuman && targetSoldier) {
                noMove = true;
            }
        }
        const bool splashOnly = weapon->getExplosionRadius() != 0.0f && guidance == 0;
        const bool staticAttack = main->getUseStaticAttack() && splashOnly && targetSoldier;

        if (ownSoldier) {
            BAPOverrideSensingAgent* const sensing = new BAPOverrideSensingAgent;
            BAPConNot* const notSprinting = new BAPConNot(checkPointer(new BAPConSprinting));
            sequence->addStatement(checkPointer(new BAPIf(checkPointer(notSprinting), checkPointer(sensing), 0,
                                                          m_behaviourId, true, false)));
        } else {
            sequence->addStatement(checkPointer(new BAPOverrideSensingAgent));
        }

        BAPConShared* canFireShared;
        if (!staticAttack)
            canFireShared = checkPointer(new BAPConShared(
                checkPointer(new BAPConCanFireAt(targetHandle, controlArgument != 0))));
        else
            canFireShared = checkPointer(new BAPConShared(checkPointer(new BAPConTrue)));

        BAPConShared* areaShared = 0;
        AIStrategicArea* const area = static_cast<AIStrategicArea*>(
            AIStrategicObjectManager::getInstance()->getClosestObject(ownPosition, bot->getSide(), false));
        Vec3 approach = {0.0f, 0.0f, 0.0f};
        if (area && !antiAircraftAtVehicle && !noMove
            && area->m_controlPointId != static_cast<unsigned int>(-1)) {
            area->getControlPointApproach(area, bot->getSide(), mobile->getVehicleNumber(),
                                          own->getUnit()->getEquipmentType(), ownPosition, approach);
            const float radius = area->m_size[bot->getSide()];
            const Vec2& centre = area->getPosition(bot->getSide());
            const Vec3 offset = {centre.x - approach.x, 0.0f, centre.y - approach.z};
            if (radius * radius > lengthSq(offset)) {
                BAPConAnd* const atArea = checkPointer(new BAPConAnd);
                atArea->addCondition(checkPointer(new BAPConStrategicStatus(area, bot->getSide(), false)));
                atArea->addCondition(checkPointer(new BAPConPositionDistance(approach, 40.0f, true)));
                areaShared = checkPointer(new BAPConShared(atArea));
            }
        }
        BAPConAnd* const ready = checkPointer(new BAPConAnd);
        ready->addCondition(canFireShared->share());
        if (areaShared) {
            BAPConOr* const either = checkPointer(new BAPConOr);
            either->addCondition(checkPointer(new BAPConObjectDistance(
                targetHandle, weapon->getMinRange(), weapon->getMaxRange(), true, false)));
            either->addCondition(areaShared->share());
            ready->addCondition(either);
        } else {
            ready->addCondition(checkPointer(new BAPConObjectDistance(
                targetHandle, weapon->getMinRange(), weapon->getMaxRange(), true, false)));
        }
        BAPConShared* const aimShared = checkPointer(new BAPConShared(checkPointer(
            new BAPConAimOnTarget(targetHandle, equipmentHandle, aimValid, aimDirection, staticAttack))));

        BAPIf* fireIf;
        if (guidance == 0) {
            BotScript* const fireSequence = buildDirectFire(bot, plan, weaponValue, targetValue, aimValid,
                                                            aimDirection, aimPosition, lookPosition, weapon,
                                                            own, control, target, targetPhysical, guidance);
            Vec3 zero = {0.0f, 0.0f, 0.0f};
            BotScript* const holdAim = checkPointer(new BAPAimAtObject(
                bot, targetHandle, zero, checkPointer(new BAPConFalse), control->getAimControls(),
                m_behaviourId, false, aimValid, aimDirection));
            fireIf = checkPointer(new BAPIf(ready, fireSequence, holdAim, m_behaviourId, true, false));
        } else {
            // PE 10108E11: the guided sequence, with a look held on the target while not ready.
            BotScript* const fireSequence = buildGuidedFire(bot, plan, weaponValue, targetValue, lookPosition,
                                                            weapon, own, control, target, targetPhysical,
                                                            guidance);
            BotScript* const holdLook = checkPointer(new BAPALookAtObject(
                bot, targetHandle, 0.0f, checkPointer(new BAPConFalse), control->getLookControls(),
                m_behaviourId, lookPosition, 0.0f));
            fireIf = checkPointer(new BAPIf(ready, fireSequence, holdLook, m_behaviourId, true, false));
        }

        BAPParallel* const body = checkPointer(new BAPParallel(m_behaviourId, 4, false));
        body->addStatement(checkPointer(new BAPEvaluateCondition(canFireShared, m_behaviourId)));
        body->addStatement(checkPointer(new BAPEvaluateCondition(aimShared, m_behaviourId)));
        if (!guidance)
            body->addStatement(checkPointer(new BAPAimingWrapper(aimDirection, aimValid, aimPosition,
                                                                 equipmentHandle, targetHandle, true)));
        else
            body->addStatement(checkPointer(new BAPLookAtWrapper(targetHandle, lookPosition, true,
                                                                 guidance == 3, aimDirection, aimValid)));

        BotScript* move = 0;
        if (!antiAircraftAtVehicle && !noMove) {
            const unsigned int id = m_behaviourId;
            // Close in until the weapon reaches, then forget the target and end the plan.
            BotScript* closeIn;
            if (targetMobile) {
                const float nearest = std::max(15.0f, weapon->getMinRange() * 1.1f);   // PE 10086630
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.5f;
                closeIn = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, 0.0f, mobile->getMaxSpeed(),
                                                                  slow, 0, drive, id, 0, nearest, 3));
            } else {
                const float nearest = std::max(15.0f, weapon->getMinRange() * 1.1f);
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.75f;
                closeIn = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0,
                                                            drive, id, 0.0f, nearest));
            }
            BAPSerial* const closeInPlan = checkPointer(new BAPSerial(id, 300));
            closeInPlan->addStatement(closeIn);
            closeInPlan->addStatement(checkPointer(new BAPForgetTarget(plan->m_target)));
            closeInPlan->addStatement(checkPointer(new BAPPlanEnd(id, 0, 0)));

            // Out of the band [max(18, min * 1.05), max * 0.5]: move to a third of the range.
            BAPConObjectDistance* const tooFar = checkPointer(
                new BAPConObjectDistance(targetHandle, weapon->getMaxRange() * 0.5f, true, false, true));
            BAPConOr* const outOfBand = checkPointer(new BAPConOr);
            outOfBand->addCondition(tooFar);
            outOfBand->addCondition(checkPointer(new BAPConObjectDistance(
                targetHandle, std::max(18.0f, weapon->getMinRange() * 1.05f), true, false, false)));
            BotScript* toBand;
            if (targetMobile) {
                const float nearest = std::max(15.0f, weapon->getMinRange() * 1.1f);
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.5f;
                const float fast = mobile->getMaxSpeed();
                toBand = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, weapon->getMaxRange() * 0.35f,
                                                                 fast, slow, 0, drive, id, 0, nearest, 4));
            } else {
                const float nearest = std::max(15.0f, weapon->getMinRange() * 1.05f);
                const float band = weapon->getMaxRange() * 0.35f;
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.75f;
                toBand = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0, drive,
                                                           id, band, nearest));
            }
            BAPIf* const bandIf = checkPointer(new BAPIf(outOfBand, toBand, 0, id, false, false));

            // While aimed: closer than max(15, min) + 5, back off; otherwise circle in to 0.7 of
            // the range, stopping once the target holds still within 50.
            BAPConObjectDistance* const tooClose = checkPointer(new BAPConObjectDistance(
                targetHandle, std::max(15.0f, weapon->getMinRange()) + 5.0f, true, false, true));
            BotScript* backOff;
            BotScript* circle;
            if (targetMobile) {
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.5f;
                const float fast = mobile->getMaxSpeed();
                backOff = checkPointer(new BAPMoveToObjectFinding(
                    bot, plan->m_target, std::max(10.0f, weapon->getMinRange() * 1.1f), fast, slow, 0, drive, id, 0,
                    0.0f, 5));
                BAPConAnd* const settled = checkPointer(new BAPConAnd);
                settled->addCondition(checkPointer(
                    new BAPConObjectDistance(m_targets[bot->getBotId()], 50.0f, true, true, false)));
                settled->addCondition(checkPointer(new BAPConObjectMoved(m_targets[bot->getBotId()], 5.0f)));
                const float nearest = std::max(30.0f, weapon->getMinRange() * 2.2f);
                const unsigned long long drive2 = control->getDriveControls();
                const float slow2 = mobile->getMaxSpeed() * 0.5f;
                const float fast2 = mobile->getMaxSpeed();
                circle = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, weapon->getMaxRange() * 0.7f,
                                                                 fast2, slow2, 0, drive2, id, settled, nearest, 6));
            } else {
                const float reach = std::max(10.0f, weapon->getMinRange() * 1.1f);
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.75f;
                backOff = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0, drive,
                                                            id, reach, 0.0f));
                const float nearest = std::max(30.0f, weapon->getMinRange() * 2.2f);
                const float band = weapon->getMaxRange() * 0.7f;
                const unsigned long long drive2 = control->getDriveControls();
                const float slow2 = mobile->getMaxSpeed() * 0.75f;
                circle = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow2, 0, drive2,
                                                           id, band, nearest));
            }
            BAPIf* const rangeIf = checkPointer(new BAPIf(tooClose, backOff, circle, id, false, false));
            BAPIf* const aimIf = checkPointer(new BAPIf(checkPointer(new BAPConNot(aimShared->share())), rangeIf,
                                                        bandIf, id, false, false));
            move = checkPointer(new BAPIf(checkPointer(new BAPConNot(canFireShared->share())), closeInPlan, aimIf,
                                          id, true, false));
        }

        if (areaShared) {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            BAPMoveToFinding* const toArea = checkPointer(new BAPMoveToFinding(
                bot, approach, mobile->getMaxSpeed(), slow, 0, drive, m_behaviourId, 0.0f, 0.0f));
            body->addStatement(checkPointer(new BAPIf(areaShared, toArea, move, m_behaviourId, false, false)));
        } else if (move) {
            body->addStatement(move);
        } else if (noMove && hasLeaderPosition && !isSamePosition(leaderPosition, zeroVector())) {
            main->setIsGoingForFlag(true);
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            body->addStatement(checkPointer(new BAPMoveToFinding(bot, leaderPosition, mobile->getMaxSpeed(), slow, 0,
                                                                 drive, m_behaviourId, 0.0f, 0.0f)));
        }
        body->addStatement(fireIf);
        BAPConAnd* const alive = checkPointer(new BAPConAnd);
        alive->addCondition(checkPointer(
            new BAPConNot(checkPointer(new BAPConObjectHealthBelow(targetHandle, 0.0f)))));
        BAPWhile* const loop = checkPointer(new BAPWhile(alive, body, false, m_behaviourId));
        if (!ownSoldier) {
            // The first argument is the bot pointer in retail, stored and never read; on a 64-bit
            // port only its low half fits. [PLATFORM]
            sequence->addStatement(new BAPUseFlareOnMissileLock(
                static_cast<int>(reinterpret_cast<std::size_t>(bot)), 0x800000000000ULL, m_behaviourId));
        }
        sequence->addStatement(loop);
        return finishScript(bot, plan, weapon, targetHandle, root);
    }
    if (heavy) {
        // ---- A heavy weapon (PE 10109B68) ----
        // While the target lives: hold the aim on it and run the deployment once. Close to
        // contact range, unpack with alternate fire while the weapon is in state one, fire, lock
        // turning, move until the weapon reports ready, back off to thirty metres, then, in a
        // crouched pose, pack again and fire once the weapon is back in state one; unlock turning.
        bool heavyTargetMobile = target->getMobile() != 0;   // recomputed, as shipped
        if (!heavyTargetMobile && !target->isPrimary())
            heavyTargetMobile = informationManager->getInformation(target->getPrimaryObject(bot->getSide()))
                                    ->getMobile() != 0;
        {
            BAPOverrideSensingAgent* const sensing = new BAPOverrideSensingAgent;
            BAPConNot* const notSprinting = new BAPConNot(checkPointer(new BAPConSprinting));
            sequence->addStatement(checkPointer(new BAPIf(checkPointer(notSprinting), checkPointer(sensing), 0,
                                                          m_behaviourId, true, false)));
        }
        const int id = m_behaviourId;
        BAPParallel* const body = checkPointer(new BAPParallel(id, 4, false));
        BAPConAnd* const alive = checkPointer(new BAPConAnd);
        alive->addCondition(checkPointer(
            new BAPConNot(checkPointer(new BAPConObjectHealthBelow(targetHandle, 0.0f)))));
        BAPWhile* const loop = checkPointer(new BAPWhile(alive, body, false, id));
        {
            Vec3 zero = {0.0f, 0.0f, 0.0f};
            body->addStatement(checkPointer(new BAPAimAtObject(bot, targetHandle, zero, checkPointer(new BAPConFalse),
                                                               control->getAimControls(), id, false, 0, 0)));
        }
        BAPSerial* const deploy = checkPointer(new BAPSerial(id, 1));
        const float contact = targetPhysical->getSmallestRadius() + physical->getSmallestRadius();

        if (heavyTargetMobile) {
            {
                const unsigned long long drive = control->getDriveControls();
                const float slow = mobile->getMaxSpeed() * 0.5f;
                deploy->addStatement(checkPointer(new BAPMoveToObjectFinding(
                    bot, plan->m_target, contact, mobile->getMaxSpeed(), slow, 0, drive, id, 0, 0.0f, 1)));
            }
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.5f;
            BAPMoveToObjectFinding* const follow = checkPointer(new BAPMoveToObjectFinding(
                bot, plan->m_target, 0.0f, mobile->getMaxSpeed(), slow, 0, drive, id, 0, 0.0f, 3));
            BAPConTimer* const settle = checkPointer(new BAPConTimer(0.20000000298023224, false, false, false));
            deploy->addStatement(checkPointer(new BAPWhile(checkPointer(new BAPConNot(settle)), follow, false, id)));
        } else {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            deploy->addStatement(checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0,
                                                                   drive, id, 0.0f, 0.0f)));
        }

        {
            BAPConFalse* const release = new BAPConFalse;
            BAPConTrue* const until = new BAPConTrue;
            BAPTrigger* const unpack = new BAPTrigger(weapon->getWeaponAltFire(), checkPointer(until),
                                                      checkPointer(release),
                                                      inputMask32(weapon->getWeaponAltFire()), id, 0.0, true);
            BAPConWeaponStateOne* const stateOne = new BAPConWeaponStateOne(weaponValue);
            deploy->addStatement(checkPointer(new BAPWhile(checkPointer(stateOne), checkPointer(unpack), false, id)));
        }
        {
            BAPConFalse* const release = new BAPConFalse;
            BAPConTrue* const until = new BAPConTrue;
            deploy->addStatement(checkPointer(new BAPTrigger(weapon->getWeaponFire(), checkPointer(until),
                                                             checkPointer(release),
                                                             inputMask32(weapon->getWeaponFire()), id, 0.0, true)));
        }
        deploy->addStatement(checkPointer(new BAPICTurnLock(true)));

        BotScript* approach;
        if (heavyTargetMobile) {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.5f;
            approach = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, 0.0f, mobile->getMaxSpeed(), slow,
                                                               0, drive, id, 0, 0.0f, 2));
        } else {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            approach = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0, drive, id,
                                                         0.0f, 0.0f));
        }
        BAPConWeaponReady* const ready = checkPointer(new BAPConWeaponReady(weaponValue));
        deploy->addStatement(checkPointer(new BAPWhile(checkPointer(new BAPConNot(ready)), approach, false, id)));

        const float clearance = contact + 9.0f;   // PE 1027E4D8
        if (heavyTargetMobile) {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.5f;
            deploy->addStatement(checkPointer(new BAPMoveToObjectFinding(
                bot, plan->m_target, 30.0f, mobile->getMaxSpeed(), slow, 0, drive, id, 0, clearance, 2)));
        } else {
            const unsigned long long drive = control->getDriveControls();
            const float slow = mobile->getMaxSpeed() * 0.75f;
            deploy->addStatement(checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0,
                                                                   drive, id, 30.0f, clearance)));
        }

        BAPParallel* const fireStage = checkPointer(new BAPParallel(id, -1, false));
        fireStage->addStatement(checkPointer(new BAPSoldierPose(2, checkPointer(new BAPConTrue), id, true)));
        BAPWhile* packLoop;
        {
            BAPConFalse* const release = new BAPConFalse;
            BAPConTrue* const until = new BAPConTrue;
            BAPTrigger* const pack = new BAPTrigger(weapon->getWeaponAltFire(), checkPointer(until),
                                                    checkPointer(release),
                                                    inputMask32(weapon->getWeaponAltFire()), id, 0.0, false);
            BAPConNot* const notStateOne = new BAPConNot(checkPointer(new BAPConWeaponStateOne(weaponValue)));
            packLoop = checkPointer(new BAPWhile(checkPointer(notStateOne), checkPointer(pack), false, id));
        }
        BAPSerial* const shots = checkPointer(new BAPSerial(id, -1));
        shots->addStatement(packLoop);
        shots->addStatement(checkPointer(new BAPIdle(
            checkPointer(new BAPConTimer(0.699999988079071, false, false, false)), 0, id)));
        {
            BAPConFalse* const release = new BAPConFalse;
            BAPConWeaponStateOne* const stateOne = new BAPConWeaponStateOne(weaponValue);
            BAPTrigger* const shot = new BAPTrigger(weapon->getWeaponFire(), checkPointer(stateOne),
                                                    checkPointer(release), inputMask32(weapon->getWeaponFire()),
                                                    id, 0.0, false);
            shots->addStatement(checkPointer(checkPointer(shot)));
        }
        fireStage->addStatement(shots);
        deploy->addStatement(fireStage);
        deploy->addStatement(checkPointer(new BAPICTurnLock(false)));
        body->addStatement(deploy);
        body->addStatement(checkPointer(new BAPMergeSoldierInputs(id)));
        sequence->addStatement(loop);
        return finishScript(bot, plan, weapon, targetHandle, root);
    }

    // ---- A known target, a weapon that is not heavy (PE 1010A7E2) ----
    const bool splash = weapon->getExplosionRadius() != 0.0f && guidance == 0;
    const SensedObject* const sensed =
        static_cast<const SensedObject*>(bot->getSensedObjectFromHandle(targetHandle));
    const bool high = main->getSpawnKit() != 1 && !weapon->getIsThrown()
                      && target->getPosition().y - own->getPosition().y > 30.0f;
    // The sensed record is not null-checked, as shipped.
    const bool reported = main->getUseStaticAttack() && splash && targetSoldier
                          && ((sensed->m_senseFlags & 8) || high);

    const float rise = target->getPosition().y - own->getPosition().y;
    float extraRange = 0.0f;
    if (rise > 5.0f && weapon->getMaxRange() > 10.0f && !weapon->getIsThrown())
        extraRange = rise * 1.25f;
    const float maxRange = weapon->getMaxRange() + extraRange;
    float minRange = weapon->getMinRange();
    if (rise > 3.0f && reported && weapon->getIsThrown())
        minRange = 5.0f;

    if (ownSoldier) {
        BAPOverrideSensingAgent* const sensing = checkPointer(new BAPOverrideSensingAgent);
        BAPConNot* const notSprinting = new BAPConNot(checkPointer(new BAPConSprinting));
        sequence->addStatement(checkPointer(new BAPIf(checkPointer(notSprinting), sensing, 0,
                                                      m_behaviourId, true, false)));
    } else {
        sequence->addStatement(checkPointer(new BAPOverrideSensingAgent));
    }

    BAPConAnd* const fireReady = checkPointer(new BAPConAnd);
    BAPConAnd* const aimReady = checkPointer(new BAPConAnd);
    if (m_weapons[bot->getBotId()] != -1 && plan->m_int08 != 0 && guidance == 0)
        aimReady->addCondition(checkPointer(
            new BAPConAimOnTarget(targetHandle, equipmentHandle, aimValid, aimDirection, reported)));
    if (!reported)
        aimReady->addCondition(checkPointer(new BAPConCanFireAt(targetHandle, controlArgument != 0)));
    BAPConShared* const canFire = checkPointer(new BAPConShared(aimReady));
    fireReady->addCondition(canFire->share());

    // Near a control point the bot may also fire from its approach.
    BAPConShared* areaCondition = 0;
    AIStrategicArea* const area = static_cast<AIStrategicArea*>(
        AIStrategicObjectManager::getInstance()->getClosestObject(ownPosition, bot->getSide(), false));
    Vec3 approach = {0.0f, 0.0f, 0.0f};
    if ((!ownSoldier || weapon->getMaxRange() > 5.0f) && area && !noMove
        && area->m_controlPointId != static_cast<unsigned int>(-1)) {
        area->getControlPointApproach(area, bot->getSide(), mobile->getVehicleNumber(),
                                      own->getUnit()->getEquipmentType(), ownPosition, approach);
        const float radius = area->m_size[bot->getSide()];
        const Vec2& centre = area->getPosition(bot->getSide());
        const Vec3 offset = {centre.x - approach.x, 0.0f, centre.y - approach.z};
        if (radius * radius > lengthSq(offset)) {
            BAPConAnd* const atArea = checkPointer(new BAPConAnd);
            atArea->addCondition(checkPointer(new BAPConStrategicStatus(area, bot->getSide(), false)));
            atArea->addCondition(checkPointer(new BAPConPositionDistance(approach, 40.0f, true)));
            areaCondition = checkPointer(new BAPConShared(atArea));
        }
    }
    if (!reported) {
        if (areaCondition) {
            BAPConOr* const either = checkPointer(new BAPConOr);
            either->addCondition(checkPointer(
                new BAPConObjectDistance(targetHandle, minRange * 0.9f, maxRange * 1.1f, true, false)));
            either->addCondition(areaCondition->share());
            fireReady->addCondition(either);
        } else {
            fireReady->addCondition(checkPointer(
                new BAPConObjectDistance(targetHandle, minRange * 0.9f, maxRange * 1.1f, true, false)));
        }
    }

    BotScript* fireSequence = 0;
    if (m_weapons[bot->getBotId()] != -1 && plan->m_int08 != 0)
        fireSequence = buildDirectFire(bot, plan, weaponValue, targetValue, aimValid, aimDirection,
                                       aimPosition, lookPosition, weapon, own, control, target,
                                       targetPhysical, guidance);
    Vec3 zero = {0.0f, 0.0f, 0.0f};
    BotScript* const holdAim = checkPointer(new BAPAimAtObject(
        bot, targetHandle, zero, checkPointer(new BAPConFalse), control->getAimControls(), m_behaviourId,
        false, aimValid, aimDirection));
    BAPIf* const fireIf = checkPointer(new BAPIf(fireReady, fireSequence, holdAim, m_behaviourId, true, false));

    BAPParallel* const body = checkPointer(new BAPParallel(m_behaviourId, 4, false));
    if (!guidance)
        body->addStatement(checkPointer(new BAPAimingWrapper(aimDirection, aimValid, aimPosition,
                                                             equipmentHandle, targetHandle, true)));
    else
        body->addStatement(checkPointer(new BAPLookAtWrapper(targetHandle, lookPosition, true, guidance == 3,
                                                             aimDirection, aimValid)));

    BotScript* move = 0;
    if (!noMove) {
        BAPConOr* const moveWhen = checkPointer(new BAPConOr);
        if (!reported) {
            moveWhen->addCondition(checkPointer(new BAPConNot(canFire->share())));
            moveWhen->addCondition(checkPointer(new BAPConNot(checkPointer(new BAPConFireAllowed))));
        } else {
            moveWhen->addCondition(checkPointer(new BAPConFalse));
        }

        bool targetMobileNow = target->getMobile() != 0;
        if (!targetMobileNow && !target->isPrimary())
            targetMobileNow = informationManager->getInformation(target->getPrimaryObject(bot->getSide()))
                                  ->getMobile() != 0;

        // Close in when no shot is possible, then forget the target and end the plan.
        BotScript* closeIn;
        if (!targetMobileNow) {
            const float slow = mobile->getMaxSpeed() * 0.75f;
            closeIn = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0,
                                                        control->getDriveControls(), m_behaviourId, 0.5f,
                                                        minRange * 1.1f));
        } else {
            const float slow = mobile->getMaxSpeed() * 0.5f;
            closeIn = checkPointer(new BAPMoveToObjectFinding(bot, plan->m_target, 0.0f, mobile->getMaxSpeed(),
                                                              slow, 0, control->getDriveControls(),
                                                              m_behaviourId, 0, minRange * 1.1f, 7));
        }
        BAPSerial* const closeInPlan = checkPointer(new BAPSerial(m_behaviourId, 300));
        closeInPlan->addStatement(closeIn);
        closeInPlan->addStatement(checkPointer(new BAPForgetTarget(plan->m_target)));
        closeInPlan->addStatement(checkPointer(new BAPPlanEnd(m_behaviourId, 0, 0)));

        // Otherwise keep to a preferred distance inside the weapon's range.
        float share = weapon->getOptimalRangePercentage() * 0.01f;
        if (weapon->getMaxRange() > 10.0f)
            share = (AIRandom::m_instance->nextRange(0.0f, 0.4f) + 0.8f) * share;
        float preferred = (maxRange - minRange) * share + minRange;
        if (reported && !weapon->getIsThrown()) {
            const float above = target->getPosition().y - own->getPosition().y;
            if (above > 0.0f)
                preferred += above * 0.25f;
        }
        BotScript* keepDistance;
        if (targetMobileNow) {
            const float slow = mobile->getMaxSpeed() * 0.5f;
            keepDistance = checkPointer(new BAPMoveToObjectFinding(
                bot, plan->m_target, preferred, mobile->getMaxSpeed(), slow, 0, control->getDriveControls(),
                m_behaviourId, 0, minRange * 1.1f, 8));
        } else {
            const float slow = mobile->getMaxSpeed() * 0.75f;
            keepDistance = checkPointer(new BAPMoveToFinding(bot, firePosition, mobile->getMaxSpeed(), slow, 0,
                                                             control->getDriveControls(), m_behaviourId,
                                                             preferred, minRange * 1.1f));
        }
        BAPConObjectDistance* const inBand = checkPointer(
            new BAPConObjectDistance(targetHandle, minRange, preferred * 1.1f, true, false));
        BAPIf* const keepIf = checkPointer(new BAPIf(checkPointer(new BAPConNot(inBand)), keepDistance, 0,
                                                     m_behaviourId, false, false));
        move = checkPointer(new BAPIf(moveWhen, closeInPlan, keepIf, m_behaviourId, true, true));
    }

    // Reload once the magazine is empty.
    BAPConMagazine* const empty = checkPointer(
        new BAPConMagazine(weaponValue, context->createVariable<int>(0, 0), false));
    BAPConFalse* const release = new BAPConFalse;
    BAPConTrue* const until = new BAPConTrue;
    BAPTrigger* const reload = new BAPTrigger(32, checkPointer(until), checkPointer(release),
                                              1ULL << 32, m_behaviourId, 0.0, false);
    body->addStatement(checkPointer(new BAPIf(empty, checkPointer(reload), 0, m_behaviourId, true, false)));
    body->addStatement(checkPointer(new BAPEvaluateCondition(canFire, m_behaviourId)));

    if (areaCondition && !noMove) {
        const float slow = mobile->getMaxSpeed() * 0.75f;
        BAPMoveToFinding* const toArea = new BAPMoveToFinding(
            bot, approach, mobile->getMaxSpeed(), slow, 0, control->getDriveControls(), m_behaviourId, 0.0f, 0.0f);
        body->addStatement(checkPointer(new BAPIf(areaCondition, checkPointer(toArea), move, m_behaviourId,
                                                  false, false)));
    } else if (move) {
        body->addStatement(move);
    } else if (noMove && hasLeaderPosition && !isSamePosition(leaderPosition, zeroVector())) {
        main->setIsGoingForFlag(true);
        const float slow = mobile->getMaxSpeed() * 0.75f;
        body->addStatement(checkPointer(new BAPMoveToFinding(bot, leaderPosition, mobile->getMaxSpeed(), slow, 0,
                                                             control->getDriveControls(), m_behaviourId,
                                                             0.0f, 0.0f)));
    }
    body->addStatement(fireIf);
    if (ownSoldier)
        body->addStatement(checkPointer(new BAPMergeSoldierInputs(m_behaviourId)));
    BAPConAnd* const alive = checkPointer(new BAPConAnd);
    alive->addCondition(checkPointer(
        new BAPConNot(checkPointer(new BAPConObjectHealthBelow(targetHandle, 0.0f)))));
    sequence->addStatement(checkPointer(new BAPWhile(alive, body, false, m_behaviourId)));

    return finishScript(bot, plan, weapon, targetHandle, root);
}

// The shared closing of the fire plans (PE 1010C056): the fire wrapper and the squad's share of
// the target. [CHOSEN, not recovered] packaging as a member function.
BotScript* BBPFireInfantery::finishScript(Bot* bot, FireInfanteryTarget* plan, WeaponInfo* weapon,
                                          unsigned int targetHandle, BotScript* root)
{
    const int weaponIndex = weapon ? m_weapons[bot->getBotId()] : -1;
    BAPFireWrapper* const wrapper = checkPointer(new BAPFireWrapper(
        bot, targetHandle, m_flagA, plan->m_bool15, weaponIndex, -1, root, m_behaviourId));
    wrapper->m_owner = this;
    SquadManager::m_instance->shareTarget(bot->getPlayerId(), bot->getSide(), m_targets[bot->getBotId()]);
    return wrapper;
}

} // namespace ai
} // namespace hfe
} // namespace dice
