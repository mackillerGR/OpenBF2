// Owns and executes a bot's sensing plan.

#ifndef AI_AIDLL_AI_AIBOT_SENSINGAGENT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SENSINGAGENT_H

#include <vector>

#include "../../../../Math/Vec3.h"     // Vec2 and Vec3 live here, and both are typedefs

namespace dice {
namespace hfe {
namespace ai {

class BAPContext;
// the plan'S base class. `SensingAgent` holds one `BAPStatement*` and nothing else about the
// statement tree; see the deletePlan block in the .cpp for how far the evidence goes.
class BotScript;
class BotMain;
class BotSenses;
class Information;
class ISensingAgentPattern;
class SensedObject;

class SensingAgent
{
public:
    // The steered senses. [INFERRED] accessor; PE BotMain::getSensedObjects inlines the load.
    BotSenses* getSenses() const { return m_senses; }
    // Raised when another bot's sighting is shared with this one. [INFERRED] name; PE writes
    // +0x88 inline (BotSenses::addSeen).
    void setAlerted() { m_flag120 = true; }
    // The second parameter is in the ABI and is never read - see the header block.
    SensingAgent(BotMain* owner, unsigned char behaviourCount);
    ~SensingAgent();                            // [FRONTIER]

    // [UNRESOLVED] Methods requiring unknown nested-type layouts remain undeclared.
    void reset();
    // Called by `BotMain::reset` when a bot loses its active plan - losing a plan releases the
    // bot's aim override.
    void setOverride(bool);
    void deletePlan();
    void clearPlanTargetInformation();
    // PE 101AE210: the shipped body is empty.
    void insertBehaviourInformation(float, const Vec3&, unsigned char) {}
    // Sets the flag that makes setOverride refuse, and drops the override. [INFERRED] name.
    void setOverrideLock(bool locked);
    // Runs the agent's own plan through the bot's interpreter. [INFERRED] name.
    void executePlan();
    // Rebuilds the agent's look plan for the current equipment. [INFERRED] name.
    void updatePlan();
    bool hasPlan() const { return m_plan != 0; }
    // Clears the weight of the sectors the camera is looking at; looking at the focus sector of
    // the running plan marks it seen. PE 101B0890.
    void setQuadSeen();
    // [INFERRED] accessors; BAPMergeSoldierInputs reads these inline.
    const Vec2& getLookDirection() const { return m_lookDirection; }
    const Vec3& getFocusPoint() const { return m_focusPoint; }
    unsigned int getFocusObject() const { return m_focusObject; }
    // The two focus writes PE 1019B3C0 makes inline. [CHOSEN, not recovered] accessors.
    void setFocusPoint(const Vec3& point) { m_focusPoint = point; }
    void setFocusObject(unsigned int object)
    {
        m_focusObject = object;
        m_focusPoint.x = m_focusPoint.y = m_focusPoint.z = 0.0f;
    }
    // BotBehaviour::getPlan raises +120 when it replaces a bot's plan. [INFERRED] name.
    void setPlanReplaced() { m_flag120 = true; }
    // A thing worth looking at: its weight and where it is. Attested as a nested type by
    // `std::vector<SensingAgent::FocusInformation>`; member names are [INFERRED]. 16 bytes in PE.
    struct FocusInformation
    {
        float m_weight;
        Vec3  m_position;
    };
    // The summed attention of one of the eight sectors. [INFERRED] name. 20 bytes in PE.
    struct QuadrantInformation
    {
        float        m_weight;
        Vec3         m_position;   // the mean of the contributing positions
        unsigned int m_count;
    };
    // The chosen sector: index (-1 for none), weight and position. [INFERRED] name.
    struct FocusChoice
    {
        int   m_quadrant;
        float m_weight;
        Vec3  m_position;
    };

private:
    // [INFERRED] friendship: the patterns read and replace the plan and clear the context inline.
    friend class SensingAgentNullPattern;
    friend class SensingAgentSoldierPattern;

    void getQuadrantDirection(int quadrant, Vec2& direction);                  // PE 101AE0A0
    bool isInQuadrant(int quadrant, const Vec2& direction);                    // PE 101AE7E0
    bool isInQuadrantHalf(int quadrant, const Vec2& direction);                // PE 101AE8C0
    bool isDirectionAllowed(const Vec2& direction);                            // PE 101AE9A0
    bool getQuadrantsOf(const Vec3& position, int quadrants[2]);               // PE 101AEBD0
    bool isQuadrantAllowed(int quadrant);                                      // PE 101AF380
    // True when the sector lies within about ten degrees of the camera's forward direction.
    // [INFERRED] name. PE 101AECF0.
    bool isQuadrantInView(int quadrant);

