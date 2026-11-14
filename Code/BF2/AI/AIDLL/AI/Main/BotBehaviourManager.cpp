
#include "BotBehaviourManager.h"

#include "../AIBotBehaviour/UrgeCurve.h"     // UCConstant / UCLinear / UCXInverse - the three factories
#include "../AIUtils/checkPointer.h"         // inlined 26 times in the constructor
#include "../AIBotBehaviour/BotBehaviours.h"
#include "../AIBotBehaviour/BBTakeCoverInfantry.h"
#include "../AIBotBehaviourPattern/PlanGenerators.h"
#include "AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

BotBehaviourManager* BotBehaviourManager::m_instance = 0;

// Behavior factories registered by name.

namespace {

// - builds one `BBAvoid`.
class BBCreateFunctionAvoid : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBMoveTo`.
class BBCreateFunctionMoveTo : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBMoveToFixed`.
class BBCreateFunctionMoveToFixed : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBMedicAssist`.
class BBCreateFunctionMedicAssist : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBIdle`.
class BBCreateFunctionIdle : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFire`.
class BBCreateFunctionFire : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFire3d`.
class BBCreateFunctionFire3d : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFireInfantery`.
class BBCreateFunctionFireInfantery : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFireUnarmed`.
class BBCreateFunctionFireUnarmed : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFireLargeBore`.
class BBCreateFunctionFireLargeBore : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBTakeCover`.
class BBCreateFunctionTakeCover : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBTakeCoverInfantry`.
class BBCreateFunctionTakeCoverInfantry : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBFireDriver`.
class BBCreateFunctionFireDriver : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBChange`.
class BBCreateFunctionChange : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBChangeLandingCraft`.
class BBCreateFunctionChangeLandingCraft : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBChangeTeleport`.
class BBCreateFunctionChangeTeleport : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBSuicide`.
class BBCreateFunctionSuicide : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBCallForMedic`.
class BBCreateFunctionCallForMedic : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBRevive`.
class BBCreateFunctionRevive : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBTakeCover3d`.
class BBCreateFunctionTakeCover3d : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBC4`.
class BBCreateFunctionC4 : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBRunOverSoldier`.
class BBCreateFunctionRunOverSoldier : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBBridgeRepair`.
class BBCreateFunctionBridgeRepair : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBDefuseMines`.
class BBCreateFunctionDefuseMines : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBRandom`.
class BBCreateFunctionRandom : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

// - builds one `BBTriggerable`.
class BBCreateFunctionTriggerable : public BBCreateFunction
{
public:
    virtual BotBehaviour* operator()(int behaviourId, std::string& name, int mode,
                           UrgeCurve* urgeCurve, float* weights);
};

} // anonymous namespace

// Initializes the behavior registry and its owned collections.

BotBehaviourManager::BotBehaviourManager()
{
    m_instance = this;

    checkPointer(m_createFunctions["BBAvoid"]              = new BBCreateFunctionAvoid());
    checkPointer(m_createFunctions["BBMoveTo"]             = new BBCreateFunctionMoveTo());
    checkPointer(m_createFunctions["BBMoveToFixed"]        = new BBCreateFunctionMoveToFixed());
    checkPointer(m_createFunctions["BBMedicAssist"]        = new BBCreateFunctionMedicAssist());
    checkPointer(m_createFunctions["BBIdle"]               = new BBCreateFunctionIdle());
    checkPointer(m_createFunctions["BBFire"]               = new BBCreateFunctionFire());
    checkPointer(m_createFunctions["BBFire3d"]             = new BBCreateFunctionFire3d());
    checkPointer(m_createFunctions["BBFireInfantery"]      = new BBCreateFunctionFireInfantery());
    checkPointer(m_createFunctions["BBFireUnarmed"]        = new BBCreateFunctionFireUnarmed());
    checkPointer(m_createFunctions["BBFireLargeBore"]      = new BBCreateFunctionFireLargeBore());
    checkPointer(m_createFunctions["BBTakeCover"]          = new BBCreateFunctionTakeCover());
    checkPointer(m_createFunctions["BBTakeCoverInfantry"]  = new BBCreateFunctionTakeCoverInfantry());
    checkPointer(m_createFunctions["BBFireDriver"]         = new BBCreateFunctionFireDriver());
    checkPointer(m_createFunctions["BBChange"]             = new BBCreateFunctionChange());
    checkPointer(m_createFunctions["BBChangeLandingCraft"] = new BBCreateFunctionChangeLandingCraft());
    checkPointer(m_createFunctions["BBChangeTeleport"]     = new BBCreateFunctionChangeTeleport());
    checkPointer(m_createFunctions["BBSuicide"]            = new BBCreateFunctionSuicide());
    checkPointer(m_createFunctions["BBCallForMedic"]       = new BBCreateFunctionCallForMedic());
    checkPointer(m_createFunctions["BBRevive"]             = new BBCreateFunctionRevive());
    checkPointer(m_createFunctions["BBTakeCover3d"]        = new BBCreateFunctionTakeCover3d());
    checkPointer(m_createFunctions["BBC4"]                 = new BBCreateFunctionC4());
    checkPointer(m_createFunctions["BBRunOverSoldier"]     = new BBCreateFunctionRunOverSoldier());
    checkPointer(m_createFunctions["BBBridgeRepair"]       = new BBCreateFunctionBridgeRepair());
    checkPointer(m_createFunctions["BBDefuseMines"]        = new BBCreateFunctionDefuseMines());
    checkPointer(m_createFunctions["BBRandom"]             = new BBCreateFunctionRandom());
    checkPointer(m_createFunctions["BBTriggerable"]        = new BBCreateFunctionTriggerable());
}

