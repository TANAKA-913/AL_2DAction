#include "Enemy.h"
#include "ShieldEnemy.h"
#include "Player.h"
#include "GameScene.h"
#include "MapChipField.h"
#include "SoundManager.h"
#include "Math.h"
#include <cmath>
#include <numbers>
#include <algorithm>

using namespace KamataEngine;

namespace {
float Lerp(float a, float b, float t) { return a + (b - a) * t; }
// 開始値から終了値まで、減速しながら変化する（クワッド・イーズアウト）
float EaseOut(float start, float end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	t = 1.0f - (1.0f - t) * (1.0f - t);
	return Lerp(start, end, t);
}
} // namespace

void Enemy::Initialize(Model* model, Camera* camera, const Vector3& position) {
	// モデルをメンバ変数に記録
	model_  = model;
	camera_ = camera;

	// ワールドトランスフォームの初期化
	worldTransform_.Initialize();

	// 初期座標の設定
	worldTransform_.translation_ = position;

	// 角度の初期値は自キャラと逆で左方向を向くように設定
	worldTransform_.rotation_.y = std::numbers::pi_v<float> * 3.0f / 2.0f;

	// 初期化直後の1フレーム目から正しい位置で表示されるよう、ここで一度行列を計算しておく
	// （Update()が呼ばれるまでの間、初期化直後のデフォルト行列＝座標(0,0,0)のまま
	//  描画されてしまうのを防ぐ）
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 最初は歩行ふるまいから開始
	behavior_ = Behavior::kWalk;
	BehaviorWalkInitialize();
}

void Enemy::Update() {
	// ふるまいのリクエストがあれば切り替える
	if (behaviorRequest_ != Behavior::kUnknown) {
		behavior_ = behaviorRequest_;

		switch (behavior_) {
		case Behavior::kWalk:
			BehaviorWalkInitialize();
			break;

		case Behavior::kDeath:
			BehaviorDeathInitialize();
			break;
		}

		behaviorRequest_ = Behavior::kUnknown;
	}

	// 現在のふるまいに応じて更新処理を分岐
	switch (behavior_) {
	case Behavior::kWalk:
	default:
		BehaviorWalkUpdate();
		break;

	case Behavior::kDeath:
		BehaviorDeathUpdate();
		break;
	}
}

void Enemy::Draw() {
	// ワールドトランスフォーム、カメラを渡して3Dモデルを描画する
	model_->Draw(worldTransform_, *camera_);

	// 背後パーティクルの描画
	DrawTrailParticles();
}

// ================================================================
//  ワールド座標を取得
// ================================================================
Vector3 Enemy::GetWorldPosition() {
	Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}

// ================================================================
//  AABBを取得
// ================================================================
AABB Enemy::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

	return aabb;
}

// ================================================================
//  衝突応答（敵側）
// ================================================================
void Enemy::OnCollision(const Player* player) {
	// デス演出中なら何もしない
	// （デス演出中に再度衝突コールバックが呼ばれてデス演出の最初に巻き戻ってしまうのを回避する早期return）
	if (behavior_ == Behavior::kDeath) {
		return;
	}

	// プレイヤーが攻撃中なら敵をデス演出に変更
	if (player->IsAttack()) {
		Defeat();
	}
}

void Enemy::Defeat() {
	// デス演出中なら何もしない（二重に呼ばれることがあるため）
	if (behavior_ == Behavior::kDeath) {
		return;
	}

	// 敵のふるまいをデス演出に変更
	behaviorRequest_ = Behavior::kDeath;

	// 撃破SEを再生
	SoundManager::GetInstance()->PlaySEEnemyDefeat();

	// 敵自身の座標にエフェクトを生成する
	Vector3 effectPos = GetWorldPosition();

	if (gameScene_) {
		gameScene_->CreateHitEffect(effectPos);
	}
}

// ================================================================
//  歩行ふるまい
// ================================================================
void Enemy::BehaviorWalkInitialize() {
	// 速度を設定する
	velocity_ = {-kWalkSpeed, 0, 0};

	walkTimer_ = 0.0f;
	turnCooldownTimer_ = 0.0f;
}

void Enemy::BehaviorWalkUpdate() {
	// タイマーを加算
	walkTimer_ += 1.0f / 60.0f;

	// 反転クールダウンのタイマーを減らす
	if (turnCooldownTimer_ > 0.0f) {
		turnCooldownTimer_ -= 1.0f / 60.0f;
	}

	// 進行方向の先に壁（ブロック）がないか、または進行方向の足元に床が
	// 無くなっていないか（穴・マップ端）を調べ、どちらかに該当したら反射する
	// ※ クールダウン中（反転した直後）は判定自体をスキップし、連続反転を防ぐ
	if (mapChipField_ && turnCooldownTimer_ <= 0.0f) {
		Vector3 worldPos = GetWorldPosition();

		// 進行方向側の先端に、わずかにマージンを持たせた点を作る
		float direction = (velocity_.x >= 0.0f) ? 1.0f : -1.0f;
		Vector3 aheadPos = worldPos;
		aheadPos.x += direction * (kWidth / 2.0f + kWallCheckMargin);

		// --- 壁チェック（進行方向、現在の高さ） ---
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(aheadPos);
		MapChipType aheadType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);

		// --- 床チェック（進行方向の、1つ下の段のマスを直接見る） ---
		MapChipType floorType = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex + 1);

		bool hitWall = (aheadType == MapChipType::kBlock);
		bool noFloorAhead = (floorType != MapChipType::kBlock);

		if (hitWall || noFloorAhead) {
			// 壁にぶつかった、または進行方向の床が無くなった（穴・マップ端）ので、
			// 速度を反転させて跳ね返る
			velocity_.x = -velocity_.x;

			// 反転した進行方向に合わせて、見た目の向きも反転させる
			worldTransform_.rotation_.y = (velocity_.x < 0.0f)
			    ? std::numbers::pi_v<float> * 3.0f / 2.0f  // 左向き
			    : std::numbers::pi_v<float> / 2.0f;         // 右向き

			// クールダウンを開始する
			turnCooldownTimer_ = kTurnCooldown;
		}
	}

	// 移動
	// 敵の座標 += 速度
	worldTransform_.translation_.x += velocity_.x;
	worldTransform_.translation_.y += velocity_.y;
	worldTransform_.translation_.z += velocity_.z;

	// 背後にパーティクルを生成する（一定間隔ごと）
	SpawnTrailParticle();

	// ワールド行列の更新
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 背後パーティクルの更新
	UpdateTrailParticles();
}

