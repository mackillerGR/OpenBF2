#include "AimSolver.h"

#include <cmath>

#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

const float kUnsetPitch = 666.0f;
const float kMaxPitch = 2.9845130443573f;           // PE 102AE7C0
const float kMaxPitchStep = 0.017f;                 // PE 10285508
const float kFixedPredictionTime = 2.8f;            // PE 102AE788
const float kHitscanLift = 20.0f;
const float kDragFastSpeed = 200.0f;
const float kMaxProjectileTime = 100.0f;           // PE 1027E200
const float kLobFour = 4.0f;                        // PE 1027B210

// PE 1006EEC0, the conditional Vec3 normaliser (file-local copies live in other TUs too).
int normalise(Vec3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return 0;
    if (!(std::fabs(lengthSquared) > 1.4210854715202004e-14f))
    {
        value.x = 0.0f;
        value.y = 0.0f;
        value.z = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
    return 0;
}

// PE 100DE7B0.
float sign(float value)
{
    if (value > 0.0f)
        return 1.0f;
    if (value < 0.0f)
        return -1.0f;
    return 0.0f;
}

double timeIncrease()
{
    return static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
}

} // anonymous namespace

AimSolver::AimSolver()
{
    m_relative.x = 1.0f;
    m_relative.y = 0.0f;
    m_relative.z = 0.0f;
    m_velocity.x = m_velocity.y = m_velocity.z = 0.0f;
    m_ownVelocity.x = m_ownVelocity.y = m_ownVelocity.z = 0.0f;
    m_time = 0.0;
    m_drag = 0.0f;
    m_gravity = 0.0f;
    m_targetSet = 0;
    m_speed = 1.0f;
    m_direction.x = 1.0f;
    m_direction.y = 0.0f;
    m_direction.z = 0.0f;
    m_up.x = 0.0f;
    m_up.y = 0.0f;
    m_up.z = 1.0f;
    m_pitch = kUnsetPitch;
    m_pitchStep = 0.0174533f;
    m_fixedTime = false;
    m_noCorrection = false;
}

void AimSolver::setDrag(float drag)
{
    m_drag = drag;
    if (drag != 0.0f)
        m_invDrag = 1.0f / drag;
    else
        m_invDrag = 0.0f;
}

bool AimSolver::setTarget(const Vec3& relative, const Vec3& velocity, const Vec3& ownVelocity,
                          const Mat4* frame, double time)
{
    m_targetSet = 1;
    m_relative = relative;
    m_direction = relative;
    normalise(m_direction);
    m_velocity = velocity;
    m_ownVelocity = ownVelocity;
    m_time = time;
    m_up.x = 0.0f;
    m_up.y = 0.0f;
    m_up.z = 1.0f;
    if (frame)
    {
        // The frame's rotation applied to the up vector (PE 1012F8C0).
        const float* const m = frame->m;
        const Vec3 up = m_up;
        m_up.x = up.z * m[8] + up.y * m[4] + up.x * m[0];
        m_up.y = up.z * m[9] + up.x * m[1] + up.y * m[5];
        m_up.z = up.z * m[10] + up.x * m[2] + up.y * m[6];
    }
    return true;
}

void AimSolver::predict(double time, Vec3& out) const
{
    double elapsed = time - m_time;
    if (m_fixedTime)
        elapsed = kFixedPredictionTime;
    const float y = static_cast<float>(elapsed * m_velocity.y);
    const float z = static_cast<float>(elapsed * m_velocity.z);
    const float px = static_cast<float>(elapsed * m_velocity.x + m_relative.x);
    const float py = y + m_relative.y;
    const float pz = z + m_relative.z;
    out.x = px;
    out.y = py;
    out.z = pz;
    if (m_fixedTime && !m_noCorrection)
    {
        out.x = px - m_relative.x * 0.1f;
        out.y = py - m_relative.y * 0.1f;
        out.z = pz - m_relative.z * 0.1f;
    }
}

float AimSolver::flightTime(float distance, float speed) const
{
    float current = speed;
    float time = 0.0f;
    const bool fast = speed > kDragFastSpeed;
    const float step = static_cast<float>(timeIncrease());
    if (!(distance > 0.0f))
        return time;
    do
    {
        float loss = step * m_drag * current;
        if (fast)
            loss *= current;
        float next = current - loss;
        if (next < 0.0f)
            next = 0.0f;
        const float covered = (current + next) * step * 0.5f;
        if (!(covered > 0.0f))
            return 1000.0f;
        if (1000.0f * covered < distance)
            return 1000.0f;
        if (distance < covered)
            time += distance / covered * step;
        else
            time += step;
        distance -= covered;
        current = next;
    }
    while (distance > 0.0f);
    return time;
}

