#pragma once

#include "kamataEngine.h"

class Skydome {

public:
	// position: 天球の中心座標（省略時は原点）。
	// マップの中心に合わせておかないと、ステージの端でプレイヤーが
	// 天球の縁に近づきすぎて見えてしまうことがあるため、
	// マップサイズに応じてGameScene側から中心座標を渡すことを想定している。
	void Initialize(KamataEngine::Model* model, const KamataEngine::Vector3& position = {0.0f, 0.0f, 0.0f});
	void Update();
	void Draw(const KamataEngine::Camera& camera);

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
};