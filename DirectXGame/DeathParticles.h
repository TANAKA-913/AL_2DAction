#pragma once

#include "kamataEngine.h"
#include <array>
#include <numbers>

class DeathParticles {

public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	// 敵・プレイヤーのダッシュと共通の背後トレイルモデル（"jetTrail"）をセットする
	// （未設定でも動作するが、その場合はこちらの追加パーティクルは出ない）
	void SetTrailModel(KamataEngine::Model* model) { modelTrail_ = model; }

	// 演出の色をセットする（機体ごとに変える。デフォルトは白）
	// ※ Initialize()より前に呼んでも後に呼んでも良い
	void SetColor(const KamataEngine::Vector4& color) { baseColor_ = color; }

	// 全パーティクルの演出が終わったか
	bool IsFinished() const { return isFinished_; }

private:
	// --- パーティクルの設定 ---
	// パーティクルの数（本体の球）
	static inline const uint32_t kNumParticles = 8;
	// パーティクルの数（トレイル用。敵・ダッシュと共通のモデルを使う分）
	static inline const uint32_t kNumTrailParticles = 6;
	// 消えるまでの時間[秒]
	static inline const float kDuration = 2.0f;
	// 外側へ広がる速さ（半径が1秒あたりに伸びる量）
	static inline const float kRadiusGrowthRate = 1.5f;
	// 渦を巻く速さ（1秒あたりの回転角[ラジアン]）
	static inline const float kRotationSpeed = 6.0f;
	// パーティクル1個あたりの角度
	static inline const float kAngleUnit = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kNumParticles);
	// トレイルパーティクル1個あたりの角度（本体の球とずらして配置する）
	static inline const float kTrailAngleUnit = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kNumTrailParticles);
	// トレイルパーティクルの大きさ
	static inline const float kTrailScale = 0.7f;

	KamataEngine::Model* model_      = nullptr;
	KamataEngine::Model* modelTrail_ = nullptr;
	KamataEngine::Camera* camera_    = nullptr;

	// 発生座標（渦の中心）
	KamataEngine::Vector3 basePosition_ = {};

	// ワールドトランスフォーム（本体の球）
	std::array<KamataEngine::WorldTransform, kNumParticles> worldTransforms_;
	// ワールドトランスフォーム（トレイル用。敵・ダッシュと共通のモデル）
	std::array<KamataEngine::WorldTransform, kNumTrailParticles> trailWorldTransforms_;

	// 色変更オブジェクト（本体・トレイルで共通の色を使う）
	KamataEngine::ObjectColor objectColor_;
	// 基準となる色（機体ごとに変える。SetColor()で上書き可能）
	KamataEngine::Vector4 baseColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	// 実際に適用する色（アルファはフェードアウトで毎フレーム変化する）
	KamataEngine::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};

	// 経過時間カウンター
	float counter_ = 0.0f;
	// 終了フラグ
	bool isFinished_ = false;
};
