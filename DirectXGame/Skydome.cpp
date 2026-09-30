#include "Skydome.h"
#include "Math.h"

using namespace KamataEngine;

void Skydome::Initialize(Model* model, const Vector3& position) {

	model_ = model;

	worldTransform_.Initialize();

	worldTransform_.scale_       = {200.0f, 200.0f, 200.0f};
	worldTransform_.translation_ = position;

	// 初期化直後の1フレーム目から正しい大きさ・位置で表示されるよう、
	// ここで一度ワールド行列を計算しておく
	// （Update()が呼ばれるまでの間、初期化直後のデフォルト行列＝
	//  座標(0,0,0)・等倍のまま描画されてしまうのを防ぐ）
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

void Skydome::Update() {

	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);

	worldTransform_.TransferMatrix();
}

void Skydome::Draw(const Camera& camera) { model_->Draw(worldTransform_, camera); }