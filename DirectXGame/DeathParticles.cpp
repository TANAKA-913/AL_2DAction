#include "DeathParticles.h"
#include "Math.h"
#include <algorithm>
#include <cmath>

using namespace KamataEngine;

// ================================================================
//  初期化
// ================================================================
void DeathParticles::Initialize(Model* model, Camera* camera, const Vector3& position) {
	// メンバ変数に記録
	model_       = model;
	camera_      = camera;
	basePosition_ = position;

	// 本体（球）のワールドトランスフォームを初期化する
	for (WorldTransform& worldTransform : worldTransforms_) {
		worldTransform.Initialize();
		worldTransform.translation_ = position;
	}

	// トレイル（敵・ダッシュと共通のモデル）のワールドトランスフォームを初期化する
	for (WorldTransform& worldTransform : trailWorldTransforms_) {
		worldTransform.Initialize();
		worldTransform.translation_ = position;
		worldTransform.scale_       = {kTrailScale, kTrailScale, kTrailScale};
	}

	// カウンター、終了フラグをリセット
	counter_    = 0.0f;
	isFinished_ = false;

	// 色変更オブジェクトの初期化（SetColor()で指定された色から開始。未指定なら白）
	objectColor_.Initialize();
	color_ = baseColor_;
	objectColor_.SetColor(color_);

	// 生成された直後の1フレーム目からUpdate()を待たずに正しい位置で描画されるよう、
	// ここで一度全パーティクルのワールド行列を計算しておく。
	// （これをしないと、生成された瞬間だけ初期化直後のデフォルト行列＝
	//  座標(0,0,0)のまま一瞬だけ描画されてしまう）
	for (WorldTransform& worldTransform : worldTransforms_) {
		worldTransform.matWorld_ = Math::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
	for (WorldTransform& worldTransform : trailWorldTransforms_) {
		worldTransform.matWorld_ = Math::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
}

// ================================================================
//  更新
// ================================================================
void DeathParticles::Update() {
	// 終了なら何もしない
	if (isFinished_) {
		return;
	}

	// --- 移動処理（渦を巻きながら外側へ広がる） ---
	// 半径は経過時間に比例して伸び、角度も経過時間に比例して回転し続けることで、
	// 直線的な飛散ではなく渦巻き状の軌道になる。
	float radius = kRadiusGrowthRate * counter_;

	for (uint32_t i = 0; i < kNumParticles; ++i) {
		float angle = kAngleUnit * static_cast<float>(i) + counter_ * kRotationSpeed;

		worldTransforms_[i].translation_.x = basePosition_.x + std::cos(angle) * radius;
		worldTransforms_[i].translation_.y = basePosition_.y + std::sin(angle) * radius;
		worldTransforms_[i].translation_.z = basePosition_.z;

		// 渦の勢いが伝わるよう、パーティクル自体も回転させる
		worldTransforms_[i].rotation_.z = angle;
	}

	// トレイルパーティクルも同じ渦の軌道に乗せる（本体の球より少し内側・逆位相にして層を作る）
	float trailRadius = radius * 0.75f;
	for (uint32_t i = 0; i < kNumTrailParticles; ++i) {
		float angle = kTrailAngleUnit * static_cast<float>(i) - counter_ * kRotationSpeed * 1.3f;

		trailWorldTransforms_[i].translation_.x = basePosition_.x + std::cos(angle) * trailRadius;
		trailWorldTransforms_[i].translation_.y = basePosition_.y + std::sin(angle) * trailRadius;
		trailWorldTransforms_[i].translation_.z = basePosition_.z;

		trailWorldTransforms_[i].rotation_.z = -angle;
	}

	// --- 一定時間で消す ---
	// カウンターを1フレーム分の秒数進める
	counter_ += 1.0f / 60.0f;
	// 存続時間の上限に達したら
	if (counter_ >= kDuration) {
		counter_ = kDuration;
		// 終了扱いにする
		isFinished_ = true;
	}

	// --- フェードアウト ---
	// counter_が0のときアルファ値が1.0、kDurationのときアルファ値が0.0になるようにする
	color_   = baseColor_;
	color_.w = std::clamp(1.0f - counter_ / kDuration, 0.0f, 1.0f);
	// 色変更オブジェクトに色の数値を設定する
	objectColor_.SetColor(color_);

	// --- ワールドトランスフォームの更新 ---
	for (WorldTransform& worldTransform : worldTransforms_) {
		// スケール、回転、平行移動からアフィン変換行列を計算し、VRAMに転送する
		worldTransform.matWorld_ = Math::MakeAffineMatrix(
		    worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
	for (WorldTransform& worldTransform : trailWorldTransforms_) {
		worldTransform.matWorld_ = Math::MakeAffineMatrix(
		    worldTransform.scale_, worldTransform.rotation_, worldTransform.translation_);
		worldTransform.TransferMatrix();
	}
}

// ================================================================
//  描画
// ================================================================
void DeathParticles::Draw() {
	// 終了なら何もしない
	if (isFinished_) {
		return;
	}

	// 本体（球）パーティクルの描画
	for (WorldTransform& worldTransform : worldTransforms_) {
		model_->Draw(worldTransform, *camera_, &objectColor_);
	}

	// トレイル（敵・ダッシュと共通のモデル）パーティクルの描画
	if (modelTrail_) {
		for (WorldTransform& worldTransform : trailWorldTransforms_) {
			modelTrail_->Draw(worldTransform, *camera_, &objectColor_);
		}
	}
}
