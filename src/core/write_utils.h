#pragma once
#include <core/macros.h>
#include <cstdint>

template<typename TBase, typename TField>
inline char* EmitShortOffset(char* pData, TBase* base, TField& field) {
    ptrdiff_t delta = pData - reinterpret_cast<char*>(base);
    delta = (delta + 1) & ~(ptrdiff_t)1;           // 2-byte align
    if (delta > 0xFFFE)
        delta = (delta + 31) & ~(ptrdiff_t)31;     // 32-byte align for large offsets
    pData = reinterpret_cast<char*>(base) + delta;
    field = static_cast<TField>(delta <= 0xFFFE ? delta : ((delta >> 4) | 1));
    return pData;
}

template<typename TBase, typename TField>
inline void EncodeShortOffset(TBase* base, char* target, TField& field) {
    const ptrdiff_t delta = target - reinterpret_cast<char*>(base);
    AssertMsg((delta & 1) == 0, "EncodeShortOffset: target is not 2-byte aligned");
    field = static_cast<TField>(delta <= 0xFFFE ? delta : ((delta >> 4) | 1));
}
