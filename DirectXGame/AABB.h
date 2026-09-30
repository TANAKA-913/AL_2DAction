#pragma once
#include "kamataEngine.h"

// AABB（軸平行境界ボックス）
struct AABB {
	KamataEngine::Vector3 min; //!< 最小点
	KamataEngine::Vector3 max; //!< 最大点
};

// AABBとAABBの衝突判定
bool IsCollision(const AABB& aabb1, const AABB& aabb2);
