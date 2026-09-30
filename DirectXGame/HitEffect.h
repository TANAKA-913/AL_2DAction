#pragma once

#include "kamataEngine.h"
#include <array>

/// <summary>
/// ヒット演出用エフェクト
/// </summary>
class HitEffect {

public:
	// --- 生成 ---
	// インスタンス生成と初期化を同時に行う（static factory）
	static HitEffect* Create(const KamataEngine::Vector3& position);

	// --- 静的セッター（全インスタンス共通のモデル・カメラを設定する） ---
	static void SetModel(KamataEngine::Model* model) { model_ = model; }
	static void SetCamera(KamataEngine::Camera* camera) { camera_ = camera; }

	void Initialize(const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	// デスフラグの取得（状態遷移の一種「デス状態」をデスフラグの代わりに使う）
	bool IsDead() const { return state_ == State::kDead; }

private:
	// --- ふるまい（状態） ---
	enum class State {
		kSpread, // 広がる
		kFade,   // 消える
		kDead,   // 消滅済み
	};

	// 楕円の個数
	static inline const uint32_t kNumEllipses = 2;

	// スプレッド・フェードの所要時間[秒]
	static inline const float kSpreadTime = 0.15f;
	static inline const float kFadeTime   = 0.35f;

	// 楕円の幅・長さ（円を細長く潰した比率）
	static inline const float kEllipseWidth  = 1.6f;
	static inline const float kEllipseLength = 0.5f;

	// モデル（借りてくる用）
	static KamataEngine::Model* model_;
	// カメラ（借りてくる用）
	static KamataEngine::Camera* camera_;

	// 円のワールドトランスフォーム
	KamataEngine::WorldTransform circleWorldTransform_;
	// 楕円のワールドトランスフォーム
	std::array<KamataEngine::WorldTransform, kNumEllipses> ellipseWorldTransforms_;

	// 色変更オブジェクト
	KamataEngine::ObjectColor objectColor_;
	// 色の数値（アルファ値をフェードに使う）
	KamataEngine::Vector4 color_;

	// 現在の状態
	State state_ = State::kSpread;
	// 状態ごとの経過時間カウンター
	float counter_ = 0.0f;
};
