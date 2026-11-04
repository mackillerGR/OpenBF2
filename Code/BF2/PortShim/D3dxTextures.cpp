// [CHOSEN, not recovered] Buffers, texture loading and surface copies of the D3DX 9 API, on plain
// D3D9 calls. Filtering is nearest for stretches and box for mip chains; retail D3DX output is not
// reproduced to the bit. Sources in block-compressed formats copy only to the same format and size.
#include <windows.h>
#include <d3d9.h>

// The few D3DX declarations this file implements. The full d3dx9.h needs GDI and COM headers that
// DXVK's native windows.h does not carry.
struct ID3DXBuffer : public IUnknown {
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
typedef ID3DXBuffer* LPD3DXBUFFER;
enum D3DXIMAGE_FILEFORMAT { D3DXIFF_BMP, D3DXIFF_JPG, D3DXIFF_TGA, D3DXIFF_PNG, D3DXIFF_DDS, D3DXIFF_PPM, D3DXIFF_DIB, D3DXIFF_HDR, D3DXIFF_PFM };
struct D3DXIMAGE_INFO { UINT Width, Height, Depth, MipLevels; D3DFORMAT Format; D3DRESOURCETYPE ResourceType; D3DXIMAGE_FILEFORMAT ImageFileFormat; };
struct D3DXVECTOR2 { float x, y; D3DXVECTOR2() {} D3DXVECTOR2(float a, float b) : x(a), y(b) {} };
struct D3DXVECTOR4 { float x, y, z, w; };
typedef VOID (WINAPI* LPD3DXFILL2D)(D3DXVECTOR4*, const D3DXVECTOR2*, const D3DXVECTOR2*, LPVOID);
#define D3DX_DEFAULT ((UINT)-1)
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#include "stb_image.h"

namespace {
class Buffer : public ID3DXBuffer {
public:
    explicit Buffer(DWORD n) : m_data(n) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) { *out = this; AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() { const ULONG r = --m_ref; if (!r) delete this; return r; }
    LPVOID STDMETHODCALLTYPE GetBufferPointer() { return m_data.data(); }
    DWORD STDMETHODCALLTYPE GetBufferSize() { return (DWORD)m_data.size(); }
private:
    ULONG m_ref = 1;
    std::vector<unsigned char> m_data;
};
struct Rgba { float r, g, b, a; };
bool isBlock(D3DFORMAT f) { return f == D3DFMT_DXT1 || f == D3DFMT_DXT2 || f == D3DFMT_DXT3 || f == D3DFMT_DXT4 || f == D3DFMT_DXT5; }
UINT blockBytes(D3DFORMAT f) { return f == D3DFMT_DXT1 ? 8 : 16; }
UINT texelBytes(D3DFORMAT f) {
    switch (f) {
        case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8: case D3DFMT_V16U16: case D3DFMT_R32F: return 4;
        case D3DFMT_R8G8B8: return 3;
        case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: case D3DFMT_A8L8: case D3DFMT_V8U8: return 2;
        case D3DFMT_L8: case D3DFMT_A8: return 1;
        default: return 0;
    }
}
Rgba readTexel(D3DFORMAT f, const unsigned char* p) {
    unsigned v = 0; std::memcpy(&v, p, std::min(texelBytes(f), 4u));
    const auto c = [](unsigned x, unsigned bits) { return (float)x / (float)((1u << bits) - 1); };
    switch (f) {
        case D3DFMT_A8R8G8B8: return {c(v >> 16 & 255, 8), c(v >> 8 & 255, 8), c(v & 255, 8), c(v >> 24, 8)};
        case D3DFMT_X8R8G8B8: case D3DFMT_R8G8B8: return {c(v >> 16 & 255, 8), c(v >> 8 & 255, 8), c(v & 255, 8), 1};
        case D3DFMT_A8B8G8R8: return {c(v & 255, 8), c(v >> 8 & 255, 8), c(v >> 16 & 255, 8), c(v >> 24, 8)};
        case D3DFMT_X8B8G8R8: return {c(v & 255, 8), c(v >> 8 & 255, 8), c(v >> 16 & 255, 8), 1};
        case D3DFMT_R5G6B5: return {c(v >> 11 & 31, 5), c(v >> 5 & 63, 6), c(v & 31, 5), 1};
        case D3DFMT_X1R5G5B5: return {c(v >> 10 & 31, 5), c(v >> 5 & 31, 5), c(v & 31, 5), 1};
        case D3DFMT_A1R5G5B5: return {c(v >> 10 & 31, 5), c(v >> 5 & 31, 5), c(v & 31, 5), (float)(v >> 15 & 1)};
        case D3DFMT_A4R4G4B4: return {c(v >> 8 & 15, 4), c(v >> 4 & 15, 4), c(v & 15, 4), c(v >> 12 & 15, 4)};
        case D3DFMT_X4R4G4B4: return {c(v >> 8 & 15, 4), c(v >> 4 & 15, 4), c(v & 15, 4), 1};
        case D3DFMT_L8: { float l = c(v & 255, 8); return {l, l, l, 1}; }
        case D3DFMT_A8: return {0, 0, 0, c(v & 255, 8)};
        case D3DFMT_A8L8: { float l = c(v & 255, 8); return {l, l, l, c(v >> 8 & 255, 8)}; }
        default: return {0, 0, 0, 1};
    }
}
void writeTexel(D3DFORMAT f, unsigned char* p, const Rgba& t) {
    const auto q = [](float x, unsigned bits) { return (unsigned)(std::min(std::max(x, 0.f), 1.f) * ((1u << bits) - 1) + 0.5f); };
    unsigned v = 0;
    switch (f) {
        case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_R8G8B8: v = q(t.a, 8) << 24 | q(t.r, 8) << 16 | q(t.g, 8) << 8 | q(t.b, 8); break;
        case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8: v = q(t.a, 8) << 24 | q(t.b, 8) << 16 | q(t.g, 8) << 8 | q(t.r, 8); break;
        case D3DFMT_R5G6B5: v = q(t.r, 5) << 11 | q(t.g, 6) << 5 | q(t.b, 5); break;
        case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: v = q(t.a, 1) << 15 | q(t.r, 5) << 10 | q(t.g, 5) << 5 | q(t.b, 5); break;
        case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: v = q(t.a, 4) << 12 | q(t.r, 4) << 8 | q(t.g, 4) << 4 | q(t.b, 4); break;
        case D3DFMT_L8: v = q(t.r, 8); break;
        case D3DFMT_A8: v = q(t.a, 8); break;
        case D3DFMT_A8L8: v = q(t.a, 8) << 8 | q(t.r, 8); break;
        default: break;
    }
    std::memcpy(p, &v, texelBytes(f));
}
// Copies a rectangle between locked images; block formats need equal format and extent.
HRESULT copyPixels(unsigned char* dst, INT dstPitch, D3DFORMAT dstFmt, UINT dw, UINT dh,
                   const unsigned char* src, INT srcPitch, D3DFORMAT srcFmt, UINT sw, UINT sh) {
    if (dstFmt == srcFmt && dw == sw && dh == sh) {
        const UINT rows = isBlock(dstFmt) ? (dh + 3) / 4 : dh;
        const UINT line = isBlock(dstFmt) ? (dw + 3) / 4 * blockBytes(dstFmt) : dw * texelBytes(dstFmt);
        if (!line) return D3DERR_INVALIDCALL;
        for (UINT y = 0; y < rows; ++y) std::memcpy(dst + y * dstPitch, src + y * srcPitch, line);
        return D3D_OK;
    }
    const UINT db = texelBytes(dstFmt), sb = texelBytes(srcFmt);
    if (!db || !sb) { std::fprintf(stderr, "D3DX: unsupported conversion %d -> %d\n", (int)srcFmt, (int)dstFmt); return D3DERR_INVALIDCALL; }
    for (UINT y = 0; y < dh; ++y) {
        const unsigned char* row = src + (uint64_t)y * sh / dh * srcPitch;
        for (UINT x = 0; x < dw; ++x)
            writeTexel(dstFmt, dst + y * dstPitch + x * db, readTexel(srcFmt, row + (uint64_t)x * sw / dw * sb));
    }
    return D3D_OK;
}
struct DdsInfo { UINT w, h, depth, mips; D3DFORMAT fmt; bool cube, volume; const unsigned char* pixels; };
bool parseDds(const unsigned char* d, UINT n, DdsInfo& o) {
    if (n < 128 || std::memcmp(d, "DDS ", 4)) return false;
    const auto u = [d](UINT at) { uint32_t v; std::memcpy(&v, d + at, 4); return v; };
    o.h = u(12); o.w = u(16); o.depth = std::max(1u, u(24)); o.mips = std::max(1u, u(28));
    const uint32_t pf = u(80), fcc = u(84), bits = u(88), rm = u(92), am = u(104), caps2 = u(112);
    o.cube = caps2 & 0x200; o.volume = caps2 & 0x200000; o.pixels = d + 128;
    if (pf & 4) o.fmt = (D3DFORMAT)fcc;
    else if (bits == 32) o.fmt = rm == 0xFF0000 ? (am ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8) : (am ? D3DFMT_A8B8G8R8 : D3DFMT_X8B8G8R8);
    else if (bits == 24) o.fmt = D3DFMT_R8G8B8;
    else if (bits == 16) o.fmt = rm == 0xF800 ? D3DFMT_R5G6B5 : rm == 0x7C00 ? (am ? D3DFMT_A1R5G5B5 : D3DFMT_X1R5G5B5) : rm == 0xF00 ? D3DFMT_A4R4G4B4 : rm == 0xFF ? D3DFMT_A8L8 : D3DFMT_V8U8;
    else if (bits == 8) o.fmt = (pf & 2) ? D3DFMT_A8 : D3DFMT_L8;
    else return false;
    return true;
}
UINT mipBytes(D3DFORMAT f, UINT w, UINT h) { return isBlock(f) ? (w + 3) / 4 * ((h + 3) / 4) * blockBytes(f) : w * h * texelBytes(f); }
}  // namespace

extern "C" {
HRESULT WINAPI D3DXCreateBuffer(DWORD n, LPD3DXBUFFER* out) { *out = new Buffer(n); return D3D_OK; }

HRESULT WINAPI D3DXCreateTexture(LPDIRECT3DDEVICE9 dev, UINT w, UINT h, UINT mips, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, LPDIRECT3DTEXTURE9* out) {
    w = w ? w : 256; h = h ? h : 256;
    return dev->CreateTexture(w, h, mips, usage, fmt == D3DFMT_UNKNOWN ? D3DFMT_A8R8G8B8 : fmt, pool, out, nullptr);
}
HRESULT WINAPI D3DXGetImageInfoFromFileInMemory(LPCVOID data, UINT size, D3DXIMAGE_INFO* info) {
    const unsigned char* d = (const unsigned char*)data;
    std::memset(info, 0, sizeof(*info));
    DdsInfo dds;
    if (parseDds(d, size, dds)) {
        info->Width = dds.w; info->Height = dds.h; info->Depth = dds.depth; info->MipLevels = dds.mips; info->Format = dds.fmt;
        info->ResourceType = dds.cube ? D3DRTYPE_CUBETEXTURE : dds.volume ? D3DRTYPE_VOLUMETEXTURE : D3DRTYPE_TEXTURE;
        info->ImageFileFormat = D3DXIFF_DDS;
        return D3D_OK;
    }
    int w, h, c;
    if (!stbi_info_from_memory(d, (int)size, &w, &h, &c)) return D3DERR_INVALIDCALL;
    info->Width = w; info->Height = h; info->Depth = 1; info->MipLevels = 1;
    info->Format = c == 4 || c == 2 ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
    info->ResourceType = D3DRTYPE_TEXTURE;
    info->ImageFileFormat = size > 8 && d[0] == 0x89 ? D3DXIFF_PNG : size > 2 && d[0] == 0xFF ? D3DXIFF_JPG : D3DXIFF_TGA;
    return D3D_OK;
}
HRESULT WINAPI D3DXFilterTexture(LPDIRECT3DBASETEXTURE9 base, const PALETTEENTRY*, UINT, DWORD) {
    IDirect3DTexture9* t = nullptr;
    if (FAILED(base->QueryInterface(__uuidof(IDirect3DTexture9), (void**)&t)) || !t) return D3D_OK;
    D3DSURFACE_DESC d; t->GetLevelDesc(0, &d);
    const UINT tb = texelBytes(d.Format);
    if (tb) for (DWORD l = 1; l < t->GetLevelCount(); ++l) {
        D3DLOCKED_RECT a, b; D3DSURFACE_DESC s, m; t->GetLevelDesc(l - 1, &s); t->GetLevelDesc(l, &m);
        if (FAILED(t->LockRect(l - 1, &a, nullptr, D3DLOCK_READONLY))) break;
        if (FAILED(t->LockRect(l, &b, nullptr, 0))) { t->UnlockRect(l - 1); break; }
        for (UINT y = 0; y < m.Height; ++y) for (UINT x = 0; x < m.Width; ++x) {
            Rgba sum{0, 0, 0, 0}; const UINT sx = x * s.Width / m.Width, sy = y * s.Height / m.Height;
            for (UINT j = 0; j < 2; ++j) for (UINT i = 0; i < 2; ++i) {
                const UINT px = std::min(sx + i, s.Width - 1), py = std::min(sy + j, s.Height - 1);
                const Rgba p = readTexel(d.Format, (unsigned char*)a.pBits + py * a.Pitch + px * tb);
                sum.r += p.r / 4; sum.g += p.g / 4; sum.b += p.b / 4; sum.a += p.a / 4;
            }
            writeTexel(d.Format, (unsigned char*)b.pBits + y * b.Pitch + x * tb, sum);
        }
        t->UnlockRect(l); t->UnlockRect(l - 1);
    }
    t->Release();
    return D3D_OK;
}
HRESULT WINAPI D3DXFillTexture(LPDIRECT3DTEXTURE9 t, LPD3DXFILL2D fn, LPVOID user) {
    for (DWORD l = 0; l < t->GetLevelCount(); ++l) {
        D3DSURFACE_DESC d; D3DLOCKED_RECT r; t->GetLevelDesc(l, &d);
        const UINT tb = texelBytes(d.Format);
        if (!tb) return D3DERR_INVALIDCALL;
        if (FAILED(t->LockRect(l, &r, nullptr, 0))) return D3DERR_INVALIDCALL;
        const D3DXVECTOR2 size(1.f / d.Width, 1.f / d.Height);
        for (UINT y = 0; y < d.Height; ++y) for (UINT x = 0; x < d.Width; ++x) {
            D3DXVECTOR4 c; const D3DXVECTOR2 p((x + .5f) / d.Width, (y + .5f) / d.Height);
            fn(&c, &p, &size, user);
            writeTexel(d.Format, (unsigned char*)r.pBits + y * r.Pitch + x * tb, Rgba{c.x, c.y, c.z, c.w});
        }
        t->UnlockRect(l);
    }
    return D3D_OK;
}
HRESULT WINAPI D3DXLoadSurfaceFromMemory(LPDIRECT3DSURFACE9 dst, const PALETTEENTRY*, const RECT* dr, LPCVOID src, D3DFORMAT fmt, UINT pitch, const PALETTEENTRY*, const RECT* sr, DWORD, D3DCOLOR) {
    D3DSURFACE_DESC d; dst->GetDesc(&d);
    D3DLOCKED_RECT l;
    HRESULT hr = dst->LockRect(&l, dr, 0);
    if (FAILED(hr)) return hr;
    const UINT dw = dr ? dr->right - dr->left : d.Width, dh = dr ? dr->bottom - dr->top : d.Height;
    const UINT sb = texelBytes(fmt);
    const unsigned char* p = (const unsigned char*)src + (sr ? sr->top * pitch + sr->left * sb : 0);
    hr = copyPixels((unsigned char*)l.pBits, l.Pitch, d.Format, dw, dh, p, pitch, fmt, sr ? sr->right - sr->left : dw, sr ? sr->bottom - sr->top : dh);
    dst->UnlockRect();
    return hr;
}
HRESULT WINAPI D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9 dst, const PALETTEENTRY*, const RECT* dr, LPDIRECT3DSURFACE9 src, const PALETTEENTRY*, const RECT* sr, DWORD, D3DCOLOR) {
    D3DSURFACE_DESC s, d; src->GetDesc(&s); dst->GetDesc(&d);
    D3DLOCKED_RECT a, b;
    HRESULT hr = src->LockRect(&a, sr, D3DLOCK_READONLY);
    if (FAILED(hr)) return hr;
    hr = dst->LockRect(&b, dr, 0);
    if (FAILED(hr)) { src->UnlockRect(); return hr; }
    hr = copyPixels((unsigned char*)b.pBits, b.Pitch, d.Format, dr ? dr->right - dr->left : d.Width, dr ? dr->bottom - dr->top : d.Height,
                    (unsigned char*)a.pBits, a.Pitch, s.Format, sr ? sr->right - sr->left : s.Width, sr ? sr->bottom - sr->top : s.Height);
    dst->UnlockRect(); src->UnlockRect();
    return hr;
}
HRESULT WINAPI D3DXCreateTextureFromFileInMemory(LPDIRECT3DDEVICE9 dev, LPCVOID data, UINT size, LPDIRECT3DTEXTURE9* out) {
    const unsigned char* bytes = (const unsigned char*)data;
    DdsInfo dds;
    if (parseDds(bytes, size, dds)) {
        if (dds.cube || dds.volume) { std::fprintf(stderr, "D3DX: cube or volume DDS passed to a 2D loader\n"); return D3DERR_INVALIDCALL; }
        HRESULT hr = dev->CreateTexture(dds.w, dds.h, dds.mips, 0, dds.fmt, D3DPOOL_MANAGED, out, nullptr);
        if (FAILED(hr)) return hr;
        const unsigned char* p = dds.pixels;
        UINT w = dds.w, h = dds.h;
        for (UINT l = 0; l < dds.mips; ++l) {
            const UINT n = mipBytes(dds.fmt, w, h);
            D3DLOCKED_RECT r;
            if (SUCCEEDED((*out)->LockRect(l, &r, nullptr, 0))) {
                const UINT rows = isBlock(dds.fmt) ? (h + 3) / 4 : h, line = n / rows;
                for (UINT y = 0; y < rows; ++y) std::memcpy((unsigned char*)r.pBits + y * r.Pitch, p + y * line, line);
                (*out)->UnlockRect(l);
            }
            p += n; w = std::max(1u, w / 2); h = std::max(1u, h / 2);
        }
        return D3D_OK;
    }
    int w, h, c;
    unsigned char* rgba = stbi_load_from_memory(bytes, (int)size, &w, &h, &c, 4);
    if (!rgba) return D3DERR_INVALIDCALL;
    HRESULT hr = dev->CreateTexture(w, h, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, out, nullptr);
    if (SUCCEEDED(hr)) {
        D3DLOCKED_RECT r;
        if (SUCCEEDED((*out)->LockRect(0, &r, nullptr, 0))) {
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                const unsigned char* s = rgba + 4 * (y * w + x);
                unsigned char* t = (unsigned char*)r.pBits + y * r.Pitch + x * 4;
                t[0] = s[2]; t[1] = s[1]; t[2] = s[0]; t[3] = s[3];
            }
            (*out)->UnlockRect(0);
        }
        D3DXFilterTexture(*out, nullptr, 0, D3DX_DEFAULT);
    }
    stbi_image_free(rgba);
    return hr;
}
}
