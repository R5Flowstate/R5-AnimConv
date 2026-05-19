#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstdio>

#include <core/macros.h>

// ============================================================================
//  Data Types
// ============================================================================

struct Vector3 {
	float x, y, z;

	inline Vector3 operator + (Vector3& a) { return { x + a.x, y + a.y, z + a.z }; }
	inline Vector3 operator + (Vector3 a) { return { x + a.x, y + a.y, z + a.z }; }
	inline Vector3 operator - (Vector3 a) { return { x - a.x, y - a.y, z - a.z }; }
	inline Vector3 operator + (int& a) { return { x + a, y + a, z + a }; }
	inline Vector3 operator + (int a) { return { x + a, y + a, z + a }; }
	inline Vector3 operator * (int& a) { return { x * a, y * a, z * a }; }
	inline Vector3 operator * (int a) { return { x * a, y * a, z * a }; }
	inline Vector3 operator*(Vector3& a) { return { x * a.x, y * a.y, z * a.z }; }
	inline bool operator==(Vector3 a) { return (x == a.x && y == a.y && z == a.z); }
	inline bool operator!=(Vector3 a) { return (x != a.x || y != a.y || z != a.z); }

	inline void operator += (Vector3 a) { x += a.x; y += a.y; z += a.z; }
	inline void operator += (float a)   { x += a; y += a; z += a; }
	inline void operator -= (Vector3 a) { x -= a.x; y -= a.y; z -= a.z; }
	inline void operator -= (float a)   { x -= a; y -= a; z -= a; }
	inline void operator *= (float a)   { x *= a; y *= a; z *= a; }

	inline float& operator[](int i) { return ((float*)this)[i]; }
	inline float operator[](int i) const { return ((float*)this)[i]; }

	inline float Min() const { return std::min({ x, y, z }); }
	inline float Max() const { return std::max({ x, y, z }); }

	bool approx_equal(const Vector3 other, float eps = 1e-6f) const {
		return std::fabs(x - other.x) < eps &&
			std::fabs(y - other.y) < eps &&
			std::fabs(z - other.z) < eps;
	}
};

struct Vector4 {
	float x, y, z, w;

	inline float& operator[](int i) { return ((float*)this)[i]; }
	inline float operator[](int i) const { return ((float*)this)[i]; }
};

struct Vector48 {
	int16_t x, y, z;
};

struct Vector64 {
	uint64_t x : 21;
	uint64_t y : 21;
	uint64_t z : 22;
};

struct Matrix3x4_t {
	float m[3][4];

	float* operator[](int i) { return m[i]; }
	const float* operator[](int i) const { return m[i]; }
};

void QuaternionRads(const struct Quaternion& q, Vector3& angles);

struct Quaternion {
	float x, y, z, w;

	inline Vector3 ToRad() {
		Vector3 angles;
		QuaternionRads(*this, angles);
		return angles;
	}
};

struct Quaternion64 {
	uint64_t x : 21;
	uint64_t y : 21;
	uint64_t z : 21;
	uint64_t wneg : 1;

	inline Quaternion64& operator=(const Quaternion64& other) {
		this->x = other.x;
		this->y = other.y;
		this->z = other.z;
		this->wneg = other.wneg;
		return *this;
	}
};

// ============================================================================
//  Encode / Decode
// ============================================================================

float HalfToFloat(const uint16_t h);
uint16_t FloatToHalf(float value);

inline float DecodeQuat64(uint64_t v1) {
	return (static_cast<int>(v1) - 1048576) * (1 / 1048576.5f);
}

inline uint64_t EncodeQuat64(float v1) {
	return std::clamp(static_cast<int>(v1 * 1048576.f) + 1048576, 0, 2097151);
}

// ============================================================================
//  Pack / Unpack
// ============================================================================

// --- Vector48 ---

inline Vector48 Pack48(Vector3 a) {
	Vector48 out{};
	out.x = FloatToHalf(a.x);
	out.y = FloatToHalf(a.y);
	out.z = FloatToHalf(a.z);
	return out;
}

inline Vector3 Unpack48(Vector48 a) {
	Vector3 out{};
	out.x = HalfToFloat(static_cast<uint16_t>(a.x));
	out.y = HalfToFloat(static_cast<uint16_t>(a.y));
	out.z = HalfToFloat(static_cast<uint16_t>(a.z));
	return out;
}

// --- Quaternion64 ---

inline Quaternion64 PackQuat64(Quaternion q) {
	Quaternion64 q64;
	q64.x = EncodeQuat64(q.x);
	q64.y = EncodeQuat64(q.y);
	q64.z = EncodeQuat64(q.z);
	q64.wneg = (q.w < 0.0f) ? 1 : 0;
	return q64;
}

inline Quaternion UnpackQuat64(Quaternion64 q64) {
	Quaternion q;
	q.x = DecodeQuat64(q64.x);
	q.y = DecodeQuat64(q64.y);
	q.z = DecodeQuat64(q64.z);

	const float dprem = 1.0f - ((q.x * q.x) + (q.y * q.y) + (q.z * q.z));
	q.w = sqrtf(dprem < 0.0f ? -dprem : dprem);

	if (q64.wneg)
		q.w = -q.w;

	return q;
}

// ============================================================================
//  Forward Declarations (implemented in math_helper.cpp)
// ============================================================================

void SinCos(float radians, float* sine, float* cosine);
void QuaternionMatrix(const Quaternion& q, Matrix3x4_t& matrix);
void MatrixRads(const Matrix3x4_t& matrix, float* angles);
void AngleQuaternion(Vector3 angles, Quaternion& outQuat);
uint64_t StringToGuid(const char* string);
