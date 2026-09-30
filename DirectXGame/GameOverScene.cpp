#include "GameOverScene.h"
#include "Math.h"
#include "SoundManager.h"
#include <cmath>

using namespace KamataEngine;

namespace {
// フェードにかける時間（秒）
constexpr float kFadeDuration = 1.0f;
} // namespace

GameOverScene::~GameOverScene() {
	// ゲームオーバーBGMを停止
	SoundManager::GetInstance()->StopBGM();

	// ゲームオーバー表示用モデルの解放
	delete modelGameOver_;
	// "RESTART"用モデルの解放
	delete modelRestartFont_;
	// 天球の解放
	delete skydome_;
	delete modelSkydome_;
	// 赤いオーバーレイスプライトの解放
	delete spriteOverlay_;
	// フェードの解放
	delete fade_;
}

void GameOverScene::Initialize() {
	// カメラの初期化
	camera_.Initialize();

	// 天球の生成
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	// ゲームオーバー表示用モデル読み込み（Resources/gameoverFont フォルダに置いた専用モデル）
	modelGameOver_ = Model::CreateFromOBJ("gameoverFont", true);

	// ワールドトランスフォームの初期化
	worldTransformGameOver_.Initialize();
	worldTransformGameOver_.translation_ = {0.0f, 0.0f, 0.0f};
	worldTransformGameOver_.scale_ = {2.0f, 2.0f, 2.0f};

	// "RESTART"的な表示用モデル読み込み（Resources/restartFont フォルダに置いたモデル）
	// ゲームオーバー表示の下に表示する
	modelRestartFont_ = Model::CreateFromOBJ("restartFont", true);

	worldTransformRestartFont_.Initialize();
	worldTransformRestartFont_.translation_ = {0.0f, -5.0f, 0.0f};
	worldTransformRestartFont_.scale_ = {2.0f, 2.0f, 2.0f};

	// 赤い半透明オーバーレイを生成（ゲームオーバーらしい雰囲気を出すための演出）
	spriteOverlay_ = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {0, 0});
	spriteOverlay_->SetSize(Vector2(1280.0f, 720.0f));
	spriteOverlay_->SetColor(Vector4(0.5f, 0.0f, 0.0f, 0.35f));

	// フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();

	// ゲームオーバーシーンの開始時点でフェードインが始まるようにする
	phase_ = Phase::kFadeIn;
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	// ゲームオーバーBGMを再生
	// ※ 仮実装：専用BGMがまだ無いのでタイトルBGMを流用している
	SoundManager::GetInstance()->PlayBGMGameOver();
}

void GameOverScene::Update() {
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
		// メインフェーズで特定のボタン（SPACEキー）を押したらフェードアウトを開始する
		// ※ キー割り当てを変えたい場合はここのDIK_SPACEを差し替えてください
		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			// 決定音を再生し、ゲームオーバーBGMを止める
			SoundManager::GetInstance()->PlaySESelect();
			SoundManager::GetInstance()->StopBGM();

			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
		break;

	case Phase::kFadeOut:
		// フェードの更新
		fade_->Update();
		// フェードアウト中にフェードが終わったらゲームオーバーシーンを終了する
		// （main側はこのフラグを見てタイトルシーンへ遷移する）
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
	worldTransformGameOver_.translation_.y = std::sin(counter_ * 2.0f) * 0.5f;

	// ロゴがゆらゆら揺れるように少しだけ回転させる
	worldTransformGameOver_.rotation_.y = std::sin(counter_) * 0.15f;

	// ワールド行列の更新
	worldTransformGameOver_.matWorld_ = Math::MakeAffineMatrix(worldTransformGameOver_.scale_, worldTransformGameOver_.rotation_, worldTransformGameOver_.translation_);
	worldTransformGameOver_.TransferMatrix();

	// "RESTART"表示も、ゲームオーバー表示の少し下で同じように揺らす
	worldTransformRestartFont_.translation_.y = -5.0f + std::sin(counter_ * 2.0f) * 0.3f;
	worldTransformRestartFont_.rotation_.y    = std::sin(counter_) * 0.15f;

	worldTransformRestartFont_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformRestartFont_.scale_, worldTransformRestartFont_.rotation_, worldTransformRestartFont_.translation_);
	worldTransformRestartFont_.TransferMatrix();

	// カメラの行列更新
	camera_.UpdateMatrix();
}

void GameOverScene::Draw() {
	// 描画開始
	Model::PreDraw();

	// 天球の描画
	if (skydome_) {
		skydome_->Draw(camera_);
	}

	// ゲームオーバー表示（専用モデル）の描画
	modelGameOver_->Draw(worldTransformGameOver_, camera_);

	// "RESTART"表示（専用モデル）の描画
	modelRestartFont_->Draw(worldTransformRestartFont_, camera_);

	// 描画終了
	Model::PostDraw();

	// 赤いオーバーレイの描画（雰囲気演出）
	Sprite::PreDraw();
	spriteOverlay_->Draw();
	Sprite::PostDraw();

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
