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
//  Constants
// ============================================================================

constexpr auto M_PI = 3.14159265358979323846;
constexpr auto M_PI2 = M_PI / 2.f;