void AimSolver::trajectory(Vec3 velocity, float time, Vec3& out) const
{
    out.x = 0.0f;
    out.y = 0.0f;
    out.z = 0.0f;
    const float speed = std::sqrt(velocity.z * velocity.z + velocity.y * velocity.y +
                                  velocity.x * velocity.x);
    if (speed == 0.0f)
        return;
    const bool fast = speed > kDragFastSpeed;
    float step = static_cast<float>(timeIncrease());
    if (!(time > 0.0f))
        return;
    do
    {
        if (time < step)
            step = time;
        float dragX = 0.0f;
        float dragY = 0.0f;
        float dragZ = 0.0f;
        if (m_drag != 0.0f)
        {
            if (fast)
            {
                const float magnitude = std::sqrt(velocity.z * velocity.z + velocity.y * velocity.y +
                                                  velocity.x * velocity.x);
                dragX = velocity.x * magnitude * m_drag * step;
                dragY = velocity.y * magnitude * m_drag * step;
                dragZ = velocity.z * magnitude * m_drag * step;
            }
            else
            {
                dragX = velocity.x * m_drag * step;
                dragY = velocity.y * m_drag * step;
                dragZ = velocity.z * m_drag * step;
            }
        }
        const float nextX = velocity.x - dragX + 0.0f * step;
        const float nextY = velocity.y - dragY + m_gravity * step;
        const float nextZ = velocity.z - dragZ + 0.0f;
        const float halfX = (nextX + velocity.x) * 0.5f;
        const float halfY = (nextY + velocity.y) * 0.5f;
        const float halfZ = (nextZ + velocity.z) * 0.5f;
        out.x += halfX * step;
        out.y += halfY * step;
        out.z += halfZ * step;
        velocity.x = nextX;
        velocity.y = nextY;
        velocity.z = nextZ;
        time -= step;
    }
    while (time > 0.0f);
}

