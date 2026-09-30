#pragma once
#include "kamataEngine.h"

// Mathクラス：すべての計算をこのクラスに集約します
class Math {
public:
	// エンジンの型を使用
	using Vector3 = KamataEngine::Vector3;
	using Matrix4x4 = KamataEngine::Matrix4x4;

	// --- 基本行列演算 ---
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
	static Matrix4x4 MakeIdentity4x4();

	// --- アフィン変換用行列生成 ---
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	static Matrix4x4 MakeRotateXMatrix(float radian);
	static Matrix4x4 MakeRotateYMatrix(float radian);
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// 回転の合成とアフィン行列の生成
	static Matrix4x4 MakeRotationMatrix(const Vector3& rotate);
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// 座標変換（Vector3をMatrix4x4で変換する）
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
};