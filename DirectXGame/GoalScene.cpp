#include "GoalScene.h"
#include "Math.h"
#include "SoundManager.h"
#include <cmath>

using namespace KamataEngine;

namespace {
// フェードにかける時間（秒）
constexpr float kFadeDuration = 1.0f;
} // namespace

GoalScene::~GoalScene() {
	// ゴール表示用モデルの解放
	delete modelGoal_;
	// "RESTART"用モデルの解放
	delete modelRestartFont_;
	// 天球の解放
	delete skydome_;
	delete modelSkydome_;
	// フェードの解放
	delete fade_;
}

void GoalScene::Initialize() {
	// カメラの初期化
	camera_.Initialize();

	// 天球の生成
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	// ゴール表示用モデル読み込み（Resources/gameclearFont フォルダに置いた専用モデル）
	modelGoal_ = Model::CreateFromOBJ("gameclearFont", true);

	// ワールドトランスフォームの初期化
	worldTransformGoal_.Initialize();
	worldTransformGoal_.translation_ = {0.0f, 0.0f, 0.0f};
	worldTransformGoal_.scale_ = {2.0f, 2.0f, 2.0f};

	// "RESTART"的な表示用モデル読み込み（Resources/restartFont フォルダに置いたモデル）
	// ゴール表示の下に表示する
	modelRestartFont_ = Model::CreateFromOBJ("restartFont", true);

	worldTransformRestartFont_.Initialize();
	worldTransformRestartFont_.translation_ = {0.0f, -5.0f, 0.0f};
	worldTransformRestartFont_.scale_ = {2.0f, 2.0f, 2.0f};

	// フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();

	// ゴールシーンの開始時点でフェードインが始まるようにする
	phase_ = Phase::kFadeIn;
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	// ゲームクリアSEを1回再生
	SoundManager::GetInstance()->PlaySEClear();
}

void GoalScene::Update() {
	// --- フェーズごとの更新処理 ---
	switch (phase_) {
	case Phase::kFadeIn:
		// フェードの更新
		fade_->Update();
		// フェードイン中にフェードが終わったらメインフェーズに切り替える
		if (fade_->IsFinished()) {
			phase_ = Phase::kMain;
		}
		break;

	case Phase::kMain:
		// メインフェーズでスペースキーを押したらフェードアウトを開始する
		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			// 決定音を再生
			SoundManager::GetInstance()->PlaySESelect();

			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
		break;

	case Phase::kFadeOut:
		// フェードの更新
		fade_->Update();
		// フェードアウト中にフェードが終わったらゴールシーンを終了する
		if (fade_->IsFinished()) {
			finished_ = true;
		}
		break;
	}

	// 経過時間を進める
	counter_ += 1.0f / 60.0f;

	// 天球の更新
	if (skydome_) {
		skydome_->Update();
	}

	// ロゴがふわふわ浮くように上下にゆらす
	worldTransformGoal_.translation_.y = std::sin(counter_ * 2.0f) * 0.5f;

	// ロゴがゆらゆら揺れるように少しだけ回転させる
	worldTransformGoal_.rotation_.y = std::sin(counter_) * 0.15f;

	// ワールド行列の更新
	worldTransformGoal_.matWorld_ = Math::MakeAffineMatrix(worldTransformGoal_.scale_, worldTransformGoal_.rotation_, worldTransformGoal_.translation_);
	worldTransformGoal_.TransferMatrix();

	// "RESTART"表示も、ゴール表示の少し下で同じように揺らす
	worldTransformRestartFont_.translation_.y = -5.0f + std::sin(counter_ * 2.0f) * 0.3f;
	worldTransformRestartFont_.rotation_.y    = std::sin(counter_) * 0.15f;

	worldTransformRestartFont_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformRestartFont_.scale_, worldTransformRestartFont_.rotation_, worldTransformRestartFont_.translation_);
	worldTransformRestartFont_.TransferMatrix();

	// カメラの行列更新
	camera_.UpdateMatrix();
}

void GoalScene::Draw() {
	// 描画開始
	Model::PreDraw();

	// 天球の描画
	if (skydome_) {
		skydome_->Draw(camera_);
	}

	// ゴール表示（専用モデル）の描画
	modelGoal_->Draw(worldTransformGoal_, camera_);

	// "RESTART"表示（専用モデル）の描画
	modelRestartFont_->Draw(worldTransformRestartFont_, camera_);

	// 描画終了
	Model::PostDraw();

	// フェードの描画（フェードイン中/フェードアウト中のみ）
	switch (phase_) {
	case Phase::kFadeIn:
	case Phase::kFadeOut:
		fade_->Draw();
		break;
	default:
		break;
	}
}