    void updateReactionTime(const FocusChoice& focus, Information* equipment,
                            const Vec2& lookDirection);                        // PE 101AF410
    void createPlanFromFocus(ISensingAgentPattern* pattern, const FocusChoice& focus);   // PE 101AF5E0
    void chooseQuadrant(const std::vector<QuadrantInformation>& quadrants,
                        FocusChoice& focus);                                   // PE 101AFCE0
    void checkPlanValidity(const FocusChoice& focus, bool force);              // PE 101AFF00
    void clearFocus();                                                         // PE 101B0050
    void updateQuadrantWeights(Vec2& lookDirection, bool reconsider);          // PE 101B01B0
    void sumQuadrants(std::vector<QuadrantInformation>& quadrants);            // PE 101B0330
    void collectSensedFocus();                                                 // PE 101B10D0
    void collectFireFocus();                                                   // PE 101B1220
    void decideFocus(FocusChoice& focus, const double& time);                  // PE 101B1470
    void createPlanTowards(const Vec3& direction, ISensingAgentPattern* pattern);  // PE 101AE290

    /*   0 */ std::vector<FocusInformation> m_sensedFocus;   // PE +0x00
    /*  12 */ std::vector<FocusInformation> m_fireFocus;     // PE +0x10
    // neither is A gap - the constructor skips them and `reset` writes both. A field the
    // constructor misses is not necessarily unused; check the reset before calling it dead.
    /*  24 */ int m_int24;                 // reset: -1
    /*  28 */ float m_focusWeight;         // reset: 0 - the weight of the sector m_int24
    /*  32 */ Vec3 m_focusPosition;        // +32, +36, +40
    /*  44 */ bool m_flag44;               // reset: false
    // the eight-quadrant weight ring - the constructor's only container, and the only thing it
    // sizes. Eight zeroed floats; see the header block.
    /*  48 */ std::vector<float> m_quadrantWeights;
    // [INFERRED] Plan-target handle, cleared to zero and passed to BotMain when checking
    // plan invalidation; the neighboring words are also cleared.
    /*  60 */ unsigned int m_planTarget60;  // clearPlanTargetInformation: 0
    /*  64 */ Vec3 m_planTargetDirection;   // PE +0x4C
    // Read as a float against the focus weight (PE +0x58). [INFERRED] name.
    /*  76 */ float m_planTargetWeight;    // clearPlanTargetInformation: 0
    // The look direction the plan last aimed along (PE +0x5C). [INFERRED] name.
    /*  80 */ Vec2 m_lookDirection;
    // A focus point and object set from outside the agent (PE +0x64 / +0x70) by the
    // path-following move statements (PE 1019B3C0); the soldier input merger runs toward them.
    // The object is not initialised.
    /*  88 */ Vec3 m_focusPoint;
    /* 100 */ unsigned int m_focusObject;
    /* 104 */ BotSenses* m_senses;         // new(0x94) - the mechanism this agent steers
    /* 108 */ BotMain*   m_bot;            // the ctor argument
    // the whole tail is `reset`'s, not the constructor's - see the note at +24.
    /* 112 */ double m_double112;          // reset: the game time at reset
    /* 120 */ bool m_flag120;              // reset: false
    /* 121 */ bool m_flag121;              // reset: false
    /* 122 */ bool m_flag122;              // reset: false
    /* 123 */ char m_pad123;
    /* 124 */ double m_double124;          // reset: 0.0
    /* 132 */ bool m_flag132;              // reset: false
    /* 133 */ bool m_flag133;              // reset: false
    /* 134 */ bool m_flag134;              // ctor: false
    /* 135 */ char m_pad135;
    // the plan itself - one owned statement, guarded everywhere it is touched.
    /* 136 */ BotScript* m_plan;        // ctor: 0
    // Set true by `reset` and again by `deletePlan`, and nothing recovered so far reads it.
    // Meaning [UNRESOLVED]; the name stays neutral rather than guess at `m_planInvalid`.
    /* 140 */ bool m_flag140;              // reset: true, deletePlan: true
    /* 141 */ char m_pad141[3];
    /* 144 */ BAPContext* m_bapContext;    // new(4) - shared vocabulary with BotMain's own
    /* 148 */ bool m_flag148;              // reset: true
    /* 149 */ char m_pad149[3];
    /* 152 */ float m_reactionTime;        // reset: a random float in [0.5, 1.0) - see the .cpp
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSINGAGENT_H