// ================================================================
//  デス演出ふるまい
// ================================================================
void Enemy::BehaviorDeathInitialize() {
	deathTimer_ = 0.0f;

	// デス演出開始と同時にコリジョンを無効化して、これ以上の衝突判定をスキップする
	isCollisionDisabled_ = true;

	// 移動は止める
	velocity_ = {};
}

void Enemy::BehaviorDeathUpdate() {
	// アニメーションのタイマーを加算する
	deathTimer_ += 1.0f / 60.0f;
	float t = deathTimer_ / kDeathTime;

	// Y軸まわりの回転角をイージングで変化させる（高速でぐるぐる回転）
	float yTurns = EaseOut(0.0f, kDeathSpinTurns, t);
	worldTransform_.rotation_.y = std::numbers::pi_v<float> * 3.0f / 2.0f + yTurns * 2.0f * std::numbers::pi_v<float>;

	// X軸まわりの回転角をイージングで変化させる（ゆっくりひっくり返る）
	float xDegree              = EaseOut(0.0f, kDeathFlipAngle, t);
	worldTransform_.rotation_.x = xDegree * std::numbers::pi_v<float> / 180.0f;

	// ワールドトランスフォームの行列更新
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// 死亡時点で残っていた背後パーティクルは、新規発生させずにフェードアウトだけ継続させる
	UpdateTrailParticles();

	// アニメーションのタイマーが一定時間に達したら
	if (deathTimer_ >= kDeathTime) {
		// デスフラグを立てる
		isDead_ = true;
	}
}

// ================================================================
//  背後パーティクル（トレイル）
// ================================================================
void Enemy::SpawnTrailParticle() {
	// モデルが設定されていなければ何もしない
	if (!modelTrail_) {
		return;
	}

	// 発生間隔タイマーを進める
	trailSpawnTimer_ += 1.0f / 60.0f;
	if (trailSpawnTimer_ < kTrailSpawnInterval) {
		return;
	}
	trailSpawnTimer_ = 0.0f;

	// 非アクティブなパーティクルを1つ探して使い回す
	for (TrailParticle& particle : trailParticles_) {
		if (particle.active) {
			continue;
		}

		// 進行方向の逆（背後）に少しオフセットした位置から発生させる
		float direction = (velocity_.x >= 0.0f) ? 1.0f : -1.0f;
		Vector3 spawnPos = worldTransform_.translation_;
		spawnPos.x -= direction * kTrailOffset;

		particle.worldTransform.Initialize();
		particle.worldTransform.scale_       = {kTrailScale, kTrailScale, kTrailScale};
		particle.worldTransform.translation_ = spawnPos;

		particle.objectColor.Initialize();
		particle.color  = {1.0f, 1.0f, 1.0f, 1.0f};
		particle.timer  = 0.0f;
		particle.active = true;
		break;
	}
}

void Enemy::UpdateTrailParticles() {
	for (TrailParticle& particle : trailParticles_) {
		if (!particle.active) {
			continue;
		}

		// 経過時間を進める
		particle.timer += 1.0f / 60.0f;
		float t = std::clamp(particle.timer / kTrailLifeTime, 0.0f, 1.0f);

		// 時間経過とともに拡大させ、消える直前にフェードアウトさせる
		// （発生直後＝敵に近いほど小さく、時間が経って敵から離れるほど大きくなる）
		float scale = kTrailScale * t;
		particle.worldTransform.scale_ = {scale, scale, scale};
		particle.color.w               = 1.0f - t;
		particle.objectColor.SetColor(particle.color);

		// 行列の更新
		particle.worldTransform.matWorld_ = Math::MakeAffineMatrix(
		    particle.worldTransform.scale_, particle.worldTransform.rotation_, particle.worldTransform.translation_);
		particle.worldTransform.TransferMatrix();

		// 寿命が尽きたら非アクティブに戻す
		if (particle.timer >= kTrailLifeTime) {
			particle.active = false;
		}
	}
}

void Enemy::DrawTrailParticles() {
	if (!modelTrail_) {
		return;
	}

	for (TrailParticle& particle : trailParticles_) {
		if (!particle.active) {
			continue;
		}
		modelTrail_->Draw(particle.worldTransform, *camera_, &particle.objectColor);
	}
}
