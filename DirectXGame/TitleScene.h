#pragma once
#include "kamataEngine.h"
#include "Fade.h"
#include "Skydome.h"

class TitleScene {

public:
	// シーンのフェーズ
	enum class Phase {
		kFadeIn,  // フェードイン
		kMain,    // メイン部
		kFadeOut, // フェードアウト
	};

	~TitleScene();

	void Initialize();
	void Update();
	void Draw();

	// 終了フラグのgetter
	bool IsFinished() const { return finished_; }

private:
	// 終了フラグ
	bool finished_ = false;

	// 現在のフェーズ
	Phase phase_ = Phase::kFadeIn;

	// --- カメラ ---
	KamataEngine::Camera camera_;

	// --- 天球 ---
	KamataEngine::Model* modelSkydome_ = nullptr;
	Skydome* skydome_ = nullptr;

	// --- タイトルロゴ（3Dモデル） ---
	// "titleFont" モデル（Resources/titleFont フォルダ）を表示する。
	KamataEngine::Model* modelTitleLogo_ = nullptr;
	KamataEngine::WorldTransform worldTransformTitleLogo_;

	// --- "PRESS START" 的な表示（3Dモデル） ---
	// "startFont" モデル（Resources/startFont フォルダ）を、タイトルロゴの下に表示する。
	KamataEngine::Model* modelStartFont_ = nullptr;
	KamataEngine::WorldTransform worldTransformStartFont_;

	// --- 3機体（プレイヤーモデル）のぷかぷか表示 ---
	// heavyBot・speedBot・balancer を並べて浮遊させる（インデックス0=ヘヴィ, 1=スピード, 2=バランス）
	static inline const int32_t kNumCharacters = 3;
	KamataEngine::Model* modelCharacters_[kNumCharacters] = {nullptr, nullptr, nullptr};
	KamataEngine::WorldTransform worldTransformCharacters_[kNumCharacters];

	// 揺れ・回転アニメーション用の経過時間
	float counter_ = 0.0f;

	// --- フェード ---
	Fade* fade_ = nullptr;
};
