// [CHOSEN, not recovered] Shared engine-heap wrappers and modern-MSVC allocation adapters.

#include "BfMemory.h"

#include <cstdlib>
#include <new>

namespace renddx9abi {

void* engineAlloc(unsigned int size)
{
#if !defined(_M_IX86)
    // [CHOSEN, not recovered] Recovered call sites pass retail x86 object sizes. No object more
    // than doubles when its pointers widen, so doubling keeps those sites in bounds.
    return std::calloc(1, static_cast<size_t>(size) * sizeof(void*) / 4 + 16);
#else
    return std::malloc(size);
#endif
}

void engineFree(void* p)
{
    std::free(p);
}

} // namespace renddx9abi

// Route scalar, array and nothrow allocations through the same heap used by engineFree.
// All allocations use the shared CRT heap.
// Throwing new reports allocation failure with bad_alloc even though the original allocator can return null.

void* operator new(size_t size)
{
    void* p = std::malloc(size ? size : 1);
    if (!p)
        throw std::bad_alloc();
    return p;
}

void* operator new[](size_t size)
{
    return ::operator new(size);
}

void* operator new(size_t size, const std::nothrow_t&) throw()
{
    return std::malloc(size ? size : 1);
}

void* operator new[](size_t size, const std::nothrow_t&) throw()
{
    return std::malloc(size ? size : 1);
}

void operator delete(void* p) throw()
{
    if (p)
        renddx9abi::engineFree(p);
}

void operator delete[](void* p) throw()
{
    ::operator delete(p);
}

void operator delete(void* p, const std::nothrow_t&) throw()
{
    ::operator delete(p);
}

void operator delete[](void* p, const std::nothrow_t&) throw()
{
    ::operator delete(p);
}

// C++14 sized deallocation uses the same shared-CRT path as the unsized forms.
void operator delete(void* p, size_t) throw()
{
    ::operator delete(p);
}

void operator delete[](void* p, size_t) throw()
{
    ::operator delete(p);
}
