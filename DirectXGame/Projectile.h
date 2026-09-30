#pragma once
#include "kamataEngine.h"
#include "AABB.h"

/// <summary>
/// 遠距離攻撃用の弾（ヘヴィボット専用）
/// 発射地点から一定速度で直進し、一定距離飛んだ、または敵に当たったら消える。
/// </summary>
class Projectile {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="model">表示に使うモデル（attackEffectモデルを流用）</param>
	/// <param name="position">発射座標</param>
	/// <param name="facingRight">true: 右方向へ発射 / false: 左方向へ発射</param>
	/// <param name="range">飛距離（マス数）</param>
	void Initialize(KamataEngine::Model* model, const KamataEngine::Vector3& position, bool facingRight, float range);

	void Update();
	void Draw(const KamataEngine::Camera& camera);

	// 消滅フラグのgetter（trueならGameScene側で削除する）
	bool IsDead() const { return isDead_; }

	// 弾を消す（敵に当たった時など、外部から呼ぶ）
	void Kill() { isDead_ = true; }

	AABB GetAABB() const;
	KamataEngine::Vector3 GetWorldPosition() const;

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;

	// 進行方向（+1: 右、-1: 左）
	float direction_ = 1.0f;
	// 飛んだ距離の合計
	float traveledDistance_ = 0.0f;
	// 最大飛距離（これを超えたら消える）
	float maxRange_ = 6.0f;

	bool isDead_ = false;

	// 弾の速度（1フレームあたりの移動量）
	static inline const float kSpeed = 0.35f;
	// 当たり判定サイズ
	static inline const float kWidth  = 0.3f;
	static inline const float kHeight = 0.3f;
};
