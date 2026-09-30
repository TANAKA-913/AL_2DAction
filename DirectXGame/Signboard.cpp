#include "Signboard.h"
#include "Math.h"

using namespace KamataEngine;

void Signboard::Initialize(Model* model, const Vector3& position, int32_t hintIndex) {
	model_     = model;
	hintIndex_ = hintIndex;

	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	// 看板らしい見た目にするため、横に細く・縦に長く変形させる（仮実装：ブロックモデル流用）
	worldTransform_.scale_ = {0.2f, 1.3f, 0.2f};

	// 初期化直後の1フレーム目から正しい位置・大きさで表示されるよう、ここで一度計算しておく
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Signboard::Draw(const Camera& camera) { model_->Draw(worldTransform_, camera); }

Vector3 Signboard::GetWorldPosition() const {
	Vector3 pos;
	pos.x = worldTransform_.matWorld_.m[3][0];
	pos.y = worldTransform_.matWorld_.m[3][1];
	pos.z = worldTransform_.matWorld_.m[3][2];
	return pos;
}
