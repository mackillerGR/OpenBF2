// [INFERRED] Source placement.

#include "BFPhysics.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Physics/BasicPhysicsSystem.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// First wins in, exact match out.
IAIPhysics::IAIPhysics()
{
    if (!CommonSingletonServer::m_iaiPhysics)
        classManager->registerSimpleSingleton(std::string("IAIPhysics"), this);
}

IAIPhysics::~IAIPhysics()
{
    if (CommonSingletonServer::m_iaiPhysics == this)
        classManager->unregisterSimpleSingleton(std::string("IAIPhysics"));
}

BFPhysics::BFPhysics()
{
}

BFPhysics::~BFPhysics()
{
}

// The engine physics system's gravity. PE BF2.exe 8181D0.
float BFPhysics::getGravity() const
{
    return static_cast<world::IPhysicsSystem*>(world::basicPhysicsSystem)->getGravity();
}

} // namespace ai
} // namespace hfe
} // namespace dice
