// Payloads of the category-3 AI events that the game posts and AIEventListener::handleEvent reads.
// [CHOSEN, not recovered] Shared declarations: retail builds each payload on the producer's stack
// and the listener reads it by offset. They are process-local event data, never wire records;
// object fields are addresses, so the x86 layouts below hold only for 32-bit builds.

#ifndef AI_AIINTERFACE_MAIN_AIEVENTPAYLOADS_H
#define AI_AIINTERFACE_MAIN_AIEVENTPAYLOADS_H

namespace dice {
namespace hfe {
namespace ai {

// Event 0, player killed: the player's team and id and the AI object of the vehicle the player
// was in, which may be null.
struct AIEventPlayerKilledPayload
{
    int team;
    int playerId;
    void* vehicleAiObject;
};

// Events 1 (entered) and 2 (exited). For an entry, `object` is the AI object of the seat or body
// entered and `other` that of the object the player left, or null. For an exit from a seat,
// `object` is the occupied object and `other` the seat. `specialContainer` marks ladders,
// parachutes, ziplines and grappling-hook ropes, which may carry no AI object.
struct AIEventOccupancyPayload
{
    void* object;
    void* other;
    int team;
    int playerId;
    bool specialContainer;
};

// Events 6 (man down) and 7 (revived): the soldier's AI object and the occupying player's id.
struct AIEventSoldierStatePayload
{
    void* soldierAiObject;
    int playerId;
};

// Player exit notification from ServerGameLogic: the player's id and the vehicle's AI object.
struct AIEventPlayerExitPayload
{
    int playerId;
    void* aiObject;
};

#if !defined(_WIN64) && !defined(__LP64__)
static_assert(sizeof(AIEventPlayerKilledPayload) == 12, "retail AI kill payload");
static_assert(sizeof(AIEventOccupancyPayload) == 20, "retail AI occupancy payload");
static_assert(sizeof(AIEventSoldierStatePayload) == 8, "retail AI soldier-state payload");
static_assert(sizeof(AIEventPlayerExitPayload) == 8, "retail AI exit payload");
#endif

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_MAIN_AIEVENTPAYLOADS_H
