// Per-bot radio message queue and timing state.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H

#include <list>
#include <map>
#include <queue>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

class BotMain;
class CommunicationHub;
class Information;

// Both opaque, and declared this way elsewhere in the tree too - see BFConsole.h for why no
// enumerator name is recoverable from either binary.
enum AIRadioChannelType : int;
enum AIRadioMessageType : int;

// Radio-message record. [UNRESOLVED] Some fields are not yet identified.
struct AIRadioMessageInfo
{
    // PE 101461C0: weight 1, no sender, time 0, unhandled, type 18.
    AIRadioMessageInfo()
        : m_weight(1.0f), m_senderPlayerId(-1), m_time(0.0), m_handled(false), m_flag17(false),
          m_type(static_cast<AIRadioMessageType>(0x12)) {}

    /*  0 */ float m_weight;               // [UNRESOLVED] meaning; BBIdle scales its urgency by it
    /*  4 */ int m_senderPlayerId;
    /*  8 */ double m_time;                // when it was received; inboxes are newest first
    // Set when a behaviour answers the message. [INFERRED] name.
    /* 16 */ bool m_handled;
    /* 17 */ bool m_flag17;                // [UNRESOLVED] a set flag also blocks answering
    /* 18 */ unsigned char m_pad18[2];
    /* 20 */ int m_type;                   // [INFERRED] the message type; indexes the send periods
};

class CommunicationAgent
{
public:
    // Reports a newly contacted enemy for the bot's side. PE 10146300; [INFERRED] name.
    void reportContact(Information* information);
    // Queued priority message. The integer at +8 determines ordering.
    struct CAPrioMessage
    {
        // The whole of the ordering. A `std::priority_queue` with `std::less` is a MAX-heap, so
        // the largest value here is what the bot says next.
        // The comparison is unsigned in the shipped heap code (PE 10147570).
        bool operator<(const CAPrioMessage& rhs) const { return m_priority < rhs.m_priority; }

        /*  0 */ int           m_type;       // AIRadioMessageType
        /*  4 */ int           m_channel;    // [INFERRED] passed on as the AIRadioChannelType
        /*  8 */ unsigned int  m_priority;
        /* 12 */ int           m_flag;       // [UNRESOLVED] passed on as the final byte
        /* 16 */ double        m_sendTime;   // enqueue time plus the delay
        /* 24 */ float         m_delay;
        /* 28 */ unsigned char m_pad28[4];   // PE size is 32: the heap code shifts by 5
    };

    explicit CommunicationAgent(BotMain* owner);
    // The implicit destructor releases member containers; no custom cleanup is required.

    void reset();
    // Expires old inbox entries and, at most once a second, sends whatever is due.
    void update();

    // Queues a radio message of one type unless that type is still in its send period or
    // already queued. The message goes out once `delay` seconds have passed.
    void enqueueRadioMessage(int type, int channel, unsigned int priority, float delay, int flag);
    // Sends a message at once, stamped with the current time. PE 10146F10; [INFERRED] name.
    void sendRadioMessageNow(int type, int channel, unsigned int priority, int flag);
    // Asks a friendly unit blocking the bot's vehicle to move: a vehicle with type flag 26 honks
    // (input 8) instead of radioing. At most once per interval (shorter for an AI blocker); a
    // soldier blocker's own agent records the time. PE 10147710; [INFERRED] name.
    void warnBlocker(Information* blocker);

    // attested by a 7-byte body: `return m_receivedMessages.data + type;`
    const std::list<AIRadioMessageInfo>& getReceivedMessages(AIRadioMessageType type) const
    { return m_receivedMessages[type]; }
    // Marks every inbox message of the type that is neither handled nor flagged; false when there
    // was none. PE 101481B0; [INFERRED] name.
    bool handleMessages(AIRadioMessageType type);
    // The same for the messages of the type from one sender. PE 10148250; [INFERRED] name.
    bool handleMessages(AIRadioMessageType type, int sender);
    // Marks the inbox copy of the message (same type, sender and time) as handled unless it
    // already is or is flagged; false when that fails. PE 10148200; [INFERRED] name.
    bool handleMessage(const AIRadioMessageInfo& message);
    // Copies the inbox message of the type from the sender; false when there is none.
    // PE 10148150; [INFERRED] name.
    bool findMessage(AIRadioMessageInfo& out, AIRadioMessageType type, int sender);
    // Whether the inbox holds a message of the type from the sender, or any when the sender is
    // -1. PE 101482B0; [INFERRED] name.
    bool hasMessage(AIRadioMessageType type, int sender);
    // [UNRESOLVED] meaning; reset stores -1. BBIdle compares a requester's equipment player id
    // against it. [INFERRED] accessor; read inline.
    int getInt92() const { return m_int92; }
    // BBPChange stores the player id of the vehicle's occupant here (255 when no bot occupies it).
    // [INFERRED] accessor; written inline at PE +0x70.
    void setInt92(int value) { m_int92 = value; }

private:
    void expireReceivedMessages(double time);
    void updateOutgoing(double time);
    void requestSupport(Information* equipment);
    void releaseFire(Information* equipment);
    void sendRadioMessage(const CAPrioMessage& message);
    void popQueuedMessage();

    typedef std::priority_queue<CAPrioMessage, std::vector<CAPrioMessage>,
                                std::less<CAPrioMessage> > MessageQueue;

    /*   0 */ BotMain* m_bot;                  // the ctor argument, and the first store
    // the constructor never sets this - `reset` does, and the constructor's last statement is a
    // call to `reset`. Same shape as `CommunicationHub`'s own two uninitialised timestamps.
    /*   4 */ CommunicationHub* m_hub;         // = CommunicationHub::m_instance
    // one inbox per message type - eighteen lists, indexed by the enum. See the header block.
    /*   8 */ std::vector<std::list<AIRadioMessageInfo> > m_receivedMessages;
    // The types now in the queue, with their send times; a type is queued at most once.
    /*  20 */ std::map<int, double> m_queuedTypes;
    // what the bot says next. See the header block.
    /*  32 */ MessageQueue m_queue;            // +32..+47: a 12-byte vector and an empty comparator
    // Eighteen doubles, one per type: the time before which that type may not be queued again
    // (enqueue time plus its max send period), or zero once that time has passed.
    /*  48 */ std::vector<double> m_nextSendTime;
    /*  60 */ double m_lastUpdateTime;         // reset: 0.0; update's once-a-second gate
    // the two cached columns of `AIRadioMessages.ai`. Filled once, in the constructor.
    /*  68 */ std::vector<float> m_receiveTimeToLive;
    /*  80 */ std::vector<float> m_maxSendPeriod;
    /*  92 */ int m_int92;                     // reset: -1, this subsystem's null-index convention
    /*  96 */ double m_double96;               // reset: 0.0; PE +0x78, read by BBAvoid
public:
    double getDouble96() const { return m_double96; }   // [INFERRED] accessor
private:
    /* 104 */ double m_double104;              // reset: 0.0; releaseFire waits two seconds past it
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_COMMUNICATIONAGENT_H
