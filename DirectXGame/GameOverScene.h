#pragma once
#include "kamataEngine.h"
#include "Fade.h"
#include "Skydome.h"

/// <summary>
/// ゲームオーバーシーン
/// "gameoverFont" モデル（Resources/gameoverFont フォルダ）を表示する。
/// </summary>
class GameOverScene {

public:
	// シーンのフェーズ
	enum class Phase {
		kFadeIn,  // フェードイン
		kMain,    // メイン部
		kFadeOut, // フェードアウト
	};

	~GameOverScene();

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

	// --- ゲームオーバー表示用（3Dモデル） ---
	KamataEngine::Model* modelGameOver_ = nullptr;
	KamataEngine::WorldTransform worldTransformGameOver_;

	// --- "RESTART" 的な表示（3Dモデル） ---
	// "restartFont" モデル（Resources/restartFont フォルダ）を、ゲームオーバー表示の下に表示する。
	KamataEngine::Model* modelRestartFont_ = nullptr;
	KamataEngine::WorldTransform worldTransformRestartFont_;

	// 赤い半透明オーバーレイ（ゲームオーバーらしい雰囲気を出すための演出）
	KamataEngine::Sprite* spriteOverlay_ = nullptr;

	// 揺れ・回転アニメーション用の経過時間
	float counter_ = 0.0f;

	// --- フェード ---
	Fade* fade_ = nullptr;
};
