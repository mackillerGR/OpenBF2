// [INFERRED] path. Complete table94D000, metadata94CD6C, constructor8330D0,
// factory/eager and all differing instructions read. No runtime math shim.
#include "Core/X87Emu.h"
#include "SetVariableSineAction.h"
#if !DEDICATED
#include "Data.h"
#include "Node.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// PE832B86..C39, same x87 order and float stores. Helper/name [INFERRED].
// Below braking distance: cos(k - distance/braking*k)*speed*delta.
// Otherwise: delta*speed. k is retail float3FC90E56, not a replacement pi/2.
// Keep fcos and extended intermediates; do not substitute a CRT cosine call.
bool chooseSineValue(float current, float target, float delta, float speed,
                     float braking, float& value)
{
    const float angle = 1.57075f;                        // 94CB68, bytes56 0E C9 3F
    float rounded;
    bool changed = false;
#if defined(_M_IX86)
    __asm {
        fld target
        fld current
        fcomp st(1)
        fld current
        fnstsw ax
        test ah, 5
        jp decreasing
        fsubr st(0), st(1)
        fcom braking
        fnstsw ax
        test ah, 5
        jp increasing_full_speed
        fdiv braking
        fmul angle
        fsubr angle
        fcos
        fmul speed
        fmul delta
        jmp increasing_add
    increasing_full_speed:
        fstp st(0)
        fld delta
        fmul speed
    increasing_add:
        fadd current
        fst rounded
        fcomp st(1)
        fnstsw ax
        test ah, 41h
        jz use_target
    keep_candidate:
        fstp st(0)
        jmp write_value
    decreasing:
        fcomp st(1)
        fnstsw ax
        test ah, 41h
        jnz unchanged
        fld current
        fsub st(0), st(1)
        fcom braking
        fnstsw ax
        test ah, 5
        jp decreasing_full_speed
        fdiv braking
        fmul angle
        fsubr angle
        fcos
        fmul speed
        fmul delta
        jmp decreasing_subtract
    decreasing_full_speed:
        fstp st(0)
        fld delta
        fmul speed
    decreasing_subtract:
        fsubr current
        fst rounded
        fcomp st(1)
        fnstsw ax
        test ah, 5
        jp keep_candidate
    use_target:
        fstp rounded
    write_value:
        mov changed, 1
        jmp done
    unchanged:
        fstp st(0)
    done:
    }
#else
    {
        x87::Cpu c;
        c.push(target);
        c.push(current);
        c.compare(c.s(0), c.s(1)); c.pop();
        c.push(current);
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto decreasing;
        c.s(0) = c.s(1) - c.s(0);
        c.compare(c.s(0), braking);
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto increasing_full_speed;
        c.s(0) /= braking;
        c.s(0) *= angle;
        c.s(0) = angle - c.s(0);
        c.s(0) = std::cos(c.s(0));
        c.s(0) *= speed;
        c.s(0) *= delta;
        goto increasing_add;
        increasing_full_speed: ;
        c.s(0) = c.s(0); c.pop();
        c.push(delta);
        c.s(0) *= speed;
        increasing_add: ;
        c.s(0) += current;
        rounded = static_cast<std::remove_reference<decltype(rounded)>::type>(c.s(0));
        c.compare(c.s(0), c.s(1)); c.pop();
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(0x41)));
        if (c.zf) goto use_target;
        keep_candidate: ;
        c.s(0) = c.s(0); c.pop();
        goto write_value;
        decreasing: ;
        c.compare(c.s(0), c.s(1)); c.pop();
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(0x41)));
        if (!c.zf) goto unchanged;
        c.push(current);
        c.s(0) -= c.s(1);
        c.compare(c.s(0), braking);
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto decreasing_full_speed;
        c.s(0) /= braking;
        c.s(0) *= angle;
        c.s(0) = angle - c.s(0);
        c.s(0) = std::cos(c.s(0));
        c.s(0) *= speed;
        c.s(0) *= delta;
        goto decreasing_subtract;
        decreasing_full_speed: ;
        c.s(0) = c.s(0); c.pop();
        c.push(delta);
        c.s(0) *= speed;
        decreasing_subtract: ;
        c.s(0) = current - c.s(0);
        rounded = static_cast<std::remove_reference<decltype(rounded)>::type>(c.s(0));
        c.compare(c.s(0), c.s(1)); c.pop();
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto keep_candidate;
        use_target: ;
        rounded = static_cast<std::remove_reference<decltype(rounded)>::type>(c.s(0)); c.pop();
        write_value: ;
        x87::fromReg(changed, uintptr_t(1));
        goto done;
        unchanged: ;
        c.s(0) = c.s(0); c.pop();
        done: ;
    }
#endif
    if (changed) value = rounded;
    return changed;
}
}
SetVariableSineAction::SetVariableSineAction() : m_brakingDistance(0.0f) {}
TypeDescriptor* SetVariableSineAction::getClassType()
{
    static ClassTypeDescriptor<SetVariableSineAction, SetVariableSoftAction>
        type("dice::meme::SetVariableSineAction");
    return &type;
}
TypeDescriptor* SetVariableSineAction::getType() { return getClassType(); }
unsigned int SetVariableSineAction::getObjectSize() { return sizeof(SetVariableSineAction); }
namespace {
// CRT96E180 ->870FC0; same descriptorA39DB8, independent counterA39D48.
TypeDescriptor* const registeredSetVariableSineActionType = SetVariableSineAction::getClassType();
}
bool SetVariableSineAction::streamProperties(System* system, Stream* stream)
{
    // The PE inlines Soft's property visitor: SetVariableAction, Speed, then braking.
    if (!SetVariableSoftAction::streamProperties(system, stream)) return false;
    return stream->streamFloat(&m_brakingDistance, "Braking distance") != false;
}
bool SetVariableSineAction::execute(System* system, const Rect*, const NodeState*,
                                    const NodeInputEvent* event)
{
    if (m_variable && m_data && event->type == 22)
    {
        const float delta = event->first;
        const float current = m_variable->getFloat(system);
        const float target = m_data->getFloat(system);
        float value;
        if (chooseSineValue(current, target, delta, m_speed, m_brakingDistance, value))
            m_variable->setFloat(system, value); // Fresh post-callback destination/table; result ignored.
    }
    return false;
}
static_assert(sizeof(SetVariableSineAction) == 20, "client SetVariableSineAction");
static_assert(sizeof(ClassTypeDescriptor<SetVariableSineAction, SetVariableSoftAction>) == 16,
              "SetVariableSineAction metadata");
} }
#endif