// Clears behavior state and registry containers. Factory objects are not deleted here.

BotBehaviourManager::~BotBehaviourManager()
{
    m_instance = 0;
    reset();
}

// The only virtual, slot 0.
// It drops everything the `.ai` scripts defined - the behaviour names, their ids and the urge
// curves - and keeps the twenty-six create functions, which are compiled in and outlive any level.
// The curve loop is read literally: for each node, load the mapped pointer at `+20`, and if it is
// non-null call `(*(*ptr + 4))(ptr)` - Itanium vtable slot 1, the deleting destructor, i.e. source
// `delete`. Then advance, rebalance-for-erase, release the key string and free the node.

bool BotBehaviourManager::reset()
{
    m_behaviourIds.clear();

    std::map<std::string, UrgeCurve*, NoCaseStringCompare>::iterator it = m_urgeCurves.begin();
    while (it != m_urgeCurves.end())
    {
        if (it->second)
            delete it->second;
        m_urgeCurves.erase(it++);
    }

    m_behaviourNames.clear();
    return true;
}

// The behaviour name/id table.

// Associates a behavior ID with its script-visible name.
void BotBehaviourManager::setBehaviourName(int id, std::string name)
{
    m_behaviourNames.resize(id + 1);
    m_behaviourNames[id] = name;
    m_behaviourIds[name] = id;
}

// . -1 for "no such behaviour"; `create` below tests for exactly that.
int BotBehaviourManager::getBehaviourId(std::string name)
{
    std::map<std::string, int, NoCaseStringCompare>::iterator it = m_behaviourIds.find(name);
    if (it != m_behaviourIds.end())
        return it->second;

    return -1;
}

// Requires a nonnegative id. The signed upper-bound comparison intentionally permits
// negative values, which would index before the vector.
std::string BotBehaviourManager::getBehaviourName(int id)
{
    if (id < (int)m_behaviourNames.size())
        return m_behaviourNames[id];

    return "";
}

// Creates a named behavior through a registered factory.

BotBehaviour* BotBehaviourManager::create(std::string behaviourName, std::string createFunctionName,
                                std::string name, int mode, std::string urgeCurveName,
                                float* weights, std::string& message)
{
    const int behaviourId = getBehaviourId(behaviourName);
    if (behaviourId == -1)
    {
        message = "'" + behaviourName + "' doesn't exist";
        return 0;
    }

    UrgeCurve* urgeCurve = 0;
    std::map<std::string, UrgeCurve*, NoCaseStringCompare>::iterator curve =
        m_urgeCurves.find(urgeCurveName);
    if (curve != m_urgeCurves.end())
        urgeCurve = curve->second;

    BBCreateFunction* const createFunction = m_createFunctions[createFunctionName];
    if (createFunction)
        return (*createFunction)(behaviourId, name, mode, urgeCurve, weights);

    message = "AIConsole::constructBehaviour, internal error";
    return 0;
}

// Creates and registers an urgency curve under the supplied name.

std::string BotBehaviourManager::createUCConstant(std::string& name, float value)
{
    m_urgeCurves[name] = new UCConstant(value);
    return "UCConstant created";
}

std::string BotBehaviourManager::createUCLinear(std::string& name, float slope, float offset)
{
    m_urgeCurves[name] = new UCLinear(slope, offset);
    return "UCLinear created";
}