// The pitch search raises the aim while the projectile passes below the target and lowers it while
// it passes above; the first reversal starts halving the step, after which the search runs its
// iteration budget again. An unconverged search doubles the stored step (capped) for next time and
// fails unless the step was already small, in which case the last pitch is still used.
bool AimSolver::solve(double time, float effort, bool lob, Vec3& direction, Vec3& work,
                      double* impactTime)
{
    const bool hitscan = m_speed == 0.0f;
    float relY = m_relative.y;
    if (hitscan)
        relY += kHitscanLift;
    const float distance = std::sqrt(relY * relY + m_relative.x * m_relative.x +
                                     m_relative.z * m_relative.z);
    const float closing = m_direction.z * m_velocity.z + m_direction.y * m_velocity.y +
                          m_velocity.x * m_direction.x;
    const float approach = m_speed - closing + m_direction.z * m_ownVelocity.z +
                           m_direction.y * m_ownVelocity.y + m_direction.x * m_ownVelocity.x;
    if (approach < 1.0f)
        return false;
    const float guess = distance / approach;
    if (guess < 0.0f)
        return false;
    const double firstImpact = guess + time;
    double impact = firstImpact;
    predict(firstImpact, work);
    float horizontal = std::sqrt(work.z * work.z + work.x * work.x);
    if (horizontal == 0.0f)
        return false;
    float height = work.y;
    if (hitscan)
        height += kHitscanLift;

    float pitch;
    if (lob && m_pitch < 0.7853981852531433f)
        pitch = 1.1780972f;
    else if (!hitscan && m_pitch != kUnsetPitch)
        pitch = m_pitch;
    else
        pitch = std::atan2(height, horizontal);

    float step = std::fabs(m_pitchStep);
    if (step < 1e-4f)
        step = 1e-4f;
    step = sign(m_pitchStep) * step;
    m_pitchStep = step;
    if (lob && !hitscan)
    {
        pitch = (pitch + 1.5079644918441772f) * 0.5f;
        if (step > 0.0f)
            m_pitchStep = step * -1.0f;
    }

    bool reversed = false;
    int lastMove = 0;
    const bool hasDrag = m_drag != 0.0f;
    // Truncated to int, then compared unsigned, as shipped.
    const unsigned int iterations =
        static_cast<unsigned int>(static_cast<int>(effort * 8.0f + 2.0f));
    unsigned int iteration = 0;
    unsigned int afterReversal = 0;
    bool converged = false;
    while (iterations != 0)
    {
        Vec3 flat;
        flat.x = work.x;
        flat.y = 0.0f;
        flat.z = work.z;
        normalise(flat);
        float ownAlong = flat.z * m_ownVelocity.z + flat.y * m_ownVelocity.y +
                         flat.x * m_ownVelocity.x;
        float forward = std::cos(pitch) * m_speed + ownAlong;
        float upward = std::sin(pitch) * m_speed + m_ownVelocity.y;
        if (!(forward > 0.1f))
            return false;
        float flight = hasDrag ? flightTime(horizontal, forward) : horizontal / forward;
        if (flight > 60.0f)
        {
            pitch = std::atan2(height, horizontal);
            m_pitch = pitch;
            ownAlong = flat.z * m_ownVelocity.z + flat.y * m_ownVelocity.y +
                       flat.x * m_ownVelocity.x;
            forward = std::cos(pitch) * m_speed + ownAlong;
            upward = std::sin(pitch) * m_speed + m_ownVelocity.y;
            predict(firstImpact, work);
            horizontal = std::sqrt(work.z * work.z + work.x * work.x);
            flight = hasDrag ? flightTime(horizontal, forward) : horizontal / forward;
        }
        Vec3 drop;
        drop.x = drop.y = drop.z = 0.0f;
        if (flight > 100.0f)
            return false;
        Vec3 launch;
        launch.x = forward;
        launch.y = upward;
        launch.z = 0.0f;
        trajectory(launch, flight, drop);
        m_halfFlightTime = flight * 0.5f;
        impact = flight + time;
        predict(impact, work);
        height = work.y;
        horizontal = std::sqrt(work.z * work.z + work.x * work.x);
        if (hitscan)
            height += kHitscanLift;
        if (m_fixedTime && !m_noCorrection)
            height -= 1.3f;
        if (!std::isfinite(horizontal) || !std::isfinite(height))
            return false;
        if (hitscan)
        {
            converged = true;
            break;
        }
        if (iteration == iterations - 1)
            break;
        if (drop.y < height)
        {
            if (!(pitch < kMaxPitch))
                return false;
            if (lastMove == 0)
                lastMove = 1;
            else if (lastMove < 0)
                reversed = true;
            if (reversed)
                m_pitchStep *= 0.5f;
            pitch += m_pitchStep;
            if (pitch > kMaxPitch)
                pitch = kMaxPitch;
        }
        else
        {
            const float minPitch = -kMaxPitch;
            if (!(pitch > minPitch))
                return false;
            if (lastMove == 0)
                lastMove = -1;
            else if (lastMove > 0)
                reversed = true;
            if (reversed)
                m_pitchStep *= 0.5f;
            pitch -= m_pitchStep;
            if (pitch < minPitch)
                pitch = minPitch;
        }
        ++iteration;
        if (iteration > 0x400)
            break;
        afterReversal += reversed ? 1 : 0;
        if (!(afterReversal < iterations))
            break;
    }
    if (reversed)
        converged = true;

    m_pitch = pitch;
    if (!converged)
    {
        const bool small = std::fabs(m_pitchStep) < kMaxPitchStep;
        const float doubled = m_pitchStep + m_pitchStep;
        m_pitchStep = kMaxPitchStep < doubled ? kMaxPitchStep : doubled;
        if (!small)
            return false;
    }

    const float scale = std::cos(pitch) / horizontal;
    const float dx = scale * work.x;
    const float dy = std::sin(pitch);
    const float dz = scale * work.z;
    direction.x = dx;
    direction.y = dy;
    direction.z = dz;
    const float length = std::sqrt(dx * dx + dz * dz + dy * dy);
    if (length < 0.95f)
        return false;
    const float inverse = 1.0f / length;
    direction.x = dx * inverse;
    direction.y = dy * inverse;
    direction.z = dz * inverse;
    if (impactTime)
        *impactTime = impact;
    return true;
}

bool AimSolver::projectilePosition(const Vec3& forward, float time, Vec3& out) const
{
    if (!(time <= kMaxProjectileTime))
        return false;
    const float speed = m_speed;
    const float z = speed * forward.z;
    const float y = speed * forward.y;
    const float x = speed * forward.x;
    const Vec3 velocity = {x + m_ownVelocity.x, y + m_ownVelocity.y, z + m_ownVelocity.z};
    trajectory(velocity, time, out);
    return true;
}

bool AimSolver::lobDirection(Vec3& direction, Vec3& work) const
{
    work = m_relative;
    const float speed = m_speed;
    const float horizontal = std::sqrt(m_relative.z * m_relative.z + m_relative.x * m_relative.x);
    const float k = -m_gravity * horizontal * horizontal / (speed * speed + speed * speed);
    const float discriminant = horizontal * horizontal - (k + m_relative.y) * k * kLobFour;
    if (!(discriminant >= 0.0f))
        return false;
    const float root = std::sqrt(discriminant);
    const float inverse = 1.0f / (k + k);
    const float high = std::atan2((root + horizontal) * inverse, 1.0f);
    const float low = std::atan2((horizontal - root) * inverse, 1.0f);
    const float angle = high < low ? low : high;
    const float scale = std::cos(angle) / horizontal;
    direction.x = scale * m_relative.x;
    direction.y = std::sin(angle);
    direction.z = scale * m_relative.z;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
