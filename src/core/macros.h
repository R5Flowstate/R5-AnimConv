#pragma once
#include <cstdint>

// ============================================================================
//  Pointer / Offset Macros
// ============================================================================

#define STRING_FROM_IDX(base, idx) reinterpret_cast<const char*>((char*)base + idx)
#define PTR_FROM_IDX(type, base, idx) reinterpret_cast<type*>((char*)base + idx)
#define OFFSET(x) static_cast<uint32_t>((x & 0xFFFE) << (4 * (x & 1)))

// ============================================================================
//  Alignment Macros
// ============================================================================

#define ALIGN2( a )  a = (char *)((__int64)((char *)a + 1) & ~ 1)
#define ALIGN4( a )  a = (char *)((__int64)((char *)a + 3) & ~ 3)
#define ALIGN16( a ) a = (char *)((__int64)((char *)a + 15) & ~ 15)

// ============================================================================
//  Error Handling
// ============================================================================

#define PRINTANDTHROW(at, msg) { \
	printf("[!] Error at %s\n", at); \
	printf("--> %s\n", msg); \
	throw std::runtime_error(msg); \
}

// ============================================================================
//  Constants
// ============================================================================

constexpr auto M_PI = 3.14159265358979323846;
