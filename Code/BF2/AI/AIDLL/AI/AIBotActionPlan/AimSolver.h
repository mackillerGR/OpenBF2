// Ballistic aim solution shared by the aiming conditions and statements of the bot fire scripts:
// given a target's relative position and the velocities of target and shooter, it searches the
// launch pitch whose trajectory (with drag and gravity) meets the predicted target position.
// [INFERRED] class name and source placement; the PE object is 0x70 bytes, embedded in its users.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_AIMSOLVER_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_AIMSOLVER_H

#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class AimSolver
{
public:
    // Level aim along +x with unit speed; no drag, no gravity, no target. PE 101DF8D0.
    AimSolver();

    // Sets the drag coefficient and its inverse (0 for no drag). PE 101B2090.
    void setDrag(float drag);
    void setGravity(float gravity) { m_gravity = gravity; }
    void setSpeed(float speed) { m_speed = speed; }
    // Thrown weapons predict over a fixed time; the aiming wrapper sets both flags each tick.
    // [INFERRED] names.
    void setFixedTime(bool fixed) { m_fixedTime = fixed; }
    void setNoCorrection(bool off) { m_noCorrection = off; }

    // Records the target: relative position, target velocity, shooter velocity, the time they hold
    // for, and an optional frame whose rotation turns the up vector. PE 101DF960; always true.
    bool setTarget(const Vec3& relative, const Vec3& velocity, const Vec3& ownVelocity,
                   const Mat4* frame, double time);

    // Searches the pitch for a shot at `time`. `effort` scales the iteration count (8 * effort + 2);
    // `lob` starts high for indirect fire. Writes the unit aim direction, uses `work` as scratch for
    // the predicted target position, and stores the time of impact when `impactTime` is given.
    // False when no usable solution exists. PE 101DFFE0.
    bool solve(double time, float effort, bool lob, Vec3& direction, Vec3& work,
               double* impactTime);

    // Where a projectile fired along `forward` (at the set speed, plus the shooter's velocity) is
    // `time` seconds later, relative to the muzzle. False beyond 100 seconds. PE 101DFF70;
    // [INFERRED] name.
    bool projectilePosition(const Vec3& forward, float time, Vec3& out) const;
    // The high-arc direction to the target without drag, for the artillery simulation; `work`
    // receives the relative target. False when the speed cannot reach it. PE 101DFD40;
    // [INFERRED] name.
    bool lobDirection(Vec3& direction, Vec3& work) const;

private:
    // The target position relative to the shooter at `time`. PE 101DFEC0.
    void predict(double time, Vec3& out) const;
    // Flight time over a horizontal distance with drag. PE 101DFA00.
    float flightTime(float distance, float speed) const;
    // The displacement of a projectile launched with `velocity` after `time`. PE 101DFB00.
    void trajectory(Vec3 velocity, float time, Vec3& out) const;

    /* +0x00 */ float  m_drag;
    /* +0x04 */ float  m_invDrag;              // not initialised by the constructor
    /* +0x08 */ float  m_gravity;
    /* +0x0C */ float  m_speed;                // 0 means an instant hit
    /* +0x10 */ double m_time;
    /* +0x18 */ Vec3   m_relative;
    /* +0x24 */ Vec3   m_direction;            // m_relative normalised
    /* +0x30 */ Vec3   m_velocity;
    /* +0x3C */ Vec3   m_ownVelocity;
    /* +0x48 */ Vec3   m_up;
    /* +0x54 */ int    m_targetSet;
    /* +0x58 */ double m_halfFlightTime;       // not initialised by the constructor
    /* +0x60 */ float  m_pitch;                // 666 until a solve has run
    /* +0x64 */ float  m_pitchStep;
    /* +0x68 */ bool   m_fixedTime;            // the aiming wrapper: the weapon is thrown
    /* +0x69 */ bool   m_noCorrection;         // the aiming wrapper: a reported splash target
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_AIMSOLVER_H
