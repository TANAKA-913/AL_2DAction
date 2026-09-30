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

void ShieldEnemy::Initialize(Model* model, Camera* camera, const Vector3& position) {
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

void ShieldEnemy::Update() {
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

		case Behavior::kGuard:
			BehaviorGuardInitialize();
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

	case Behavior::kGuard:
		BehaviorGuardUpdate();
		break;
	}
}

void ShieldEnemy::Draw() {
	// ワールドトランスフォーム、カメラを渡して3Dモデルを描画する
	model_->Draw(worldTransform_, *camera_);

	// 背後パーティクルの描画
	DrawTrailParticles();
}

// ================================================================
//  ワールド座標を取得
// ================================================================
Vector3 ShieldEnemy::GetWorldPosition() {
	Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}

// ================================================================
//  AABBを取得
// ================================================================
AABB ShieldEnemy::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

	return aabb;
}

// ================================================================
//  衝突応答（敵側）
// ================================================================
void ShieldEnemy::OnCollision(Player* player) {
	// デス演出中・ガード演出中なら何もしない
	// （演出中に再度衝突コールバックが呼ばれて演出の最初に巻き戻ってしまうのを回避する早期return）
	if (behavior_ == Behavior::kDeath || behavior_ == Behavior::kGuard) {
		return;
	}

	// プレイヤーが攻撃中なら敵が死ぬ
	if (player->IsAttack()) {

		// 前方であればガード成功
		if (IsFacingPlayer(player)) {
			// ガードエフェクトを生成する
			if (gameScene_) {
				gameScene_->CreateGuardEffect(GetWorldPosition());
			}

			// ガード成功SE（攻撃SEを流用）を再生
			SoundManager::GetInstance()->PlaySEAttack();

			// プレイヤーのノックバックを要求する
			player->RequestKnockback();

			// 敵のふるまいをガード演出に変更
			behaviorRequest_ = Behavior::kGuard;

			// 早期returnで敵のデスを回避する
			return;
		}

		// ここに来るのは背後などガードが成功しなかった攻撃 → 通常どおりデス演出へ
		Defeat();
	}
}

void ShieldEnemy::OnProjectileHit(Player* player) {
	// デス演出中・ガード演出中なら何もしない
	if (behavior_ == Behavior::kDeath || behavior_ == Behavior::kGuard) {
		return;
	}

	// 近接攻撃と同じく、正面（盾のある側）で受けたかどうかを判定する
	if (IsFacingPlayer(player)) {
		// ガード成功：弾を防ぎ、敵は倒れない
		// ガードエフェクトを生成する
		if (gameScene_) {
			gameScene_->CreateGuardEffect(GetWorldPosition());
		}

		// ガード成功SE（攻撃SEを流用）を再生
		SoundManager::GetInstance()->PlaySEAttack();

		// プレイヤーのノックバックを要求する
		player->RequestKnockback();

		// 敵のふるまいをガード演出に変更
		behaviorRequest_ = Behavior::kGuard;
		return;
	}

	// 背後など、ガードできない向きで受けた弾はそのまま撃破する
	Defeat();
}

void ShieldEnemy::Defeat() {
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
//  プレイヤーと正面から向かい合っているかどうかを判定する
// ================================================================
bool ShieldEnemy::IsFacingPlayer(const Player* player) {
	// このシールド敵の「正面（盾のある側）」は、現在の進行方向側になる。
	// （壁に当たって反射すると進行方向＝向きが反転するため、固定ではなく毎回判定する）
	Vector3 shieldPos = GetWorldPosition();
	Vector3 playerPos = player->GetWorldPosition();

	bool facingRight = (velocity_.x > 0.0f);

	// プレイヤーが盾の正面側にいるか
	bool playerIsInFront = facingRight ? (playerPos.x > shieldPos.x) : (playerPos.x < shieldPos.x);
	// プレイヤーが盾に向かって突進してくる向きを向いているか
	bool playerFacingShield = facingRight ? !player->IsFacingRight() : player->IsFacingRight();

	return playerIsInFront && playerFacingShield;
}

// ================================================================
//  歩行ふるまい
// ================================================================
void ShieldEnemy::BehaviorWalkInitialize() {
	// 速度を設定する
	velocity_ = {-kWalkSpeed, 0, 0};

	walkTimer_ = 0.0f;
	turnCooldownTimer_ = 0.0f;
}

void ShieldEnemy::BehaviorWalkUpdate() {
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

	// 回転アニメーション
	float param  = std::sin(walkTimer_ * 2.0f * std::numbers::pi_v<float> / kWalkMotionTime);
	float degree = kWalkMotionAngleStart + kWalkMotionAngleEnd * (param + 1.0f) / 2.0f;
	worldTransform_.rotation_.x = degree * std::numbers::pi_v<float> / 180.0f;

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
void ShieldEnemy::BehaviorDeathInitialize() {
	deathTimer_ = 0.0f;

	// デス演出開始と同時にコリジョンを無効化して、これ以上の衝突判定をスキップする
	isCollisionDisabled_ = true;

	// 移動は止める
	velocity_ = {};
}

void ShieldEnemy::BehaviorDeathUpdate() {
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
//  ガード演出ふるまい
//  最初にやや下向きから、天井向きにのけぞり、また元の向きへ戻る
// ================================================================
void ShieldEnemy::BehaviorGuardInitialize() {
	guardTimer_ = 0.0f;

	// ガード中は移動を止める
	velocity_ = {};
}

void ShieldEnemy::BehaviorGuardUpdate() {
	// アニメーションのタイマーを加算する
	guardTimer_ += 1.0f / 60.0f;
	float t = std::clamp(guardTimer_ / kGuardTime, 0.0f, 1.0f);

	float angleDegree;
	if (t < 0.5f) {
		// 前半：やや下向き → のけぞって天井向きへ
		float half  = t / 0.5f;
		angleDegree = Lerp(kGuardTiltAngle, kGuardBackAngle, EaseOut(0.0f, 1.0f, half));
	} else {
		// 後半：のけぞり姿勢から元の向き（歩行時の0度）へ戻る
		float half  = (t - 0.5f) / 0.5f;
		angleDegree = Lerp(kGuardBackAngle, 0.0f, EaseOut(0.0f, 1.0f, half));
	}
	worldTransform_.rotation_.x = angleDegree * std::numbers::pi_v<float> / 180.0f;

	// ワールドトランスフォームの行列更新
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// ガード演出が終わったら歩行ふるまいに戻る
	if (guardTimer_ >= kGuardTime) {
		behaviorRequest_ = Behavior::kWalk;
	}
}

// ================================================================
//  背後パーティクル（トレイル）
// ================================================================
void ShieldEnemy::SpawnTrailParticle() {
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

void ShieldEnemy::UpdateTrailParticles() {
	for (TrailParticle& particle : trailParticles_) {
		if (!particle.active) {
			continue;
		}

		// 経過時間を進める
		particle.timer += 1.0f / 60.0f;
		float t = std::clamp(particle.timer / kTrailLifeTime, 0.0f, 1.0f);

		// 時間経過とともに拡大させ、消える直前にフェードアウトさせる
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

void ShieldEnemy::DrawTrailParticles() {
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
