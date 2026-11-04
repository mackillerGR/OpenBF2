// [INFERRED] split in the attested BF2HudManager.cpp family. Complete653-byte
// PE752B00 and its helpers read. Filters stay separate.
#include "Core/X87Emu.h"
#include "BF2HudManager.h"
#if !DEDICATED
#include "Ref2System.h"
#include "../External/BFMeme/Pointer.h"
#include "../External/BFMeme/Style.h"
#include "../IO/Input/Input.h"
namespace dice { namespace hfe { namespace io { extern IBase* inputDeviceManager; } } }
namespace dice { namespace hfe { namespace hud {
namespace {
float centeredInputOrigin(float origin, float dimension)
{
    const float half = 0.5f;
    float result;
#if defined(_M_IX86)
    __asm {
        fld dimension
        fmul half
        fsubr origin
        fstp result
    }
#else
    {
        x87::Cpu c;
        c.push(dimension);
        c.s(0) *= half;
        c.s(0) = origin - c.s(0);
        result = static_cast<std::remove_reference<decltype(result)>::type>(c.s(0)); c.pop();
    }
#endif
    return result;
}

void moveInputPointer(float* position, float x, float y, int sensitivity,
                      bool absolute, float width, float height)
{
    const float half = 0.5f;
    if (absolute) {
#if defined(_M_IX86)
        __asm {
            mov eax, position
            fld x
            fmul width
            fmul half
            fstp dword ptr [eax]
            fld y
            fmul height
            fmul half
            fstp dword ptr [eax+4]
        }
#else
        {
            x87::Cpu c;
            c.eax = x87::toReg(position);
            c.push(x);
            c.s(0) *= width;
            c.s(0) *= half;
            x87::f32(c.eax) = float(c.s(0)); c.pop();
            c.push(y);
            c.s(0) *= height;
            c.s(0) *= half;
            x87::f32(c.eax + 4) = float(c.s(0)); c.pop();
        }
#endif
    } else {
        // fild preserves all bits of the signed sensitivity until multiplication.
#if defined(_M_IX86)
        __asm {
            mov eax, position
            fild sensitivity
            fld x
            fmul st(0), st(1)
            fadd dword ptr [eax]
            fstp dword ptr [eax]
            fmul y
            fadd dword ptr [eax+4]
            fstp dword ptr [eax+4]
        }
#else
        {
            x87::Cpu c;
            c.eax = x87::toReg(position);
            c.push(double(sensitivity));
            c.push(x);
            c.s(0) *= c.s(1);
            c.s(0) += x87::f32(c.eax);
            x87::f32(c.eax) = float(c.s(0)); c.pop();
            c.s(0) *= y;
            c.s(0) += x87::f32(c.eax + 4);
            x87::f32(c.eax + 4) = float(c.s(0)); c.pop();
        }
#endif
    }
}

void clampInputCoordinate(float& value, float origin, float extent)
{
    // Keep the unrounded upper-bound sum and the shipped unordered comparisons.
#if defined(_M_IX86)
    __asm {
        mov edx, value
        fld dword ptr [edx]
        fcomp origin
        fnstsw ax
        test ah, 5
        jp upper_bound
        mov eax, origin
        mov [edx], eax
        jmp clamp_done
    upper_bound:
        fld extent
        fadd origin
        fcom dword ptr [edx]
        fnstsw ax
        test ah, 5
        jp upper_unchanged
        fstp dword ptr [edx]
        jmp clamp_done
    upper_unchanged:
        fstp st(0)
    clamp_done:
    }
#else
    {
        x87::Cpu c;
        c.edx = x87::toReg(&value);
        c.push(x87::f32(c.edx));
        c.compare(c.s(0), origin); c.pop();
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto upper_bound;
        c.eax = x87::toReg(origin);
        x87::u32(c.edx) = uint32_t(c.eax);
        goto clamp_done;
        upper_bound: ;
        c.push(extent);
        c.s(0) += origin;
        c.compare(c.s(0), x87::f32(c.edx));
        c.eax = (c.eax & ~uintptr_t(0xFFFF)) | c.sw;
        c.testFlags(uint32_t(((c.eax >> 8) & 0xFF) & uintptr_t(5)));
        if (c.pf) goto upper_unchanged;
        x87::f32(c.edx) = float(c.s(0)); c.pop();
        goto clamp_done;
        upper_unchanged: ;
        c.s(0) = c.s(0); c.pop();
        clamp_done: ;
    }
#endif
}
}

void BF2HudManager::handleGameInput(float /*delta*/, io::GameInput& input)
{
    if (!ref2System) return;
    ref2System->setInput(input, 0);
    if (!m_field258) return;
    int sensitivity = m_field180;
    const float y = (input.mappingFlags & 0x200000u) ? input.inputs[21] : 0.0f;
    const float x = (input.mappingFlags & 0x100000u) ? input.inputs[20] : 0.0f;
    meme::Rect bounds = {-400.0f, -300.0f, 800.0f, 600.0f, 0.0f};
    // Do not cache getMode: every comparison is a separate retail virtual call.
    if (getMode() == 3 || getMode() == 4 || getMode() == 5 || getMode() == 7
            || getMode() == 8 || getMode() == 30 || getMode() == 31) {
        bounds.width = m_field200;
        bounds.height = m_field204;
        bounds.x = centeredInputOrigin(m_field208, ref2System->getWidth());
        bounds.y = centeredInputOrigin(m_field20c, ref2System->getHeight());
        sensitivity = m_field17c;
    } else if (getMode() == 6) {
        bounds.width = m_field218;
        bounds.height = m_field21c;
        bounds.x = centeredInputOrigin(m_field220, ref2System->getWidth());
        bounds.y = centeredInputOrigin(m_field224, ref2System->getHeight());
        sensitivity = m_field17c;
    }
    float* position = ref2System->getPointerPosition();
    const bool absolute = static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)
            ->getInputDevice(io::IDFFalcon)
        && static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)
            ->getInputDevice(io::IDFFalcon)->isActive();
    moveInputPointer(position, x, y, sensitivity, absolute, bounds.width, bounds.height);
    clampInputCoordinate(position[0], bounds.x, bounds.width);
    clampInputCoordinate(position[1], bounds.y, bounds.height);
    const float pointerX = position[0], pointerY = position[1];
    auto* pointer = static_cast<meme::Pointer*>(ref2System->getPointerObject());
    meme::positionPointer(ref2System, pointer, pointerX, pointerY);
}
} } }
#endif
