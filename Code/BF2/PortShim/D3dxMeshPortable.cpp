// [CHOSEN, not recovered] D3DX shape meshes for builds without d3dx9_25.dll. The light manager only
// takes their vertex and index buffers, so a mesh here is a position-only indexed triangle list.
// Vertex order may differ from D3DX; the surfaces match. Other ID3DXMesh entry points stay frontiers.
// [RETIRE-WHEN: portable-d3dx] Remove with a native renderer path that no longer uses D3DX.

#include <d3d9.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>

namespace {

struct Vertex { float x, y, z; };

// The interface prefix the renderer consumes; see RendD3dxMesh in XpLightManager.cpp.
struct MeshPrefix : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE DrawSubset(DWORD) = 0;
    virtual DWORD STDMETHODCALLTYPE GetNumFaces() = 0;
    virtual DWORD STDMETHODCALLTYPE GetNumVertices() = 0;
    virtual DWORD STDMETHODCALLTYPE GetFVF() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9*) = 0;
    virtual DWORD STDMETHODCALLTYPE GetNumBytesPerVertex() = 0;
    virtual DWORD STDMETHODCALLTYPE GetOptions() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CloneMeshFVF(DWORD, DWORD, IDirect3DDevice9*, MeshPrefix**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CloneMesh(DWORD, const D3DVERTEXELEMENT9*, IDirect3DDevice9*, MeshPrefix**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVertexBuffer(IDirect3DVertexBuffer9**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndexBuffer(IDirect3DIndexBuffer9**) = 0;
};

[[noreturn]] void stop(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] portable ID3DXMesh::%s reached\n", name);
    std::fflush(stderr);
    std::abort();
}

class ShapeMesh final : public MeshPrefix
{
public:
    ShapeMesh(IDirect3DDevice9* device, const std::vector<Vertex>& vertices,
              const std::vector<unsigned short>& indices)
        : m_device(device), m_vertices(static_cast<DWORD>(vertices.size())),
          m_faces(static_cast<DWORD>(indices.size() / 3))
    {
        void* data = nullptr;
        const UINT vertexBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
        const UINT indexBytes = static_cast<UINT>(indices.size() * 2);
        device->CreateVertexBuffer(vertexBytes, 0, D3DFVF_XYZ, D3DPOOL_MANAGED, &m_vb, nullptr);
        m_vb->Lock(0, vertexBytes, &data, 0);
        std::memcpy(data, vertices.data(), vertexBytes);
        m_vb->Unlock();
        device->CreateIndexBuffer(indexBytes, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &m_ib, nullptr);
        m_ib->Lock(0, indexBytes, &data, 0);
        std::memcpy(data, indices.data(), indexBytes);
        m_ib->Unlock();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { stop("QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG left = --m_refs;
        if (!left)
        {
            m_vb->Release();
            m_ib->Release();
            delete this;
        }
        return left;
    }
    HRESULT STDMETHODCALLTYPE DrawSubset(DWORD) override { stop("DrawSubset"); }
    DWORD STDMETHODCALLTYPE GetNumFaces() override { return m_faces; }
    DWORD STDMETHODCALLTYPE GetNumVertices() override { return m_vertices; }
    DWORD STDMETHODCALLTYPE GetFVF() override { return D3DFVF_XYZ; }
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9*) override { stop("GetDeclaration"); }
    DWORD STDMETHODCALLTYPE GetNumBytesPerVertex() override { return sizeof(Vertex); }
    DWORD STDMETHODCALLTYPE GetOptions() override { return 0; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override
    {
        m_device->AddRef();
        *out = m_device;
        return S_OK;
    }
    // Every shape is already position-only, which is the only format the renderer clones to.
    HRESULT STDMETHODCALLTYPE CloneMeshFVF(DWORD, DWORD fvf, IDirect3DDevice9*, MeshPrefix** out) override
    {
        if (fvf != D3DFVF_XYZ) stop("CloneMeshFVF");
        AddRef();
        *out = this;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CloneMesh(DWORD, const D3DVERTEXELEMENT9*, IDirect3DDevice9*, MeshPrefix**) override
    {
        stop("CloneMesh");
    }
    HRESULT STDMETHODCALLTYPE GetVertexBuffer(IDirect3DVertexBuffer9** out) override
    {
        m_vb->AddRef();
        *out = m_vb;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetIndexBuffer(IDirect3DIndexBuffer9** out) override
    {
        m_ib->AddRef();
        *out = m_ib;
        return S_OK;
    }

private:
    ULONG m_refs = 1;
    IDirect3DDevice9* m_device;
    IDirect3DVertexBuffer9* m_vb = nullptr;
    IDirect3DIndexBuffer9* m_ib = nullptr;
    DWORD m_vertices, m_faces;
};

const float kPi = 3.14159265358979f;

void quad(std::vector<unsigned short>& out, unsigned a, unsigned b, unsigned c, unsigned d)
{
    const unsigned order[6] = {a, b, c, c, b, d};
    for (unsigned v : order) out.push_back(static_cast<unsigned short>(v));
}

// Rings of `slices + 1` vertices from bottom to top, closed with one fan at each end.
long buildRings(IDirect3DDevice9* device, const std::vector<std::pair<float, float>>& rings,
                unsigned slices, MeshPrefix** mesh)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned short> indices;
    const unsigned ringCount = static_cast<unsigned>(rings.size());
    for (const std::pair<float, float>& ring : rings)
        for (unsigned s = 0; s <= slices; ++s)
        {
            const float angle = 2.0f * kPi * static_cast<float>(s) / static_cast<float>(slices);
            vertices.push_back({ring.first * std::cos(angle), ring.first * std::sin(angle), ring.second});
        }
    for (unsigned r = 0; r + 1 < ringCount; ++r)
        for (unsigned s = 0; s < slices; ++s)
        {
            const unsigned a = r * (slices + 1) + s, c = a + slices + 1;
            quad(indices, a, c, a + 1, c + 1);
        }
    const unsigned first = static_cast<unsigned>(vertices.size());
    vertices.push_back({0.0f, 0.0f, rings.front().second});
    vertices.push_back({0.0f, 0.0f, rings.back().second});
    const unsigned top = (ringCount - 1) * (slices + 1);
    for (unsigned s = 0; s < slices; ++s)
    {
        const unsigned tri[6] = {first, s, s + 1, first + 1, top + s + 1, top + s};
        for (unsigned v : tri) indices.push_back(static_cast<unsigned short>(v));
    }
    *mesh = new ShapeMesh(device, vertices, indices);
    return S_OK;
}

} // namespace

extern "C" long D3DXCreateSphere(IDirect3DDevice9* device, float radius, unsigned slices,
                                 unsigned stacks, MeshPrefix** mesh, void** adjacency)
{
    if (adjacency) *adjacency = nullptr;
    std::vector<std::pair<float, float>> rings;
    for (unsigned i = 0; i <= stacks; ++i)
    {
        const float angle = -0.5f * kPi + kPi * static_cast<float>(i) / static_cast<float>(stacks);
        rings.push_back({radius * std::cos(angle), radius * std::sin(angle)});
    }
    return buildRings(device, rings, slices, mesh);
}

extern "C" long D3DXCreateCylinder(IDirect3DDevice9* device, float radius1, float radius2,
                                   float length, unsigned slices, unsigned stacks,
                                   MeshPrefix** mesh, void** adjacency)
{
    if (adjacency) *adjacency = nullptr;
    std::vector<std::pair<float, float>> rings;
    for (unsigned i = 0; i <= stacks; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(stacks);
        rings.push_back({radius1 + (radius2 - radius1) * t, length * (t - 0.5f)});
    }
    return buildRings(device, rings, slices, mesh);
}

extern "C" long D3DXCreateBox(IDirect3DDevice9* device, float width, float height, float depth,
                              MeshPrefix** mesh, void** adjacency)
{
    if (adjacency) *adjacency = nullptr;
    const float x = width / 2, y = height / 2, z = depth / 2;
    const std::vector<Vertex> vertices = {{-x, -y, -z}, {x, -y, -z}, {-x, y, -z}, {x, y, -z},
                                          {-x, -y, z},  {x, -y, z},  {-x, y, z},  {x, y, z}};
    std::vector<unsigned short> indices;
    quad(indices, 0, 2, 1, 3);
    quad(indices, 5, 7, 4, 6);
    quad(indices, 4, 6, 0, 2);
    quad(indices, 1, 3, 5, 7);
    quad(indices, 2, 6, 3, 7);
    quad(indices, 4, 0, 5, 1);
    *mesh = new ShapeMesh(device, vertices, indices);
    return S_OK;
}

// [TEMP] [CHOSEN, not recovered] Android resolves every import at load, so the terrain's editor-only
// mesh entry needs a body. [RETIRE-WHEN: portable-d3dx]
extern "C" long __stdcall D3DXCreateMesh(unsigned long, unsigned long, unsigned long, const void*,
                                         void*, void**)
{
    std::fprintf(stderr, "[FRONTIER] D3DXCreateMesh reached\n");
    std::fflush(stderr);
    std::abort();
}
