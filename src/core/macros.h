#pragma once
#include <cstdint>

#define Error(fmt, ...) \
	do { \
		printf("[!] Error (%s:%d): " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
		throw std::runtime_error(fmt); \
	} while(0)

#define Assert(expr) \
	do { \
		if (!(expr)) { \
			printf("Assertion failed: '%s'\n", #expr); \
			printf("  %s (%d)\n", __FILE__, __LINE__); \
			throw std::runtime_error("Assertion failed: " #expr); \
		} \
	} while(0)

#define AssertMsg(expr, fmt, ...) \
	do { \
		if (!(expr)) { \
			printf("Assertion failed: '%s'\n", #expr); \
			printf("  %s (%d)\n", __FILE__, __LINE__); \
			printf("  " fmt "\n", ##__VA_ARGS__); \
			throw std::runtime_error("Assertion failed: " #expr); \
		} \
	} while(0)

// ============================================================================
//  Pointer / Offset Macros
// ============================================================================

#define STRING_FROM_IDX(base, idx) reinterpret_cast<const char*>((char*)base + idx)
#define PTR_FROM_IDX(type, base, idx) reinterpret_cast<type*>((char*)base + idx)
#define OFFSET(x) static_cast<uint32_t>((x & 0xFFFE) << (4 * (x & 1)))
#define SHORTOFFSET(base, target) (\
	(uint16_t)(\
		((const char*)(target)-(const char*)(base)) <= 0xFFFE ? \
		((const char*)(target)-(const char*)(base)) : \
		((((const char*)(target)-(const char*)(base)) >> 4) | 1) \
		) \
	); AssertMsg((((uintptr_t)target & 1) != 0), "SHORT_OFFSET_ERROR") \

// ============================================================================
//  Alignment Macros
// ============================================================================

#define ALIGN2( a )  a = (char *)((__int64)((char *)a + 1) & ~ 1)
#define ALIGN4( a )  a = (char *)((__int64)((char *)a + 3) & ~ 3)
#define ALIGN16( a ) a = (char *)((__int64)((char *)a + 15) & ~ 15)
#define ALIGN32( a ) a = (char *)((__int64)((char *)a + 31) & ~ 31)
#define ALIGN64( a ) a = (char *)((__int64)((char *)a + 63) & ~ 63)

// ============================================================================
//  Constants
// ============================================================================

constexpr auto M_PI = 3.14159265358979323846;
constexpr auto M_PI2 = M_PI / 2.f;
