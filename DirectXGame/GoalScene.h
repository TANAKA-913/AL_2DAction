#pragma once
#include "kamataEngine.h"
#include "Fade.h"
#include "Skydome.h"

/// <summary>
/// ゴールシーン
/// "gameclearFont" モデル（Resources/gameclearFont フォルダ）を表示する。
/// </summary>
class GoalScene {

public:
	// シーンのフェーズ
	enum class Phase {
		kFadeIn,  // フェードイン
		kMain,    // メイン部
		kFadeOut, // フェードアウト
	};

	~GoalScene();

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

	// --- ゴール表示用（3Dモデル） ---
	KamataEngine::Model* modelGoal_ = nullptr;
	KamataEngine::WorldTransform worldTransformGoal_;

	// --- "RESTART" 的な表示（3Dモデル） ---
	// "restartFont" モデル（Resources/restartFont フォルダ）を、ゴール表示の下に表示する。
	KamataEngine::Model* modelRestartFont_ = nullptr;
	KamataEngine::WorldTransform worldTransformRestartFont_;

	// 揺れ・回転アニメーション用の経過時間
	float counter_ = 0.0f;

	// --- フェード ---
	Fade* fade_ = nullptr;
};
