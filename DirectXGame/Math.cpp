#include "Math.h"
#include <cmath>

using namespace KamataEngine;

// 行列の乗算
Matrix4x4 Math::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += m1.m[i][k] * m2.m[k][j];
			}
		}
	}
	return result;
}

Matrix4x4 Math::MakeIdentity4x4() {
	Matrix4x4 result = {0};
	for (int i = 0; i < 4; ++i)
		result.m[i][i] = 1.0f;
	return result;
}

Matrix4x4 Math::MakeScaleMatrix(const Vector3& s) {
	Matrix4x4 res = {0};
	res.m[0][0] = s.x;
	res.m[1][1] = s.y;
	res.m[2][2] = s.z;
	res.m[3][3] = 1.0f;
	return res;
}

Matrix4x4 Math::MakeRotateXMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[1][1] = std::cos(rad);
	res.m[1][2] = std::sin(rad);
	res.m[2][1] = -std::sin(rad);
	res.m[2][2] = std::cos(rad);
	return res;
}

Matrix4x4 Math::MakeRotateYMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = std::cos(rad);
	res.m[0][2] = -std::sin(rad);
	res.m[2][0] = std::sin(rad);
	res.m[2][2] = std::cos(rad);
	return res;
}

Matrix4x4 Math::MakeRotateZMatrix(float rad) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[0][0] = std::cos(rad);
	res.m[0][1] = std::sin(rad);
	res.m[1][0] = -std::sin(rad);
	res.m[1][1] = std::cos(rad);
	return res;
}

Matrix4x4 Math::MakeTranslateMatrix(const Vector3& t) {
	Matrix4x4 res = MakeIdentity4x4();
	res.m[3][0] = t.x;
	res.m[3][1] = t.y;
	res.m[3][2] = t.z;
	return res;
}

// 回転合成
Matrix4x4 Math::MakeRotationMatrix(const Vector3& r) { return Multiply(Multiply(MakeRotateZMatrix(r.z), MakeRotateXMatrix(r.x)), MakeRotateYMatrix(r.y)); }

// アフィン行列生成（ここでエラーC2084が出ないよう、これ1つに絞る）
Matrix4x4 Math::MakeAffineMatrix(const Vector3& s, const Vector3& r, const Vector3& t) {
	Matrix4x4 S = MakeScaleMatrix(s);
	Matrix4x4 R = MakeRotationMatrix(r);
	Matrix4x4 T = MakeTranslateMatrix(t);
	return Multiply(Multiply(S, R), T);
}

// 座標変換（w=1として計算し、wで正規化する）
Vector3 Math::Transform(const Vector3& v, const Matrix4x4& m) {
	Vector3 result;
	result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
	result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
	result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
	float w  = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
	if (w != 0.0f) {
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}
	return result;
}