// Control information plug-in; adds no virtual methods to InformationPlugIn.
// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the corresponding IPIControlInfo operation.
#ifndef AI_AIDLL_AI_AIKNOWLEDGE_IPICONTROLINFO_H
#define AI_AIDLL_AI_AIKNOWLEDGE_IPICONTROLINFO_H

#include "InformationPlugIn.h"
#include "Information.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

namespace dice { namespace hfe { namespace ai {

class IPIControlInfo : public InformationPlugIn
{
public:
    explicit IPIControlInfo(Information* information) { m_information = information; }

    // only the base's vptr store - nothing of its own.
    virtual ~IPIControlInfo() {}
    // 5 - Information::removePlugIn's case for the control-info plug-in.
    virtual int getType() { return 5; }
    // empty - the control values are read
    // through to the engine object on every call, so there is nothing to refresh.
    virtual void setTime() {}
    virtual void setTime(double /*time*/) {}

    // Read through to the engine object's control info. PE 1019CA90 / 1019CAB0 / 1019CAD0.
    float getCameraRelativeDofRotationOffset() const;
    unsigned long long getDriveControls() const;     // PE 1019C6B0
    unsigned long long getAimControls() const;       // PE 1019C6D0
    unsigned long long getLookControls() const;      // PE 1019C6F0
    io::PlayerInputMap getLookVerticalControl() const;     // PE 1019CA30
    io::PlayerInputMap getLookHorizontalControl() const;   // PE 1019CA50
    float getProportionalConstant() const;           // PE 1019C7D0
    float getDerivativeConstant() const;             // PE 1019C7F0
    float getAngleSpeedConstant() const;             // PE 1019C810
    const Vec3& getCameraRelativeMinRotation() const;
    const Vec3& getCameraRelativeMaxRotation() const;
    // Whether the camera may turn to the direction. PE 1019CBD0.
    bool validateCameraDirection(const Vec3& direction) const;
    // The camera's base position. PE 1019CBB0.
    void getCameraBasePos(Vec3& out) const;
    // The camera's transformation. PE 1019CB10.
    const Mat4& getCameraTransformation() const;
    // Whether the camera may turn toward the position. PE 1019CC30.
    bool validateCameraDirectionByPos(const Vec3& position) const;
};

inline bool IPIControlInfo::validateCameraDirection(const Vec3& direction) const
{
    return m_information->getAIObject()->getControlInfo()->validateCameraDirection(direction);
}

inline const Mat4& IPIControlInfo::getCameraTransformation() const
{
    return m_information->getAIObject()->getControlInfo()->getCameraTransformation();
}

inline void IPIControlInfo::getCameraBasePos(Vec3& out) const
{
    m_information->getAIObject()->getControlInfo()->getCameraBasePos(out);
}

inline bool IPIControlInfo::validateCameraDirectionByPos(const Vec3& position) const
{
    return m_information->getAIObject()->getControlInfo()->validateCameraDirectionByPos(position);
}

inline float IPIControlInfo::getCameraRelativeDofRotationOffset() const
{
    return m_information->getAIObject()->getControlInfo()->getCameraRelativeDofRotationOffset();
}

inline unsigned long long IPIControlInfo::getDriveControls() const
{
    return m_information->getAIObject()->getControlInfo()->getDriveControls();
}

inline unsigned long long IPIControlInfo::getAimControls() const
{
    return m_information->getAIObject()->getControlInfo()->getAimControls();
}

inline unsigned long long IPIControlInfo::getLookControls() const
{
    return m_information->getAIObject()->getControlInfo()->getLookControls();
}

inline io::PlayerInputMap IPIControlInfo::getLookVerticalControl() const
{
    return m_information->getAIObject()->getControlInfo()->getLookVerticalControl();
}

inline io::PlayerInputMap IPIControlInfo::getLookHorizontalControl() const
{
    return m_information->getAIObject()->getControlInfo()->getLookHorizontalControl();
}

inline float IPIControlInfo::getProportionalConstant() const
{
    return m_information->getAIObject()->getControlInfo()->getProportionalConstant();
}

inline float IPIControlInfo::getDerivativeConstant() const
{
    return m_information->getAIObject()->getControlInfo()->getDerivativeConstant();
}

inline float IPIControlInfo::getAngleSpeedConstant() const
{
    return m_information->getAIObject()->getControlInfo()->getAngleSpeedConstant();
}

inline const Vec3& IPIControlInfo::getCameraRelativeMinRotation() const
{
    return m_information->getAIObject()->getControlInfo()->getCameraRelativeMinRotation();
}

inline const Vec3& IPIControlInfo::getCameraRelativeMaxRotation() const
{
    return m_information->getAIObject()->getControlInfo()->getCameraRelativeMaxRotation();
}
} } }
#endif
