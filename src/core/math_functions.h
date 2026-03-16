#pragma once
#include <vector>
#include <core/math_types.h>

bool allEqualVector(const std::vector<Vector3>& v, size_t start, size_t end);
bool allEqualVector(const std::vector<Vector3>& v, size_t start, size_t end, int axis);
bool allEqualVector(const std::vector<Vector3>& v, size_t start, size_t end, int axis, float scale);
bool allEqualVector(const std::vector<Vector4>& v, size_t start, size_t end, int axis, float scale);
void findMinMaxSIMD(const std::vector<Vector3>& v, size_t start, size_t end, Vector3& minOut, Vector3& maxOut);
void findMinMaxSIMD(const std::vector<Vector4>& v, size_t start, size_t end, Vector4& minOut, Vector4& maxOut);
