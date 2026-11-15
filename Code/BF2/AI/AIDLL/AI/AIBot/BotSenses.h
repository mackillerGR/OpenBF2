// Per-bot sight, hearing and communication state. Owns a view frustum and sensed-object records.

#ifndef AI_AIDLL_AI_AIBOT_BOTSENSES_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_BOTSENSES_H

#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "../../../../Math/Frustum.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class IAIObject;
class Information;
// The two things a bot's perception owns and deletes. Both are [FRONTIER] classes - `reset` and
// the destructor delete them through an inlined, non-virtual destructor, so neither can be written
// until the class itself is recovered. See the reset block in the .cpp.
// Minimal views of the two record types: only the members the sensing agent reads are declared,
// and their bodies stay frontiers until the records themselves are recovered.
// One shot fired at the bot. Offsets are the retail PE layout; names are [INFERRED]. The
// constructor is not recovered: nothing in this tree creates one yet (BotMain::event_projectile
// is a frontier).
class FireObject
{
public:
    // Releases the strength array. PE 1017E3B0.
    ~FireObject() { delete[] m_strengths; }
    const Vec3& getPosition() const { return m_position; }
    double getTime() const { return m_time; }
    // The strength of one kind, decaying with the time since the shot. PE 1017E3C0.
    float getStrength(int kind) const;

    /* +0x00 */ bool   m_heavy;           // counts ten times in the attacker strength
    /* +0x04 */ int*   m_strengths;       // owned, new[]
    /* +0x14 */ Vec3   m_position;
    /* +0x20 */ double m_time;
    /* +0x28 */ float  m_decay;
};
// What a bot knows about one sensed object. Offsets are the retail PE layout; field names are
// [INFERRED]. Construction: PE 101E1510, 0x70 bytes.
class SensedObject
{
public:
    // `direct`: sensed by this bot itself rather than reported; `sided`: the object has a side.
    SensedObject(unsigned int handle, bool direct, bool sided);

    unsigned int getHandle() const { return m_handle; }
    unsigned int getSenseFlags() const { return m_senseFlags; }

    /* +0x00 */ unsigned int m_handle;
    /* +0x04 */ Vec3   m_senseNormal;       // the normal of the last sensed point
    /* +0x10 */ unsigned int m_part;        // the soldier part sensed, -1 for none
    /* +0x18 */ double m_firstSensed;
    /* +0x20 */ Vec3   m_position;
    /* +0x2C */ Vec3   m_lastPosition;
    /* +0x38 */ double m_lastSeen;
    /* +0x40 */ double m_double40;          // = -1000.0 [UNRESOLVED]
    // Bit 1 direct, 2 seen, 4 sided, 8 reported only, 0x10 not direct, 0x40 seen through cover.
    /* +0x50 */ unsigned int m_senseFlags;
    /* +0x54 */ unsigned int m_lastSense;   // the flag of the last sense
    /* +0x58 */ double m_lastUpdate;        // = 0.0
    /* +0x60 */ double m_positionTime;      // = 0.0
    /* +0x68 */ bool   m_contactAdded;      // = false
};

class BotSenses
{
public:
    // (`operator new(0x94)` in SensingAgent's constructor).
    explicit BotSenses(BotMain* owner);
    ~BotSenses();
    void reset();                            // - (two arms frontier)

    // Sight in four steps spread over ticks: gather the player-occupied objects in view or close,
    // sort them by distance, trace lines of sight within the slice, then wait for the next
    // round. PE 101BDDC0.
    void see(const double& slice);
    // Records a sighting of `handle` for this bot. `direct` is false when another bot in the
    // same vehicle saw it; `clear` is the line-of-sight trace's flag. [INFERRED] name.
    // PE 101BD8F0.
    void addSeen(unsigned int handle, const Vec3& normal, unsigned int part, Information* info,
                 bool direct, bool clear);
    // Hearing, incoming fire and seen-record upkeep within the slice. The first step gets a
    // tenth of it. Reached through BotMain::updateSensing. PE 101BFA10; [INFERRED] name.
    void updateSensing(const double& slice);

