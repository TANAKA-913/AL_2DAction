#include "Projectile.h"
#include "Math.h"

using namespace KamataEngine;

void Projectile::Initialize(Model* model, const Vector3& position, bool facingRight, float range) {
	model_ = model;
	direction_ = facingRight ? 1.0f : -1.0f;
	maxRange_ = range;
	traveledDistance_ = 0.0f;
	isDead_ = false;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.scale_ = {0.5f, 0.5f, 0.5f};
	// 進行方向に合わせて少し傾けておく（見た目のアクセント）
	worldTransform_.rotation_.y = facingRight ? 1.5707963f : -1.5707963f;

	// 初期化直後の1フレーム目から正しい位置で表示されるよう、ここで一度計算しておく
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Projectile::Update() {
	if (isDead_) {
		return;
	}

	// 直進させる
	worldTransform_.translation_.x += kSpeed * direction_;
	traveledDistance_ += kSpeed;

	// 少し回転させて弾らしい動きを出す
	worldTransform_.rotation_.z += 0.3f * direction_;

	// 最大飛距離を超えたら消える
	if (traveledDistance_ >= maxRange_) {
		isDead_ = true;
	}

	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Projectile::Draw(const Camera& camera) {
	if (isDead_) {
		return;
	}
	model_->Draw(worldTransform_, camera);
}

AABB Projectile::GetAABB() const {
	Vector3 pos = GetWorldPosition();
	AABB aabb;
	aabb.min = {pos.x - kWidth / 2.0f, pos.y - kHeight / 2.0f, pos.z - kWidth / 2.0f};
	aabb.max = {pos.x + kWidth / 2.0f, pos.y + kHeight / 2.0f, pos.z + kWidth / 2.0f};
	return aabb;
}

Vector3 Projectile::GetWorldPosition() const {
	Vector3 pos;
	pos.x = worldTransform_.matWorld_.m[3][0];
	pos.y = worldTransform_.matWorld_.m[3][1];
	pos.z = worldTransform_.matWorld_.m[3][2];
	return pos;
}
