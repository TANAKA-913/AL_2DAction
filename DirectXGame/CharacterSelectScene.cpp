#include "CharacterSelectScene.h"
#include "SoundManager.h"
#include "Math.h"
#include <cmath>
#include <numbers>

using namespace KamataEngine;

namespace {
// フェードにかける時間（秒）
constexpr float kFadeDuration = 1.0f;

// 3体の機体を並べる際のX方向の間隔（モデルを2倍サイズにした分、間隔も広げてある）
constexpr float kCharacterSpacing = 5.0f;

// カメラを機体から離す距離（3体を横並びで画面に収めるため、通常プレイ時より少し引いている。
// モデルを2倍サイズにした分、以前より少し多めに引いている）
constexpr float kCameraDistance = -28.0f;

// カーソル用スプライトの、画面中央を基準にした横方向の間隔[px]
constexpr float kCursorSpacingPx = 220.0f;
} // namespace

CharacterSelectScene::~CharacterSelectScene() {
	// 機体表示用モデルの解放（3体分）
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		delete modelCharacters_[i];
	}
	// 天球の解放
	delete skydome_;
	delete modelSkydome_;
	// カーソル用スプライトの解放
	delete spriteCursor_;
	// 操作ヒントスプライトの解放
	delete spriteHintSelect_;
	// フェードの解放
	delete fade_;
}

void CharacterSelectScene::Initialize() {
	// カメラの初期化（3体を横並びで見渡せるよう、少し引いた位置に置く）
	camera_.Initialize();
	camera_.translation_.z = kCameraDistance;
	camera_.farZ = 1000.0f;

	// 天球の生成
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	skydome_ = new Skydome();
	skydome_->Initialize(modelSkydome_);

	// 機体表示用モデル読み込み（3体それぞれの専用モデル）
	modelCharacters_[static_cast<int32_t>(CharacterType::kHeavy)]   = Model::CreateFromOBJ("heavyBot", true);
	modelCharacters_[static_cast<int32_t>(CharacterType::kSpeed)]   = Model::CreateFromOBJ("speedBot", true);
	modelCharacters_[static_cast<int32_t>(CharacterType::kBalance)] = Model::CreateFromOBJ("balancer", true);

	for (int32_t i = 0; i < kNumCharacters; ++i) {
		worldTransformCharacters_[i].Initialize();
		worldTransformCharacters_[i].translation_ = {
		    (static_cast<float>(i) - 1.0f) * kCharacterSpacing, 0.0f, 0.0f};
		worldTransformCharacters_[i].scale_ = {2.0f, 2.0f, 2.0f};
		worldTransformCharacters_[i].matWorld_ = Math::MakeAffineMatrix(
		    worldTransformCharacters_[i].scale_, worldTransformCharacters_[i].rotation_, worldTransformCharacters_[i].translation_);
		worldTransformCharacters_[i].TransferMatrix();

		// 専用モデル自体の色をそのまま見せるため、着色はせず白（無着色）にしておく
		objectColorCharacters_[i].Initialize();
		objectColorCharacters_[i].SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	// カーソル用スプライトの生成（選択中の機体の下に表示する三角形代わりの四角）
	spriteCursor_ = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {0, 0});
	spriteCursor_->SetSize(Vector2(32.0f, 8.0f));
	spriteCursor_->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));

	// 操作ヒントスプライトの生成（画面上部に常時表示）
	{
		constexpr float kHintWidth  = 480.0f;
		constexpr float kScreenWidth = 1280.0f;
		constexpr float kHintY      = 30.0f;
		float hintX = (kScreenWidth - kHintWidth) / 2.0f;
		spriteHintSelect_ = Sprite::Create(TextureManager::GetInstance()->Load("hints/hint_select.png"), {hintX, kHintY});
	}

	// デフォルトの選択（バランサー）
	selectedIndex_ = static_cast<int32_t>(CharacterType::kBalance);

	// フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();

	// キャラクター選択シーンの開始時点でフェードインが始まるようにする
	phase_ = Phase::kFadeIn;
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	// タイトルと同じBGMを再生（既に再生中の場合は何もしない）
	SoundManager::GetInstance()->PlayBGMTitle();
}

void CharacterSelectScene::Update() {
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
		// 左右キーで選択する機体を切り替える
		if (Input::GetInstance()->TriggerKey(DIK_RIGHT)) {
			selectedIndex_ = (selectedIndex_ + 1) % kNumCharacters;
			SoundManager::GetInstance()->PlaySESelect();
		} else if (Input::GetInstance()->TriggerKey(DIK_LEFT)) {
			selectedIndex_ = (selectedIndex_ - 1 + kNumCharacters) % kNumCharacters;
			SoundManager::GetInstance()->PlaySESelect();
		}

		// SPACEキーで決定し、フェードアウトを開始する
		if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
			SoundManager::GetInstance()->PlaySESelect();
			SoundManager::GetInstance()->StopBGM();
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
		break;

	case Phase::kFadeOut:
		// フェードの更新
		fade_->Update();
		// フェードアウト中にフェードが終わったらシーンを終了する
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

	// 3体それぞれをゆっくり回転させつつ、選択中の機体だけ少し浮かせて目立たせる
	// ※ rotation_.y = 0 だとモデルの向きが逆（背面）になるため、
	//   πを基準（＝開始時点で正面）にしてそこから回転させる。
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		worldTransformCharacters_[i].rotation_.y = std::numbers::pi_v<float> + counter_ * 0.5f;

		float baseY = 0.0f;
		if (i == selectedIndex_) {
			// 選択中は上下にふわふわ浮かせる
			baseY = 0.3f + std::sin(counter_ * 3.0f) * 0.15f;
		}
		worldTransformCharacters_[i].translation_.y = baseY;

		worldTransformCharacters_[i].matWorld_ = Math::MakeAffineMatrix(
		    worldTransformCharacters_[i].scale_, worldTransformCharacters_[i].rotation_, worldTransformCharacters_[i].translation_);
		worldTransformCharacters_[i].TransferMatrix();
	}

	// カーソルスプライトの位置を、選択中の機体の下に合わせる
	// ※ Sprite::SetPosition()が無いバージョンのKamataEngineの場合はコンパイルエラーになるので、
	//   その際はSprite.hを確認し、対応するメソッド名（例：SetTranslate等）に置き換えてください。
	{
		constexpr float kScreenCenterX = 640.0f;
		constexpr float kCursorY       = 560.0f;
		float cursorX = kScreenCenterX + (static_cast<float>(selectedIndex_) - 1.0f) * kCursorSpacingPx - 16.0f;
		spriteCursor_->SetPosition(Vector2(cursorX, kCursorY));
	}

	// カメラの行列更新
	camera_.UpdateMatrix();
}

void CharacterSelectScene::Draw() {
	// 描画開始
	Model::PreDraw();

	// 天球の描画
	if (skydome_) {
		skydome_->Draw(camera_);
	}

	// 3体分の機体（それぞれの専用モデル）を描画
	for (int32_t i = 0; i < kNumCharacters; ++i) {
		modelCharacters_[i]->Draw(worldTransformCharacters_[i], camera_, &objectColorCharacters_[i]);
	}

	// 描画終了
	Model::PostDraw();

	// カーソルスプライトの描画
	Sprite::PreDraw();
	spriteCursor_->Draw();
	// 操作ヒントの描画（画面上部）
	if (spriteHintSelect_) {
		spriteHintSelect_->Draw();
	}
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
