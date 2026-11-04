// World-to-HUD projection. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "Core/X87Emu.h"
#include "BF2HudManager.h"
#if !DEDICATED
#include "Ref2System.h"
#include "../RendMisc/IRenderView.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Scene/Object/PlayerManager.h"
namespace dice { namespace hfe { namespace hud {
namespace {
// Keep the PE's unrounded x87 multiply/add chain up to its final float store.
// Mat4's existing affine inverse cannot invert a general projection matrix.
float affineComponent(float a, float ma, float b, float mb,
                      float c, float mc, float translation)
{
    float result;
#if defined(_M_IX86)
    __asm {
        fld a
        fmul ma
        fld b
        fmul mb
        faddp st(1), st(0)
        fld c
        fmul mc
        faddp st(1), st(0)
        fadd translation
        fstp result
    }
#else
    {
        const double cIn = c;
        x87::Cpu cpu;
        cpu.push(a);
        cpu.s(0) *= ma;
        cpu.push(b);
        cpu.s(0) *= mb;
        cpu.s(1) += cpu.s(0); cpu.pop();
        cpu.push(cIn);
        cpu.s(0) *= mc;
        cpu.s(1) += cpu.s(0); cpu.pop();
        cpu.s(0) += translation;
        result = static_cast<std::remove_reference<decltype(result)>::type>(cpu.s(0)); cpu.pop();
    }
#endif
    return result;
}

Vec3 affineInOrder(const Mat4& matrix, const Vec3& point, int first, int second, int third)
{
    const float p[3] = {point.x, point.y, point.z};
    Vec3 result;
    result.z = affineComponent(p[first], matrix.m[first * 4 + 2],
        p[second], matrix.m[second * 4 + 2], p[third], matrix.m[third * 4 + 2], matrix.m[14]);
    result.y = affineComponent(p[first], matrix.m[first * 4 + 1],
        p[second], matrix.m[second * 4 + 1], p[third], matrix.m[third * 4 + 1], matrix.m[13]);
    result.x = affineComponent(p[first], matrix.m[first * 4],
        p[second], matrix.m[second * 4], p[third], matrix.m[third * 4], matrix.m[12]);
    return result;
}

float screenCoordinate(float numerator, float denominator)
{
    float result;
#if defined(_M_IX86)
    __asm {
        fld numerator
        fdiv denominator
        fstp result
    }
#else
    {
        x87::Cpu c;
        c.push(numerator);
        c.s(0) /= denominator;
        result = static_cast<std::remove_reference<decltype(result)>::type>(c.s(0)); c.pop();
    }
#endif
    return result;
}

void projectToScreen(const Vec3& clip, float halfWidth, float height, Vec3& result)
{
    const float half = 0.5f;
    // The PE divides by transformed Z, not by a homogeneous W. Unordered,
    // zero and negative Z take the same hidden-point arm; X/Y stay zero.
    if (!(clip.z > 0.0f)) {
        result.z = -1.0f;
        return;
    }
    const float x = clip.x, y = clip.y, z = clip.z;
    float outputX, outputY, outputZ;
#if defined(_M_IX86)
    __asm {
        fld height
        fmul half
        fld1
        fdiv z
        fld x
        fmul st(0), st(1)
        fmul halfWidth
        fstp outputX
        fxch st(1)
        fchs
        fmul y
        fmul st(0), st(1)
        fstp outputY
        fstp outputZ
    }
#else
    {
        x87::Cpu c;
        c.push(height);
        c.s(0) *= half;
        c.push(1.0);
        c.s(0) /= z;
        c.push(x);
        c.s(0) *= c.s(1);
        c.s(0) *= halfWidth;
        outputX = static_cast<std::remove_reference<decltype(outputX)>::type>(c.s(0)); c.pop();
        { double t = c.s(0); c.s(0) = c.s(1); c.s(1) = t; }
        c.s(0) = -c.s(0);
        c.s(0) *= y;
        c.s(0) *= c.s(1);
        outputY = static_cast<std::remove_reference<decltype(outputY)>::type>(c.s(0)); c.pop();
        outputZ = static_cast<std::remove_reference<decltype(outputZ)>::type>(c.s(0)); c.pop();
    }
#endif
    result.x = outputX;
    result.y = outputY;
    result.z = outputZ;
}
}

Vec3 BF2HudManager::screenToWorld(Vec2 position)
{
    Vec3 result(Vec3::getZero());
    if (!ref2System) return result;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!local) return result;
    auto* ui = static_cast<dx9rend::IRenderView*>(ref2System->getRenderView());
    auto* scene = static_cast<dx9rend::IRenderView*>(ref2System->getPreviousRenderView());
    const float halfWidth = ui->getOrthoWidth() * 0.5f;
    const float halfHeight = ui->getOrthoHeight() * 0.5f;
    const float nearPlane = scene->getNearPlane();
    const Vec3 normalized = {screenCoordinate(position.x, halfWidth),
                            screenCoordinate(position.y, -halfHeight), nearPlane};
    Mat4 projection(*scene->getProjectionMatrixPtr());
    const Mat4 source(projection);
    fullInvert(source, projection);
    const Vec3 cameraPoint = affineInOrder(projection, normalized, 2, 1, 0);
    result = affineInOrder(local->getCamera()->getTransformation(0.0f), cameraPoint, 1, 2, 0);
    return result;
}

Vec3 BF2HudManager::worldToScreen(Vec3 position)
{
    Vec3 result(Vec3::getZero());
    if (!ref2System) return result;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    auto* ui = static_cast<dx9rend::IRenderView*>(ref2System->getRenderView());
    auto* scene = static_cast<dx9rend::IRenderView*>(ref2System->getPreviousRenderView());
    // Unlike screenToWorld, the shipped body does not guard the local player.
    Mat4 camera(local->getCamera()->getTransformation(0.0f));
    const Mat4 source(camera);
    fullInvert(source, camera);
    const Vec3 cameraPoint = affineInOrder(camera, position, 0, 1, 2);
    const Vec3 clip = affineInOrder(*scene->getProjectionMatrixPtr(), cameraPoint, 1, 2, 0);
    const float halfWidth = ui->getOrthoWidth() * 0.5f;
    const float height = ui->getOrthoHeight();
    projectToScreen(clip, halfWidth, height, result);
    return result;
}
} } }
#endif
