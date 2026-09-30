#pragma once
#include "kamataEngine.h"
#include "Fade.h"
#include "CharacterData.h"
#include "Skydome.h"

/// <summary>
/// キャラクター（機体）選択シーン
/// heavyBot・speedBot・balancer の3体分の専用モデルを表示する。
/// </summary>
class CharacterSelectScene {

public:
	// シーンのフェーズ
	enum class Phase {
		kFadeIn,  // フェードイン
		kMain,    // メイン部（機体選択操作）
		kFadeOut, // フェードアウト
	};

	~CharacterSelectScene();

	void Initialize();
	void Update();
	void Draw();

	// 終了フラグのgetter
	bool IsFinished() const { return finished_; }

	// 決定された機体タイプのgetter
	CharacterType GetSelectedCharacter() const { return static_cast<CharacterType>(selectedIndex_); }

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

	// --- 機体表示用（3Dモデル） ---
	// 3体それぞれの専用モデル（インデックスはCharacterTypeと対応：0=ヘヴィ, 1=スピード, 2=バランス）
	static inline const int32_t kNumCharacters = kCharacterTypeCount;
	KamataEngine::Model* modelCharacters_[kNumCharacters] = {nullptr, nullptr, nullptr};

	// 3体ぶんのワールドトランスフォーム・色
	KamataEngine::WorldTransform worldTransformCharacters_[kNumCharacters];
	KamataEngine::ObjectColor    objectColorCharacters_[kNumCharacters];

	// --- 選択カーソル用スプライト ---
	KamataEngine::Sprite* spriteCursor_ = nullptr;

	// --- 操作ヒント表示（画面上部に常時表示） ---
	KamataEngine::Sprite* spriteHintSelect_ = nullptr;

	// 現在選択中のインデックス（0:ヘヴィ, 1:スピード, 2:バランス）
	int32_t selectedIndex_ = static_cast<int32_t>(CharacterType::kBalance);

	// 揺れ・回転アニメーション用の経過時間
	float counter_ = 0.0f;

	// --- フェード ---
	Fade* fade_ = nullptr;
};
