// [CHOSEN, not recovered] Client bodies that are not recovered yet. Each stops loudly at first use.
// [RETIRE-WHEN: portable-client-frontiers] the retail body has been read and written.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/Game/AreaObject.h"
#include "../Game/Objects/Fx/EffectBundleTemplate.h"
#include "../Scene/Object/ObjectPriorityCalculator.h"
#include "../Menu/HudArtillerySeams.h"
#include "../External/BFMeme/ClassIStream.h"
#include "../External/BFMeme/MovePointer.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/OStream.h"
#define FRONTIER(name) do { std::fprintf(stderr, "[FRONTIER] " name " reached\n"); std::fflush(stderr); std::abort(); } while (0)
namespace dice { namespace hfe {
namespace ai { void dispatchArtilleryRequest(IAIEventCourier*, int, Vec2) { FRONTIER("dice::hfe::ai::dispatchArtilleryRequest"); } }
bool modInfo_vslot0x30(IBase*, const std::string&) { FRONTIER("dice::hfe::modInfo_vslot0x30"); }
bool unresolvedGlobalA33C08_vslot0x18() { FRONTIER("dice::hfe::unresolvedGlobalA33C08_vslot0x18"); }
namespace world {
void AreaObject::handleFrameUpdate(float) { FRONTIER("dice::hfe::world::AreaObject::handleFrameUpdate"); }
bool EffectBundleTemplate::makeMemberScript(const TemplateInfo*, io::IStream*) { FRONTIER("dice::hfe::world::EffectBundleTemplate::makeMemberScript"); }
void ObjectPriorityCalculator::update() { FRONTIER("dice::hfe::world::ObjectPriorityCalculator::update"); }
}
} }
namespace dice { namespace meme {
TypeDescriptor* ClassIStream::getType() { FRONTIER("dice::meme::ClassIStream::getType"); }
TypeDescriptor* IStream::getType() { FRONTIER("dice::meme::IStream::getType"); }
TypeDescriptor* OStream::getType() { FRONTIER("dice::meme::OStream::getType"); }
TypeDescriptor* MovePointer::getClassType() { FRONTIER("dice::meme::MovePointer::getClassType"); }
TypeDescriptor* getTreeClassType() { FRONTIER("dice::meme::getTreeClassType"); }
void releaseListBoxCellResource(dice::hfe::IBase*, int) { FRONTIER("dice::meme::releaseListBoxCellResource"); }
} }
#endif
