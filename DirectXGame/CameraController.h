#pragma once

#include "kamataEngine.h"

class Player;

class CameraController {
public:
	struct Rect {
		float left = 0.0f;
		float right = 1.0f;
		float bottom = 0.0f;
		float top = 1.0f;
	};

	void Initialize();
	void Update();
	void SetTarget(Player* target) { target_ = target; }
	void Reset();

	const KamataEngine::Camera& GetCamera() const { return camera_; }

	void SetMovableArea(const Rect& area) { movableArea_ = area; }

private:
	KamataEngine::Camera camera_;
	KamataEngine::DebugCamera* debugCamera_ = nullptr;
	Player* target_ = nullptr;
	KamataEngine::Vector3 targetoffset_ = {0, 0, -15.0f};

	KamataEngine::Vector3 targetPosition_;

	static inline const float kInterpolationRate = 0.1f;

	static inline const float kVelocityBias = 5.0f;

	static inline const Rect kMargin = {-5.0f, 5.0f, -3.0f, 4.0f};

	Rect movableArea_ = {0.0f, 100.0f, 0.0f, 100.0f};
};