#pragma once
// [CHOSEN, not recovered] A portable stand-in for the recovered client's inline x87
// blocks on non-x86 targets. Generated code (tools: asm2cpp.py) drives this model.
// Intermediates are double, not 80-bit extended, so the last bits can differ from retail.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace x87 {
struct Cpu {
    double st[8] = {};
    int top = 0;
    uint16_t cw = 0x027F, sw = 0;
    uintptr_t eax = 0, ecx = 0, edx = 0, edi = 0;
    bool zf = false, pf = false;

    double& s(int i) { return st[(top + i) & 7]; }
    void push(double v) { top = (top + 7) & 7; st[top] = v; }
    double pop() { double v = st[top]; top = (top + 1) & 7; return v; }
    void compare(double a, double b) {
        sw &= ~0x4500;
        if (a != a || b != b) sw |= 0x4500;
        else if (a < b) sw |= 0x0100;
        else if (a == b) sw |= 0x4000;
    }
    void testFlags(uint32_t v) {
        zf = v == 0;
        v &= 0xFF; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
        pf = (v & 1) == 0;
    }
    double round(double v) const {
        switch ((cw >> 10) & 3) {
            case 1: return std::floor(v);
            case 2: return std::ceil(v);
            case 3: return std::trunc(v);
            default: return std::nearbyint(v);
        }
    }
};
template <class T> inline uintptr_t toReg(T* v) { return reinterpret_cast<uintptr_t>(v); }
inline uintptr_t toReg(float v) { uint32_t b; std::memcpy(&b, &v, 4); return b; }
template <class T, class = typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>::type>
inline uintptr_t toReg(T v) { return static_cast<uintptr_t>(static_cast<uint64_t>(v)); }
// MSVC loads the address when `mov reg, name` names a reference to a class.
template <class T, class = typename std::enable_if<std::is_class<T>::value>::type, class = void>
inline uintptr_t toReg(const T& v) { return reinterpret_cast<uintptr_t>(&v); }
template <class T> inline void fromReg(T*& out, uintptr_t v) { out = reinterpret_cast<T*>(v); }
inline void fromReg(float& out, uintptr_t v) { uint32_t b = static_cast<uint32_t>(v); std::memcpy(&out, &b, 4); }
template <class T, class = typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>::type>
inline void fromReg(T& out, uintptr_t v) { out = static_cast<T>(v); }
inline float& f32(uintptr_t a) { return *reinterpret_cast<float*>(a); }
inline uint32_t& u32(uintptr_t a) { return *reinterpret_cast<uint32_t*>(a); }
inline uint8_t& u8(uintptr_t a) { return *reinterpret_cast<uint8_t*>(a); }
}  // namespace x87
