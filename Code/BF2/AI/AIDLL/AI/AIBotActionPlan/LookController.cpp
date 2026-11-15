#include "LookController.h"

#include <cmath>

#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Writable data in the shipped image. [INFERRED] names.
float s_minimumOutput = 0.01f;            // PE 102AE258
float s_climbScale = 0.05f;               // PE 102AE25C
float s_unskilledPitchLimit = 2.0f;       // PE 102AE260
float s_unskilledYawLimit = 1.0f;         // PE 102AE264
float s_movingSoldierYawLimit = 1.0f;     // PE 102AE268
float s_stillSoldierYawLimit = 1.5f;      // PE 102AE26C
float s_stillSoldierYawGain = -3.0f;      // PE 102AE270
bool  s_useDeadZone = false;              // PE 102C062D

// PE 1006ED90: clamps to [low, high]; an unordered value yields low.
float clampToRange(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// The inlined pre-clamp before acos.
float clampCosine(float cosine)
{
    if (!(-1.0f <= cosine))
        return -1.0f;
    if (!(1.0f >= cosine))
        return 1.0f;
    return cosine;
}

// PE 100DE7B0 and its inlined copies: 1, -1 or 0.
float sign(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (!(value >= 0.0f))
        return -1.0f;
    return 0.0f;
}

const float kHalfPi = 1.5707963705062866f;
const float kThreeHalfPi = 4.71238899230957f;

// Shared tail of both axes: dead zone, saturation at four, a minimum magnitude, then the limit.
float finishOutput(float output, float error, float deadZone)
{
    if (s_useDeadZone && std::fabs(error) < deadZone)
        return 0.0f;
    if (std::fabs(output) > 4.0f)
        return sign(output) * 4.0f;
    return output;
}

} // anonymous namespace

// PE 101C8010.
void steerLook(Bot* bot, const Vec3& direction, const Mat4& camera,
               float& pitchError, float previousPitch, float& yawError, float previousYaw,
               float deadZone, IPIControlInfo* control, io::PlayerInput* input,
               float climb, float yawSpeed, float pitchSpeed)
{
    const float up = clampCosine(camera.m[6] * direction.z + camera.m[5] * direction.y +
                                 camera.m[4] * direction.x);
    const float right = clampCosine(camera.m[2] * direction.z + camera.m[1] * direction.y +
                                    direction.x * camera.m[0]);
    const float forward = clampCosine(camera.m[10] * direction.z + camera.m[9] * direction.y +
                                      direction.x * camera.m[8]);

    pitchError = -(std::acos(clampToRange(clampCosine(up), -1.0f, 1.0f)) - kHalfPi);
    const float across = std::acos(clampToRange(clampCosine(right), -1.0f, 1.0f));
    yawError = across;
    if (!(forward < 0.0f))
        yawError = across - kHalfPi;
    else if (!(right < 0.0f))
        yawError = -(across + kHalfPi);
    else
        yawError = kThreeHalfPi - across;

    const float proportional = control->getProportionalConstant();
    float yawGain = proportional;
    const float derivative = control->getDerivativeConstant();
    const float angleSpeed = control->getAngleSpeedConstant();
    float pitchLimit = 4.0f;
    float yawLimit = 4.0f;

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
    if (equipment && ((equipment->getObjectTypeFlags() >> 21) & 1))
    {
        const unsigned long long mapped = input->mappingFlags;
        bool adjust = true;
        if (static_cast<BotMain*>(bot)->getFiringTarget() == static_cast<unsigned int>(-1))
        {
            const bool throttle = (mapped & (1ULL << io::PIThrottle)) && input->inputs[io::PIThrottle] != 0.0f;
            const bool sprint = (mapped & (1ULL << io::PISprint)) && input->inputs[io::PISprint] != 0.0f;
            const bool yaw = (mapped & (1ULL << io::PIYaw)) && input->inputs[io::PIYaw] != 0.0f;
            if (!throttle && !sprint && !yaw)
            {
                yawGain = s_stillSoldierYawGain;
                yawLimit = s_stillSoldierYawLimit;
                adjust = false;
            }
        }
        if (adjust && !((mapped & (1ULL << io::PISprint)) && input->inputs[io::PISprint] != 0.0f))
            yawLimit = (4.0f - s_movingSoldierYawLimit) * skill + s_movingSoldierYawLimit;
    }
    else
    {
        yawLimit = (4.0f - s_unskilledYawLimit) * skill + s_unskilledYawLimit;
        pitchLimit = (4.0f - s_unskilledPitchLimit) * skill + s_unskilledPitchLimit;
    }

    // Pitch.
    {
        const float error = pitchError;
        const float change = error - previousPitch;
        float speedTerm = angleSpeed * pitchSpeed;
        if (sign(error) == sign(pitchSpeed))
            speedTerm *= 0.5f;
        float output = proportional * error + derivative * change + speedTerm;
        float climbBias = 0.0f;
        if (!(climb >= -9.999999747378752e-05f))
        {
            climbBias = -climb / s_climbScale;
            if (!(climbBias < 0.75f))
                climbBias = 0.75f;
        }
        output += climbBias;
        output = finishOutput(output, error, deadZone);
        output = (1.0f - skill * 0.5f) * output * 0.5f;
        if (std::fabs(output) < s_minimumOutput && output != 0.0f)
            output = sign(output) * s_minimumOutput;
        output = clampToRange(output, -pitchLimit, pitchLimit);
        input->setInput(control->getLookVerticalControl(), output);
    }

    // Yaw.
    {
        const float error = yawError;
        const float change = error - previousYaw;
        float speedTerm = angleSpeed * yawSpeed;
        if (sign(error) == sign(yawSpeed))
            speedTerm *= 0.5f;
        float output = derivative * change + yawGain * error + speedTerm;
        output = finishOutput(output, error, deadZone);
        if (std::fabs(output) < s_minimumOutput && output != 0.0f)
            output = sign(output) * s_minimumOutput;
        output = clampToRange(output, -yawLimit, yawLimit);
        input->setInput(control->getLookHorizontalControl(), output);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
