#include "TitleScene.h"
#include "Math.h"
#include "SoundManager.h"
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// フェードにかける時間（秒）
constexpr float kFadeDuration = 1.0f;
} // namespace

TitleScene::~TitleScene() {
	// タイトルロゴ用モデルの解放
	delete modelTitleLogo_;
	// "PRESS START"用モデルの解放
	delete modelStartFont_;
	// 3機体（プレイヤーモデル）の解放
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		delete modelCharacters_[i];
	}
	// 天球の解放
	delete skydome_;
	delete modelSkydome_;
	// フェードの解放
	delete fade_;
}

void TitleScene::Initialize() {
	// カメラの初期化
	camera_.Initialize();

	// 天球の生成
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	// タイトルロゴ表示用モデル読み込み（Resources/titleFont フォルダに置いたモデル）
	modelTitleLogo_ = Model::CreateFromOBJ("titleFont", true);

	// ワールドトランスフォームの初期化
	worldTransformTitleLogo_.Initialize();
	worldTransformTitleLogo_.translation_ = {0.0f, 0.0f, 0.0f};
	worldTransformTitleLogo_.scale_ = {2.0f, 2.0f, 2.0f};

	// "PRESS START"的な表示用モデル読み込み（Resources/startFont フォルダに置いたモデル）
	// タイトルロゴの下に表示する
	modelStartFont_ = Model::CreateFromOBJ("startFont", true);

	worldTransformStartFont_.Initialize();
	worldTransformStartFont_.translation_ = {0.0f, -5.0f, 0.0f};
	worldTransformStartFont_.scale_ = {2.0f, 2.0f, 2.0f};

	// 3機体（プレイヤーモデル）の読み込みと配置（"PRESS START"のさらに下に横並びで表示する）
	modelCharacters_[0] = Model::CreateFromOBJ("heavyBot", true);
	modelCharacters_[1] = Model::CreateFromOBJ("speedBot", true);
	modelCharacters_[2] = Model::CreateFromOBJ("balancer", true);

	constexpr float kCharacterSpacing = 7.0f;
	constexpr float kCharacterBaseY   = -11.0f;
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		worldTransformCharacters_[i].Initialize();
		worldTransformCharacters_[i].translation_ = {
		    (static_cast<float>(i) - 1.0f) * kCharacterSpacing, kCharacterBaseY, 0.0f};
		worldTransformCharacters_[i].scale_ = {3.0f, 3.0f, 3.0f};
		// 正面を向かせる（rotation_.y = 0だと逆向きだったため180度回転させる）
		worldTransformCharacters_[i].rotation_.y = std::numbers::pi_v<float>;
	}

	// フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();

	// タイトルシーンの開始時点でフェードインが始まるようにする
	phase_ = Phase::kFadeIn;
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	// タイトルBGMを再生（既に再生中の場合は何もしない）
	SoundManager::GetInstance()->PlayBGMTitle();
}

void TitleScene::Update() {
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
			// 決定音を再生し、タイトルBGMを止める
			SoundManager::GetInstance()->PlaySESelect();
			SoundManager::GetInstance()->StopBGM();

			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
		break;

	case Phase::kFadeOut:
		// フェードの更新
		fade_->Update();
		// フェードアウト中にフェードが終わったらタイトルシーンを終了する
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
	worldTransformTitleLogo_.translation_.y = std::sin(counter_ * 2.0f) * 0.5f;

	// ロゴがゆらゆら揺れるように少しだけ回転させる
	worldTransformTitleLogo_.rotation_.y = std::sin(counter_) * 0.15f;

	// ワールド行列の更新
	worldTransformTitleLogo_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformTitleLogo_.scale_, worldTransformTitleLogo_.rotation_, worldTransformTitleLogo_.translation_);
	worldTransformTitleLogo_.TransferMatrix();

	// "PRESS START"表示も、タイトルロゴの少し下で同じように揺らす
	worldTransformStartFont_.translation_.y = -5.0f + std::sin(counter_ * 2.0f) * 0.3f;
	worldTransformStartFont_.rotation_.y    = std::sin(counter_) * 0.15f;

	worldTransformStartFont_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformStartFont_.scale_, worldTransformStartFont_.rotation_, worldTransformStartFont_.translation_);
	worldTransformStartFont_.TransferMatrix();

	// 3機体（プレイヤーモデル）を、それぞれ位相をずらしてぷかぷか上下に浮遊させる
	// （正面（カメラ向き）で固定して見せたいので、上下浮遊以外の回転はさせない）
	constexpr float kCharacterBaseY = -11.0f;
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		// 機体ごとに揺れのタイミングをずらして、バラバラに動いているように見せる
		float phaseOffset = static_cast<float>(i) * 1.3f;

		worldTransformCharacters_[i].translation_.y = kCharacterBaseY + std::sin(counter_ * 1.8f + phaseOffset) * 0.4f;
		worldTransformCharacters_[i].rotation_.y     = std::numbers::pi_v<float>;

		worldTransformCharacters_[i].matWorld_ = Math::MakeAffineMatrix(
		    worldTransformCharacters_[i].scale_, worldTransformCharacters_[i].rotation_, worldTransformCharacters_[i].translation_);
		worldTransformCharacters_[i].TransferMatrix();
	}

	// カメラの行列更新
	camera_.UpdateMatrix();
}

void TitleScene::Draw() {
	// 描画開始
	Model::PreDraw();

	// 天球の描画
	if (skydome_) {
		skydome_->Draw(camera_);
	}

	// タイトルロゴ（専用モデル）の描画
	modelTitleLogo_->Draw(worldTransformTitleLogo_, camera_);

	// "PRESS START"表示（専用モデル）の描画
	modelStartFont_->Draw(worldTransformStartFont_, camera_);

	// 3機体（プレイヤーモデル）の描画
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		modelCharacters_[i]->Draw(worldTransformCharacters_[i], camera_);
	}

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
