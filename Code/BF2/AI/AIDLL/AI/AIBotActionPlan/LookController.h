// The soldier and turret look controller shared by the look entry and the input merger.
// [INFERRED] placement and name: the shipped helper (PE 101C8010) is a free function whose file
// is not attested.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_LOOKCONTROLLER_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_LOOKCONTROLLER_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace io { class PlayerInput; }
struct Mat4;
namespace ai {

class Bot;
class IPIControlInfo;

// Drives the look inputs toward `direction` with a proportional-derivative controller.
// Writes the pitch and yaw errors (radians) to `pitchError` and `yawError`; `previousPitch` and
// `previousYaw` are last tick's errors. A look error inside `deadZone` is ignored when the
// dead-zone switch is on. The last three values bias the pitch output and add the angle-speed
// terms; every shipped caller passes zero.
void steerLook(Bot* bot, const Vec3& direction, const Mat4& camera,
               float& pitchError, float previousPitch, float& yawError, float previousYaw,
               float deadZone, IPIControlInfo* control, io::PlayerInput* input,
               float climb, float yawSpeed, float pitchSpeed);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_LOOKCONTROLLER_H
