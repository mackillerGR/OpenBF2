// [CHOSEN, not recovered] The D3DX 9 entry points the renderer imports, for builds without d3dx9_25.dll.
// Maths are complete. Everything else stops loudly until a real implementation lands.
// [RETIRE-WHEN: portable-d3dx] effects, textures and surfaces have real implementations.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define FRONTIER(name) extern "C" long name() { std::fprintf(stderr, "[FRONTIER] " #name " reached\n"); std::fflush(stderr); std::abort(); }
FRONTIER(D3DXAssembleShader) FRONTIER(D3DXComputeBoundingBox) FRONTIER(D3DXComputeBoundingSphere)

FRONTIER(D3DXCreateMeshFVF) FRONTIER(D3DXCreateRenderToSurface)
  
 
FRONTIER(D3DXSaveSurfaceToFileA) FRONTIER(D3DXSaveTextureToFileA)
FRONTIER(BinkClose) FRONTIER(BinkCopyToBuffer) FRONTIER(BinkDoFrame) FRONTIER(BinkGetSummary)
FRONTIER(BinkNextFrame) FRONTIER(BinkPause) FRONTIER(BinkWait)
extern "C" void* BinkOpen(const char*, unsigned) { return nullptr; }  // No video codec: every movie fails to open.
extern "C" void* BinkOpenDirectSound(unsigned long) { return nullptr; }
extern "C" int BinkSetSoundSystem(void*, unsigned long) { return 0; }

namespace { struct M { float m[4][4]; }; struct V { float x, y, z; };
V sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V cross(V a, V b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V norm(V a) { float l = std::sqrt(dot(a, a)); return l > 0 ? V{a.x / l, a.y / l, a.z / l} : a; } }

extern "C" M* D3DXMatrixMultiply(M* out, const M* a, const M* b) {
    M r; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        r.m[i][j] = 0; for (int k = 0; k < 4; ++k) r.m[i][j] += a->m[i][k] * b->m[k][j]; }
    *out = r; return out;
}
extern "C" M* D3DXMatrixTranspose(M* out, const M* a) {
    M r; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) r.m[i][j] = a->m[j][i];
    *out = r; return out;
}
extern "C" M* D3DXMatrixInverse(M* out, float* det, const M* a) {
    double t[4][8]; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) { t[i][j] = a->m[i][j]; t[i][j + 4] = i == j; }
    double d = 1;
    for (int c = 0; c < 4; ++c) {
        int p = c; for (int r = c + 1; r < 4; ++r) if (std::fabs(t[r][c]) > std::fabs(t[p][c])) p = r;
        if (t[p][c] == 0) return nullptr;
        if (p != c) { for (int k = 0; k < 8; ++k) { double x = t[p][k]; t[p][k] = t[c][k]; t[c][k] = x; } d = -d; }
        d *= t[c][c]; const double s = 1 / t[c][c]; for (int k = 0; k < 8; ++k) t[c][k] *= s;
        for (int r = 0; r < 4; ++r) if (r != c) { const double f = t[r][c]; for (int k = 0; k < 8; ++k) t[r][k] -= f * t[c][k]; }
    }
    M r; for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) r.m[i][j] = (float)t[i][j + 4];
    if (det) *det = (float)d;
    *out = r; return out;
}
extern "C" M* D3DXMatrixLookAtLH(M* o, const V* eye, const V* at, const V* up) {
    V z = norm(sub(*at, *eye)), x = norm(cross(*up, z)), y = cross(z, x);
    M r = {{{x.x, y.x, z.x, 0}, {x.y, y.y, z.y, 0}, {x.z, y.z, z.z, 0}, {-dot(x, *eye), -dot(y, *eye), -dot(z, *eye), 1}}};
    *o = r; return o;
}
extern "C" M* D3DXMatrixOrthoLH(M* o, float w, float h, float zn, float zf) {
    M r = {{{2 / w, 0, 0, 0}, {0, 2 / h, 0, 0}, {0, 0, 1 / (zf - zn), 0}, {0, 0, zn / (zn - zf), 1}}};
    *o = r; return o;
}
extern "C" M* D3DXMatrixRotationZ(M* o, float a) {
    const float c = std::cos(a), s = std::sin(a);
    M r = {{{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    *o = r; return o;
}
extern "C" uint16_t* D3DXFloat32To16Array(uint16_t* out, const float* in, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        uint32_t b; std::memcpy(&b, &in[i], 4);
        const uint32_t s = (b >> 16) & 0x8000; const int e = ((b >> 23) & 255) - 112; uint32_t m = b & 0x7FFFFF;
        out[i] = (uint16_t)(e >= 31 ? s | 0x7BFF : e <= 0 ? s : s | (e << 10) | (m >> 13));
    }
    return out;
}
extern "C" void _purecall() { std::fprintf(stderr, "pure virtual call\n"); std::abort(); }
namespace renddx9abi {
void* crtMalloc(size_t n) { return std::malloc(n); }
void crtFree(void* p) { std::free(p); }
void installCrashReporter() {}
}
