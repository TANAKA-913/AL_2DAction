#include "GuardEffect.h"
#include "Math.h"
#include <cassert>
#include <algorithm>
#include <numbers>
#include <random>

using namespace KamataEngine;

// --- 静的メンバ変数の実体 ---
Model*  GuardEffect::model_  = nullptr;
Camera* GuardEffect::camera_ = nullptr;

namespace {

float Lerp(float a, float b, float t) { return a + (b - a) * t; }
// 開始値から終了値まで、減速しながら変化する（クワッド・イーズアウト）
float EaseOut(float start, float end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	t = 1.0f - (1.0f - t) * (1.0f - t);
	return Lerp(start, end, t);
}

// --- 乱数生成 ---
// 共通の初期化（プログラム全体でただ一つのエンジンを共有する）
std::mt19937_64& GetRandomEngine() {
	static std::random_device seedGenerator;
	static std::mt19937_64    randomEngine(seedGenerator());
	return randomEngine;
}

} // namespace

// ================================================================
//  インスタンス生成（static factory）
// ================================================================
GuardEffect* GuardEffect::Create(const Vector3& position) {
	// インスタンス生成
	GuardEffect* instance = new GuardEffect();
	// newの失敗を検出
	assert(instance);
	// インスタンスの初期化
	instance->Initialize(position);
	// 初期化したインスタンスを返す
	return instance;
}

// ================================================================
//  初期化
// ================================================================
void GuardEffect::Initialize(const Vector3& position) {
	// 円形エフェクトのワールドトランスフォームを発生座標で初期化
	circleWorldTransform_.Initialize();
	circleWorldTransform_.translation_ = position;
	circleWorldTransform_.scale_       = {0.0f, 0.0f, 1.0f};

	// 楕円エフェクト
	// 指定範囲の乱数生成器（-π～+πの範囲でランダムな回転角を生成する）
	std::uniform_real_distribution<float> rotationDistribution(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

	for (WorldTransform& worldTransform : ellipseWorldTransforms_) {
		// Initialize()は座標・回転をデフォルト値に戻すので、必ず値の設定より先に呼ぶ
		worldTransform.Initialize();
		worldTransform.scale_    = {0.0f, 0.0f, 1.0f};
		worldTransform.rotation_ = {0.0f, 0.0f, rotationDistribution(GetRandomEngine())};
		// 楕円エフェクトのトランスレーションを発生座標で初期化
		worldTransform.translation_ = position;
	}

	// 状態・カウンターの初期化
	state_   = State::kSpread;
	counter_ = 0.0f;

	// 色変更オブジェクトの初期化
	// ガード成功は「弾いた」感じを出すため、ヒットエフェクトと差別化して水色にしておく
	objectColor_.Initialize();
	color_ = {0.4f, 0.8f, 1.0f, 1.0f};

	// 生成された直後の1フレーム目からUpdate()を待たずに正しい位置・大きさ（今はscale=0）で
	// 描画されるよう、ここで一度ワールド行列を計算しておく。
	// （これをしないと、生成された瞬間だけ初期化直後のデフォルト行列＝
	//  座標(0,0,0)・等倍のまま一瞬だけ描画されてしまう）
	circleWorldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    circleWorldTransform_.scale_, circleWorldTransform_.rotation_, circleWorldTransform_.translation_);
	circleWorldTransform_.TransferMatrix();

	for (WorldTransform& worldTransform : ellipseWorldTransforms_) {
		worldTransform.matWorld_ = Math::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
}

// ================================================================
//  更新
// ================================================================
void GuardEffect::Update() {
	// 消滅済みなら何もしない
	if (state_ == State::kDead) {
		return;
	}

	// カウンターを1フレーム分の秒数進める
	counter_ += 1.0f / 60.0f;

	switch (state_) {
		// 広がる
	case State::kSpread: {
		float t     = counter_ / kSpreadTime;
		float scale = EaseOut(0.0f, 1.0f, t);

		// 円形エフェクトのワールドトランスフォームを更新
		circleWorldTransform_.scale_.x = scale;
		circleWorldTransform_.scale_.y = scale;

		// 楕円エフェクトも同様にスプレッドさせる
		for (WorldTransform& worldTransform : ellipseWorldTransforms_) {
			worldTransform.scale_.x = kEllipseWidth * scale;
			worldTransform.scale_.y = kEllipseLength * scale;
		}

		// フェードへ移行
		if (counter_ >= kSpreadTime) {
			state_   = State::kFade;
			counter_ = 0.0f; // カウンターをリセット
		}
		break;
	}
		// 消える
	case State::kFade: {
		float t = counter_ / kFadeTime;
		// アルファ値をイージングで変化させる
		color_.w = EaseOut(1.0f, 0.0f, t);

		// 消滅済みへ移行
		if (counter_ >= kFadeTime) {
			state_ = State::kDead;
		}
		break;
	}
	default:
		break;
	}

	// 色変更オブジェクトに色の数値を設定する
	objectColor_.SetColor(color_);

	// 円形エフェクトのワールドトランスフォームを更新
	circleWorldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    circleWorldTransform_.scale_, circleWorldTransform_.rotation_, circleWorldTransform_.translation_);
	circleWorldTransform_.TransferMatrix();

	// 楕円エフェクトのワールドトランスフォームを更新
	for (WorldTransform& worldTransform : ellipseWorldTransforms_) {
		worldTransform.matWorld_ = Math::MakeAffineMatrix(
		    worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
}

// ================================================================
//  描画
// ================================================================
void GuardEffect::Draw() {
	// 消滅済みなら何もしない
	if (state_ == State::kDead) {
		return;
	}

	if (!model_ || !camera_) {
		return;
	}

	// 円形エフェクトのモデルを描画
	model_->Draw(circleWorldTransform_, *camera_, &objectColor_);

	// 楕円エフェクトのモデルを描画
	for (WorldTransform& worldTransform : ellipseWorldTransforms_) {
		model_->Draw(worldTransform, *camera_, &objectColor_);
	}
}
