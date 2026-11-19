// [INFERRED] the class comment; the source path is attested by the image's __FILE__ string.
#include "PlanGenerators.h"

#include "PlanGeneratorSupport.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSenses.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPStatements.h"
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

// [FRONTIER] [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bbp-fire-infantery-builder]
// Recover PE 10108090 and its sub-builders 10106230 / 101069D0 / 10106E90.
BotScript* BBPFireInfantery::buildScript(Bot*, IPIControlInfo*)
{
    BF2_AI_FRONTIER("dice::hfe::ai::BBPFireInfantery::buildScript");
}

} // namespace ai
} // namespace hfe
} // namespace dice