    // attested: an 11-byte body that returns `this + 112`.
    const std::map<unsigned int, SensedObject*>& getSensedObjects() const { return m_sensedObjects; }
    // PE 101BB7E0: the record for a handle, or null.
    SensedObject* getSensedObject(unsigned int handle) const;
    // The incoming-fire records with the handle of the shooter. [INFERRED] accessor; PE reads
    // +0x6C inline.
    const std::list<std::pair<FireObject*, unsigned int> >& getFireEntries() const { return m_list84; }

private:
    // True when the object is a primary object within 200 behind the equipment's primary
    // object. [INFERRED] name. PE 101B8AA0.
    bool isCloseBehind(Information* equipment, IAIObject* object);
    // The chance of hearing the object now: zero within 1.5 s of +172's time, for dead or
    // silent objects and outside the louder of its movement and recent-shot sound spheres;
    // otherwise interpolated over the distance. PE 101B8840; [INFERRED] name.
    float getHearingProbability(unsigned int handle, Information* info);
    // Re-hears the queued heard records until the deadline. PE 101BCEF0; [INFERRED] name.
    void updateHearing(const double& slice);
    // Sums the incoming-fire strengths and drops shots past their window. The argument is not
    // read. PE 101BC1F0; [INFERRED] name.
    void updateFire(const double& deadline);
    // Seen-record upkeep. PE 101BE980; [INFERRED] name.
    void updateSeen(const double& remaining);
    // The sense point on a bot soldier: its position, lowered for a prone pose. [INFERRED]
    // name. PE 101B8750.
    static void getBotSoldierSensePoint(Vec3& target, Vec3& offset, Information* info);
    // Traces again from the eye raised or lowered for the requested pose and records the
    // result against the target. Part of updateSeen; [CHOSEN] split.
    void probeRequestedPose(int requested, int current, const Vec3& eye, const Vec3& target,
                            const std::vector<IAIObject*>& ignore, bool targetIsBot,
                            float tolerance, unsigned int handle);

    typedef std::pair<double, SensedObject*>      TimedObject;   // a time and a thing
    typedef std::list<TimedObject>                TimedList;
    typedef std::pair<FireObject*, unsigned int>  FireEntry;
    typedef std::map<unsigned int, SensedObject*> SensedObjectMap;

    /*   0 */ BotMain* m_bot;                // the ctor argument, and the first store
    /*   4 */ Frustum* m_frustum;            // new(0x1E8) - . See the header block.
    // Equipment type bit 22, refreshed by see; selects the narrower view table. [INFERRED].
    /*   8 */ int m_narrowView;
    // When see last finished gathering. ELF +12, PE +0x10.
    /*  12 */ double m_double12;             // = -1000000.0 a fifth kind of sentinel
    // the distance sort buffer - what `InfoDistPred` and `partial_sort` operate on. Held as a
    // member rather than a local, so the seeing path does not reallocate every tick.
    /*  20 */ std::vector<std::pair<float, unsigned int> > m_sortBuffer;
    /*  32 */ int m_int32;                   // ctor: 0, reset: 0; see's view cycle, 0..2
    /*  36 */ int m_int36;                   // ctor: 0, reset: 0; see's step, 0..3
    /*  40 */ int m_int40;                   // ctor: 0, reset: 0; see's trace cursor
    // handle -> position in the list at +56. See the header block.
    /*  44 */ std::map<unsigned int, TimedList::iterator> m_map44;
    /*  56 */ TimedList m_list56;
    // A cursor into the list above, seeded to end by the constructor's last statement.
    /*  60 */ TimedList::iterator m_cursor60;
    /*  64 */ std::set<unsigned int> m_set64;
    /*  76 */ TimedList m_list76;
    // A cursor into the list above - and unlike +60 the constructor leaves it default-constructed.
    // Only `reset` makes it valid. See the header block.
    /*  80 */ TimedList::iterator m_cursor80;
    /*  84 */ std::list<FireEntry> m_list84;
    // Incoming-fire strengths summed by updateFire (PE +0x78, +0x7C, +0x80): all kinds, the
    // kind matching the equipment's strength type, and that kind from the latest shot of each
    // recent shooter. [INFERRED] names; zeroed as ints by the constructor and reset.
    /*  88 */ float m_attackerStrength;
    /*  92 */ float m_attackerStrengthOwnKind;
    /*  96 */ float m_recentAttackerStrength;
    /* 100 */ std::map<unsigned int, FireObject*> m_map100;
    /* 112 */ SensedObjectMap m_sensedObjects;
    // the only two containers `reset` does not empty. Element width 4 and trivially destructible
    // (the destructor deallocates without a per-element loop); the element type is [INFERRED].
    // see's gathered objects (PE +0x9C).
    /* 124 */ std::vector<IAIObject*> m_vector124;
    /* 136 */ std::vector<unsigned int> m_vector136;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_BOTSENSES_H
