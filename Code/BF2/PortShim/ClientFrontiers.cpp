// [CHOSEN, not recovered] Bodies the portable client links against until the real ones exist.
// [RETIRE-WHEN: portable-client-frontiers] each function has a recovered or ported body.
#include "../Core/BuildConfig.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include "../External/NovintHFX/NovintHFX.h"
#include "../VoipServer/VoipServerApi.h"

// Novint Falcon: no such hardware here, so the system reports zero devices.
namespace NovintHFX {
ApplicationData::mouse_settings::mouse_settings() {}
void Device::InputUpdate() {}
void Device::GetButtonData(int* a, int* b, int* c) const { *a = *b = *c = 0; }
void Device::GetToolPosition(double* p) const { p[0] = p[1] = p[2] = 0.0; }
bool HapticsSystem::AttachApplication(ApplicationData&) { return false; }
void HapticsSystem::RecountConnectedDevices() {}
int HapticsSystem::ConnectedDeviceCount() const { return 0; }
bool HapticsSystem::RunDevice(Device**, int) { return false; }
void HapticsSystem::StartMouse(Device*) {}
bool HapticsSystem::StopRunningDevice(Device**) { return false; }
int HapticsSystem::InputUpdate() { return 0; }
}
// In-game voice hosting is not part of this port yet.
namespace BF2VoipServer {
#define VOIP_FRONTIER(name) do { std::fprintf(stderr, "[FRONTIER] BF2VoipServer::CBF2VoipServer::" name " reached\n"); std::fflush(stderr); std::abort(); } while (0)
CBF2VoipServer::CBF2VoipServer() {}
CBF2VoipServer::~CBF2VoipServer() {}
VOIP::VRESULT CBF2VoipServer::Initialize(bool) { VOIP_FRONTIER("Initialize"); }
VOIP::VRESULT CBF2VoipServer::CreateInstance(unsigned int, std::string, int&) { VOIP_FRONTIER("CreateInstance"); }
VOIP::VRESULT CBF2VoipServer::Update() { VOIP_FRONTIER("Update"); }
VOIP::VRESULT CBF2VoipServer::Shutdown() { VOIP_FRONTIER("Shutdown"); }
}
