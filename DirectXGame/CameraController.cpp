#include "CameraController.h"
#include "Player.h"
#include "kamataEngine.h"
#include <algorithm> 

void CameraController::Initialize() { camera_.Initialize(); }

void CameraController::Update() {
	if (!target_)
		return;

	const KamataEngine::WorldTransform& targetWorldTransform = target_->GetWorldTransform();
	const KamataEngine::Vector3& targetVelocity = target_->GetVelocity();

	targetPosition_.x = targetWorldTransform.translation_.x + targetoffset_.x + (targetVelocity.x * kVelocityBias);
	targetPosition_.y = targetWorldTransform.translation_.y + targetoffset_.y + (targetVelocity.y * kVelocityBias);
	targetPosition_.z = targetWorldTransform.translation_.z + targetoffset_.z + (targetVelocity.z * kVelocityBias);

	camera_.translation_.x = camera_.translation_.x + (targetPosition_.x - camera_.translation_.x) * kInterpolationRate;
	camera_.translation_.y = camera_.translation_.y + (targetPosition_.y - camera_.translation_.y) * kInterpolationRate;
	camera_.translation_.z = camera_.translation_.z + (targetPosition_.z - camera_.translation_.z) * kInterpolationRate;

	float targetX = targetWorldTransform.translation_.x;
	float targetY = targetWorldTransform.translation_.y;

	camera_.translation_.x = (std::max)(camera_.translation_.x, targetX + kMargin.left);
	camera_.translation_.x = (std::min)(camera_.translation_.x, targetX + kMargin.right);
	camera_.translation_.y = (std::max)(camera_.translation_.y, targetY + kMargin.bottom);
	camera_.translation_.y = (std::min)(camera_.translation_.y, targetY + kMargin.top);

	camera_.translation_.x = std::clamp(camera_.translation_.x, movableArea_.left, movableArea_.right);
	camera_.translation_.y = std::clamp(camera_.translation_.y, movableArea_.bottom, movableArea_.top);

	camera_.UpdateMatrix();
}

void CameraController::Reset() {
	if (!target_)
		return;

	const KamataEngine::WorldTransform& targetWorldTransform = target_->GetWorldTransform();

	camera_.translation_.x = targetWorldTransform.translation_.x + targetoffset_.x;
	camera_.translation_.y = targetWorldTransform.translation_.y + targetoffset_.y;
	camera_.translation_.z = targetWorldTransform.translation_.z + targetoffset_.z;

	camera_.translation_.x = std::clamp(camera_.translation_.x, movableArea_.left, movableArea_.right);
	camera_.translation_.y = std::clamp(camera_.translation_.y, movableArea_.bottom, movableArea_.top);

	camera_.UpdateMatrix();
}