std::string BotBehaviourManager::createUCXInverse(std::string& name, float numerator, float offset,
                                                  float slope, float bias)
{
    m_urgeCurves[name] = new UCXInverse(numerator, offset, slope, bias);
    return "UCXInverse created";
}

// The twenty-six create functions. Each picks the plan generator named by the script, or none
// for an unknown name, and builds the behaviour around it; allocations are checked, not refused.

namespace {

// PE 100686D0.
BotBehaviour* BBCreateFunctionAvoid::operator()(int behaviourId, std::string& name, int mode,
                                                UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPAvoidCollision2d")
        generator = checkPointer(new BBPAvoidCollision2d(behaviourId));
    else if (name == "BBPAvoidCollision3d")
        generator = checkPointer(new BBPAvoidCollision3d(behaviourId));
    else if (name == "BBPAvoidCollisionInfantery")
        generator = checkPointer(new BBPAvoidCollisionInfantery(behaviourId));
    return checkPointer(new BBAvoid(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 100689E0.
BotBehaviour* BBCreateFunctionMoveTo::operator()(int behaviourId, std::string& name, int mode,
                                                 UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPGotoWaypoint2d")
        generator = checkPointer(new BBPGotoWaypoint2d(behaviourId));
    else if (name == "BBPGotoWaypointBoat")
        generator = checkPointer(new BBPGotoWaypointBoat(behaviourId));
    else if (name == "BBPGotoWaypoint3d")
        generator = checkPointer(new BBPGotoWaypoint3d(behaviourId));
    else if (name == "BBPGotoWaypointInfantery")
        generator = checkPointer(new BBPGotoWaypointInfantery(behaviourId));
    return checkPointer(new BBMoveTo(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10067BC0.
BotBehaviour* BBCreateFunctionMoveToFixed::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    (void)name;
    (void)urgeCurve;
    (void)weights;
    return checkPointer(new BBMoveToFixed(behaviourId, mode));
}

// PE 100695C0.
BotBehaviour* BBCreateFunctionMedicAssist::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPMedicAssist")
        generator = checkPointer(new BBPMedicAssist(behaviourId));
    return checkPointer(new BBMedicAssist(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10068DC0.
BotBehaviour* BBCreateFunctionIdle::operator()(int behaviourId, std::string& name, int mode,
                                               UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPIdle2d")
        generator = checkPointer(new BBPIdle2d(behaviourId));
    else if (name == "BBPIdle3d")
        generator = checkPointer(new BBPIdle3d(behaviourId));
    else if (name == "BBPIdleInfantery")
        generator = checkPointer(new BBPIdleInfantery(behaviourId));
    return checkPointer(new BBIdle(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069750.
BotBehaviour* BBCreateFunctionFire::operator()(int behaviourId, std::string& name, int mode,
                                               UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPFire2d")
        generator = checkPointer(new BBPFire2d(behaviourId));
    else if (name == "BBPFireInfantery")
        generator = checkPointer(new BBPFireInfantery(behaviourId));
    return checkPointer(new BBFire(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069C90.
BotBehaviour* BBCreateFunctionFire3d::operator()(int behaviourId, std::string& name, int mode,
                                                 UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPFire3d")
        generator = checkPointer(new BBPFire3d(behaviourId));
    else if (name == "BBPHeliFire3d")
        generator = checkPointer(new BBPHeliFire3d(behaviourId));
    return checkPointer(new BBFire3d(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069EE0.
BotBehaviour* BBCreateFunctionFireInfantery::operator()(int behaviourId, std::string& name, int mode,
                                                        UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPFireInfantery")
        generator = checkPointer(new BBPFireInfantery(behaviourId));
    return checkPointer(new BBFireInfantery(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10067CE0.
BotBehaviour* BBCreateFunctionFireUnarmed::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    (void)name;
    (void)urgeCurve;
    (void)weights;
    return checkPointer(new BBFireUnarmed(behaviourId, mode));
}

// PE 100699A0.
BotBehaviour* BBCreateFunctionFireLargeBore::operator()(int behaviourId, std::string& name, int mode,
                                                        UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPFire2d")
        generator = checkPointer(new BBPFire2d(behaviourId));
    else if (name == "BBPFireInfantery")
        generator = checkPointer(new BBPFireInfantery(behaviourId));
    return checkPointer(
        new BBFireLargeBore(behaviourId, mode, generator, urgeCurve, weights, false));
}

// PE 10069FF0.
BotBehaviour* BBCreateFunctionTakeCover::operator()(int behaviourId, std::string& name, int mode,
                                                    UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPTakeCover")
        generator = checkPointer(new BBPTakeCover(behaviourId));
    return checkPointer(new BBTakeCover(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A180.
BotBehaviour* BBCreateFunctionTakeCoverInfantry::operator()(int behaviourId, std::string& name, int mode,
                                                            UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPTakeCoverInfantry")
        generator = checkPointer(new BBPTakeCoverInfantry(behaviourId));
    return checkPointer(new BBTakeCoverInfantry(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069B00.
BotBehaviour* BBCreateFunctionFireDriver::operator()(int behaviourId, std::string& name, int mode,
                                                     UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPFire2dDriver")
        generator = checkPointer(new BBPFire2dDriver(behaviourId));
    return checkPointer(new BBFireDriver(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 100690D0.
BotBehaviour* BBCreateFunctionChange::operator()(int behaviourId, std::string& name, int mode,
                                                 UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPChange")
        generator = checkPointer(new BBPChange(behaviourId));
    return checkPointer(new BBChange(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069260.
BotBehaviour* BBCreateFunctionChangeLandingCraft::operator()(int behaviourId, std::string& name, int mode,
                                                             UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPChangeLandingCraft")
        generator = checkPointer(new BBPChangeLandingCraft(behaviourId));
    else if (name == "BBPChange")
        generator = checkPointer(new BBPChange(behaviourId));
    return checkPointer(new BBChangeLandingCraft(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 10069430.
BotBehaviour* BBCreateFunctionChangeTeleport::operator()(int behaviourId, std::string& name, int mode,
                                                         UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPChangeTeleport")
        generator = checkPointer(new BBPChangeTeleport(behaviourId));
    return checkPointer(new BBChangeTeleport(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A310.
BotBehaviour* BBCreateFunctionSuicide::operator()(int behaviourId, std::string& name, int mode,
                                                  UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPSuicide")
        generator = checkPointer(new BBPSuicide(behaviourId));
    return checkPointer(new BBSuicide(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A4A0.
BotBehaviour* BBCreateFunctionCallForMedic::operator()(int behaviourId, std::string& name, int mode,
                                                       UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPCallForMedic")
        generator = checkPointer(new BBPCallForMedic(behaviourId));
    return checkPointer(new BBCallForMedic(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A630.
BotBehaviour* BBCreateFunctionRevive::operator()(int behaviourId, std::string& name, int mode,
                                                 UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPRevive")
        generator = checkPointer(new BBPRevive(behaviourId));
    return checkPointer(new BBRevive(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A7C0.
BotBehaviour* BBCreateFunctionTakeCover3d::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPTakeCover3d")
        generator = checkPointer(new BBPTakeCover3d(behaviourId));
    return checkPointer(new BBTakeCover3d(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006A950.
BotBehaviour* BBCreateFunctionC4::operator()(int behaviourId, std::string& name, int mode,
                                             UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPC4")
        generator = checkPointer(new BBPC4(behaviourId));
    return checkPointer(new BBC4(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006AE00.
BotBehaviour* BBCreateFunctionRunOverSoldier::operator()(int behaviourId, std::string& name, int mode,
                                                         UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPRunOverSoldier")
        generator = checkPointer(new BBPRunOverSoldier(behaviourId));
    return checkPointer(new BBRunOverSoldier(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006AAE0.
BotBehaviour* BBCreateFunctionBridgeRepair::operator()(int behaviourId, std::string& name, int mode,
                                                       UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPBridgeRepair")
        generator = checkPointer(new BBPBridgeRepair(behaviourId));
    return checkPointer(new BBBridgeRepair(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006AC70.
BotBehaviour* BBCreateFunctionDefuseMines::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPDefuseMines")
        generator = checkPointer(new BBPDefuseMines(behaviourId));
    return checkPointer(new BBDefuseMines(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006AF90.
BotBehaviour* BBCreateFunctionRandom::operator()(int behaviourId, std::string& name, int mode,
                                                 UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPRandom")
        generator = checkPointer(new BBPRandom(behaviourId));
    return checkPointer(new BBRandom(behaviourId, mode, generator, urgeCurve, weights));
}

// PE 1006B120.
BotBehaviour* BBCreateFunctionTriggerable::operator()(int behaviourId, std::string& name, int mode,
                                                      UrgeCurve* urgeCurve, float* weights)
{
    PlanGenerator* generator = 0;
    if (name == "BBPTriggerable")
        generator = checkPointer(new BBPTriggerable(behaviourId));
    return checkPointer(new BBTriggerable(behaviourId, mode, generator, urgeCurve, weights));
}

} // anonymous namespace

} // namespace ai
} // namespace hfe
} // namespace dice
