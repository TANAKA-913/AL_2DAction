#define NOMINMAX
#include "Player.h"
#include "Enemy.h"
#include "ShieldEnemy.h"
#include "Math.h"
#include "SoundManager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>

// KamataEngine::Vector3 用の演算子（エンジン側に未定義のため補完）
namespace KamataEngine {
inline Vector3 operator+(const Vector3& lhs, const Vector3& rhs) {
	return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}
inline Vector3 operator-(const Vector3& lhs, const Vector3& rhs) {
	return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}
inline Vector3& operator+=(Vector3& lhs, const Vector3& rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	lhs.z += rhs.z;
	return lhs;
}
} // namespace KamataEngine

using namespace KamataEngine;

// ---- ユーティリティ ----
static float EaseInOutQuad(float t) {
	return (t < 0.5f) ? 2.0f * t * t : 1.0f - std::powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}
static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
// 開始値から終了値まで、減速しながら変化する（クワッド・イーズアウト）
static float EaseOut(float start, float end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	t = 1.0f - (1.0f - t) * (1.0f - t);
	return Lerp(start, end, t);
}
// 開始値から終了値まで、加速しながら変化する（クワッド・イーズイン）
static float EaseIn(float start, float end, float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	t = t * t;
	return Lerp(start, end, t);
}

// ================================================================
//  Initialize
// ================================================================
void Player::Initialize(Model* model, uint32_t textureHandle, Model* modelAttack, Camera* camera, const Vector3& position) {
	assert(model);
	assert(camera);
	model_         = model;
	textureHandle_ = textureHandle;
	camera_        = camera;
	worldTransform_.Initialize();
	worldTransform_.translation_ = position;
	worldTransform_.rotation_.y  = std::numbers::pi_v<float> / 2.0f;

	// 初期化直後の1フレーム目から正しい位置で表示されるよう、ここで一度行列を計算しておく
	// （Update()が呼ばれるまでの間、初期化直後のデフォルト行列＝座標(0,0,0)のまま
	//  描画されてしまうのを防ぐ）
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	// エフェクト用モデル・ワールドトランスフォームの初期化
	modelAttack_ = modelAttack;
	worldTransformAttack_.Initialize();
}

// ================================================================
//  CornerPosition：データテーブルで分岐を排除
// ================================================================
Vector3 Player::CornerPosition(const Vector3& center, Corner corner) {
	Vector3 offsetTable[kNumCorner] = {
	    {+kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kRightBottom
	    {-kWidth / 2.0f, -kHeight / 2.0f, 0.0f}, // kLeftBottom
	    {+kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kRightTop
	    {-kWidth / 2.0f, +kHeight / 2.0f, 0.0f}, // kLeftTop
	};
	return center + offsetTable[static_cast<uint32_t>(corner)];
}

// ================================================================
//  ⓪ 壁・床へのめり込み解消
//  自キャラのAABB（体全体）が、周囲のブロックと重なってしまっている場合、
//  重なりが最も小さい軸方向へ最短距離で押し出す。
//  （中心座標だけを見る簡易チェックだと、体の端だけがめり込んでいる
//   ケースを見逃してしまうため、AABB同士の重なりで判定するように変更した）
//  （通常はここまでめり込むことは無いはずだが、ノックバック等で
//   一時的に強制移動させた際の保険として、毎フレーム先頭でチェックする）
// ================================================================
void Player::ResolveEmbeddedInBlock() {
	if (!mapChipField_) {
		return;
	}

	// 押し出し後、ブロックにぴったりくっつかないようにする余白
	constexpr float kPushOutMargin = 0.02f;

	// このフレームで一度でも押し出しを行ったかどうか
	// （行った場合は、天井/床判定の混乱を断ち切るため、最後に強制的に接地状態へ戻す）
	bool resolvedAny = false;

	// 一度の押し出しで解消しきれない（角に挟まっている等の）ケースに備え、
	// 複数回試行する
	for (int32_t iteration = 0; iteration < 4; ++iteration) {
		// 自キャラのAABB（体全体）
		float aabbMinX = worldTransform_.translation_.x - kWidth / 2.0f;
		float aabbMaxX = worldTransform_.translation_.x + kWidth / 2.0f;
		float aabbMinY = worldTransform_.translation_.y - kHeight / 2.0f;
		float aabbMaxY = worldTransform_.translation_.y + kHeight / 2.0f;

		// AABBが重なりうるマス目の範囲を求める
		MapChipField::IndexSet indexMin = mapChipField_->GetMapChipIndexSetByPosition({aabbMinX, aabbMinY, worldTransform_.translation_.z});
		MapChipField::IndexSet indexMax = mapChipField_->GetMapChipIndexSetByPosition({aabbMaxX, aabbMaxY, worldTransform_.translation_.z});

		uint32_t xStart = (std::min)(indexMin.xIndex, indexMax.xIndex);
		uint32_t xEnd   = (std::max)(indexMin.xIndex, indexMax.xIndex);
		uint32_t yStart = (std::min)(indexMin.yIndex, indexMax.yIndex);
		uint32_t yEnd   = (std::max)(indexMin.yIndex, indexMax.yIndex);

		// 安全策：万一座標が大きく範囲外になっても、ここで有効な範囲にクランプしておく
		// （座標のキャスト時のオーバーフロー等で、ループが暴走するのを防ぐ）
		uint32_t maxXIndex = mapChipField_->GetNumBlockHorizontal() - 1;
		uint32_t maxYIndex = mapChipField_->GetNumBlockVertical() - 1;
		xStart = (std::min)(xStart, maxXIndex);
		xEnd   = (std::min)(xEnd, maxXIndex);
		yStart = (std::min)(yStart, maxYIndex);
		yEnd   = (std::min)(yEnd, maxYIndex);

		// 最も重なりが小さい軸・その押し出し量を探す
		bool  found          = false;
		float smallestOverlap = (std::numeric_limits<float>::max)();
		float pushX          = 0.0f;
		float pushY          = 0.0f;

		for (uint32_t xi = xStart; xi <= xEnd; ++xi) {
			for (uint32_t yi = yStart; yi <= yEnd; ++yi) {
				if (mapChipField_->GetMapChipTypeByIndex(xi, yi) != MapChipType::kBlock) {
					continue;
				}

				MapChipField::Rect rect = mapChipField_->GetRectByIndex(xi, yi);

				// AABB同士の重なり量（軸ごと）
				float overlapX = (std::min)(aabbMaxX, rect.right) - (std::max)(aabbMinX, rect.left);
				float overlapY = (std::min)(aabbMaxY, rect.top) - (std::max)(aabbMinY, rect.bottom);

				if (overlapX <= 0.0f || overlapY <= 0.0f) {
					// 実際には重なっていない（隣接しているだけ）
					continue;
				}

				found = true;

				// このブロックの上または下にもブロックが積まれている（＝壁の一部）かどうか。
				// 壁にめり込んだ場合、Y方向（上下）に押し出してしまうと、
				// 積まれた隣のブロックに再びめり込んでジャンプ不能になる事故につながるため、
				// 壁の一部と判定できる場合は必ずX方向（横）優先で押し出す。
				bool isPartOfVerticalStack = (yi > 0 && mapChipField_->GetMapChipTypeByIndex(xi, yi - 1) == MapChipType::kBlock) ||
				                             (mapChipField_->GetMapChipTypeByIndex(xi, yi + 1) == MapChipType::kBlock);

				if (isPartOfVerticalStack || overlapX < overlapY) {
					float candidate = overlapX + kPushOutMargin;
					if (candidate < smallestOverlap) {
						smallestOverlap = candidate;
						float blockCenterX = (rect.left + rect.right) * 0.5f;
						pushX = (worldTransform_.translation_.x < blockCenterX) ? -candidate : candidate;
						pushY = 0.0f;
					}
				} else {
					float candidate = overlapY + kPushOutMargin;
					if (candidate < smallestOverlap) {
						smallestOverlap = candidate;
						float blockCenterY = (rect.bottom + rect.top) * 0.5f;
						pushX = 0.0f;
						pushY = (worldTransform_.translation_.y < blockCenterY) ? -candidate : candidate;
					}
				}
			}
		}

		if (!found) {
			// どこにも重なっていなければ終了
			break;
		}

		resolvedAny = true;

		// 求めた方向へ押し出す
		worldTransform_.translation_.x += pushX;
		worldTransform_.translation_.y += pushY;

		// 押し出した勢いで再度めり込むことがないよう、速度をリセットしておく
		velocity_ = {};
	}

	// このフレームで一度でも押し出しを行った場合、
	// めり込み中に発生していたかもしれない「天井に当たっている」という
	// 誤った判定を断ち切るため、強制的に接地状態・2段ジャンプ可能な状態に戻しておく。
	// （これにより、めり込み解消直後は必ずジャンプできる状態が保証される）
	if (resolvedAny) {
		onGround_      = true;
		canDoubleJump_ = true;
	}
}

// ================================================================
//  マップの左右・上方向の外に出られないようにする（見えない境界壁）
//  ※ 下方向は穴に落ちて死亡する仕様のため、あえてクランプしない
// ================================================================
void Player::ClampToMapBounds() {
	if (!mapChipField_) {
		return;
	}

	// マップの左端・右端（マスの中心基準の座標）
	const float minX = 0.0f;
	const float maxX = MapChipField::kMapWidth * static_cast<float>(mapChipField_->GetNumBlockHorizontal() - 1);

	// マップの上端。
	// 足場は下の方（マスの下側）に集中しているので、マップ最上段を境界にすれば
	// 自然と大きくマージンが取れ、ジャンプ時に圧迫感が出ない。
	const float maxY = MapChipField::kMapHeight * static_cast<float>(mapChipField_->GetNumBlockVertical() - 1);

	bool clamped = false;

	if (worldTransform_.translation_.x < minX) {
		worldTransform_.translation_.x = minX;
		velocity_.x = 0.0f;
		clamped = true;
	} else if (worldTransform_.translation_.x > maxX) {
		worldTransform_.translation_.x = maxX;
		velocity_.x = 0.0f;
		clamped = true;
	}

	if (worldTransform_.translation_.y > maxY) {
		worldTransform_.translation_.y = maxY;
		// 上限に当たった場合は上昇速度だけ止める（落下は妨げない）
		if (velocity_.y > 0.0f) {
			velocity_.y = 0.0f;
		}
		clamped = true;
	}

	if (clamped) {
		// クランプした分をワールド行列にも反映しておく
		worldTransform_.matWorld_ = Math::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
		worldTransform_.TransferMatrix();
	}
}

// ================================================================
//  ① 移動入力
// ================================================================
void Player::InputMove() {
	if (onGround_) {
		if (Input::GetInstance()->PushKey(DIK_RIGHT)) {
			if (velocity_.x < 0.0f) {
				velocity_.x *= (1.0f - kAttenuation);
			}
			velocity_.x += acceleration_;
			if (lrDirection_ != LRDirection::kRight) {
				lrDirection_       = LRDirection::kRight;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_          = kTimeTurn;
			}
		} else if (Input::GetInstance()->PushKey(DIK_LEFT)) {
			if (velocity_.x > 0.0f) {
				velocity_.x *= (1.0f - kAttenuation);
			}
			velocity_.x -= acceleration_;
			if (lrDirection_ != LRDirection::kLeft) {
				lrDirection_       = LRDirection::kLeft;
				turnFirstRotationY_ = worldTransform_.rotation_.y;
				turnTimer_          = kTimeTurn;
			}
		} else {
			velocity_.x *= (1.0f - acceleration_);
		}
		// ジャンプ
		if (Input::GetInstance()->TriggerKey(DIK_UP)) {
			velocity_.y += jumpAcceleration_;
			// ジャンプSEを再生
			SoundManager::GetInstance()->PlaySEJump();
		}
	} else {
		// 空中：重力を加算
		velocity_.y -= kGravityAcceleration;
		velocity_.y  = std::max(velocity_.y, -kMaxFallSpeed);

		// 2段ジャンプ（空中で一度だけ追加でジャンプできる）
		if (canDoubleJump_ && Input::GetInstance()->TriggerKey(DIK_UP)) {
			// 前の上昇/下降の勢いをリセットしてから跳び上がることで、
			// 常に同じ高さで2段目のジャンプができるようにする
			velocity_.y = jumpAcceleration_;
			// 2段ジャンプは使い切ったので、次に着地するまで使えなくする
			canDoubleJump_ = false;
			// ジャンプSEを再生
			SoundManager::GetInstance()->PlaySEJump();
		}
	}
	velocity_.x = std::clamp(velocity_.x, -limitRunSpeed_, limitRunSpeed_);
}

// ================================================================
//  ② マップ衝突判定
// ================================================================
void Player::CheckMapCollision(CollisionMapInfo& info) {
	// すり抜け（トンネリング）対策：
	// 1フレームでの移動量が1マス分（1.0f）以上になると、
	// 天井や床のマスを飛び越えて当たり判定を素通りしてしまう可能性がある。
	// （このクラスの当たり判定は「移動前後のマス目」しか見ていないため）
	// そのため、実際に反映する移動量を1マス未満に安全側でクランプしておく。
	// ※ 各Behaviorの速度（jumpAcceleration_等）自体も1マス未満に収まる値にしてあるが、
	//   将来的な調整ミスで再発しないよう、ここでも二重に安全策をかけている。
	constexpr float kMaxSafeStep = 0.9f; // 1マス(1.0f)未満に制限
	info.velocity.x = std::clamp(info.velocity.x, -kMaxSafeStep, kMaxSafeStep);
	info.velocity.y = std::clamp(info.velocity.y, -kMaxSafeStep, kMaxSafeStep);

	CheckMapCollisionUp(info);
	CheckMapCollisionDown(info);
	CheckMapCollisionRight(info);
	CheckMapCollisionLeft(info);
}

// ---- 上方向 ----
void Player::CheckMapCollisionUp(CollisionMapInfo& info) {
	// ガード節：上昇中でなければスキップ
	if (info.velocity.y <= 0.0f) {
		return;
	}

	// ※ ここではY移動のみを反映した位置で判定する（X移動は含めない）。
	//   X・Yを合成した斜め移動後の位置で判定すると、
	//   「壁側に入力しながらジャンプする」ケースで横のブロック（壁）が
	//   誤って天井として検出されてしまう事故につながるため。
	Vector3 verticalOnlyOffset = {0.0f, info.velocity.y, 0.0f};

	std::array<Vector3, kNumCorner> positionsNew{};
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + verticalOnlyOffset, static_cast<Corner>(i));
	}

	bool hit = false;
	for (Corner c : {kLeftTop, kRightTop}) {
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(positionsNew[c]);

		// セル境界判定：移動前後でYセル番号が変わった場合のみ判定
		MapChipField::IndexSet indexSetNow =
		    mapChipField_->GetMapChipIndexSetByPosition(CornerPosition(worldTransform_.translation_, c));
		if (indexSetNow.yIndex == indexSet.yIndex) {
			continue;
		}

		MapChipType type = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
		if (type == MapChipType::kBlock) {
			hit = true;
		}
	}

	if (hit) {
		// 自キャラ上端でめり込み先ブロックを特定
		Vector3 topPos   = worldTransform_.translation_ + verticalOnlyOffset;
		topPos.y        += kHeight / 2.0f;
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(topPos);
		MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);

		// ブロック下端 - 移動前自キャラ座標 - 自キャラ半径 - 余白
		float move     = rect.bottom - worldTransform_.translation_.y - kHeight / 2.0f - kBlank;
		info.velocity.y = std::max(0.0f, move);
		info.hitCeiling = true;
	}
}

// ---- 下方向 ----
void Player::CheckMapCollisionDown(CollisionMapInfo& info) {
	// ガード節：下降中でなければスキップ
	if (info.velocity.y >= 0.0f) {
		return;
	}

	// ※ ここではY移動のみを反映した位置で判定する（X移動は含めない）。理由はUp方向と同様。
	std::array<Vector3, kNumCorner> positionsNew{};
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		Vector3 offset = {0.0f, info.velocity.y, 0.0f};
		// 左下・右下のみ吸着補正でY座標を少し下にずらす
		if (i == kLeftBottom || i == kRightBottom) {
			offset.y -= kGroundCheckOffset;
		}
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + offset, static_cast<Corner>(i));
	}

	bool hit = false;
	for (Corner c : {kLeftBottom, kRightBottom}) {
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(positionsNew[c]);

		// セル境界判定：移動前後でYセル番号が変わった場合のみ判定
		MapChipField::IndexSet indexSetNow =
		    mapChipField_->GetMapChipIndexSetByPosition(CornerPosition(worldTransform_.translation_, c));
		if (indexSetNow.yIndex == indexSet.yIndex) {
			continue;
		}

		// 一つ上のセルもブロックなら連続した壁なので着地しない
		MapChipType typeAbove = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
		MapChipType type      = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
		if (type == MapChipType::kBlock && typeAbove != MapChipType::kBlock) {
			hit = true;
		}
	}

	if (hit) {
		Vector3 bottomPos  = worldTransform_.translation_ + Vector3{0.0f, info.velocity.y, 0.0f};
		bottomPos.y       -= kHeight / 2.0f;
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(bottomPos);
		MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);

		// ブロック上端 - 移動前自キャラ座標 + 自キャラ半径 + 余白
		float move     = rect.top - worldTransform_.translation_.y + kHeight / 2.0f + kBlank;
		info.velocity.y = std::min(0.0f, move);
		info.landed     = true;
	}
}

// ---- 右方向 ----
void Player::CheckMapCollisionRight(CollisionMapInfo& info) {
	// ガード節：右移動でなければスキップ
	if (info.velocity.x <= 0.0f) {
		return;
	}

	// ※ ここではX移動のみを反映した位置で判定する（Y移動は含めない）。
	//   上下判定と同様、斜め移動での誤判定（床や天井の誤検出）を防ぐため。
	Vector3 horizontalOnlyOffset = {info.velocity.x, 0.0f, 0.0f};

	std::array<Vector3, kNumCorner> positionsNew{};
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + horizontalOnlyOffset, static_cast<Corner>(i));
	}

	bool hit = false;
	for (Corner c : {kRightTop, kRightBottom}) {
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(positionsNew[c]);

		// セル境界判定
		MapChipField::IndexSet indexSetNow =
		    mapChipField_->GetMapChipIndexSetByPosition(CornerPosition(worldTransform_.translation_, c));
		if (indexSetNow.xIndex == indexSet.xIndex) {
			continue;
		}

		// 一つ左のセルもブロックなら連続した壁なので無視
		MapChipType typeLeft = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex - 1, indexSet.yIndex);
		MapChipType type     = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
		if (type == MapChipType::kBlock && typeLeft != MapChipType::kBlock) {
			hit = true;
		}
	}

	if (hit) {
		Vector3 rightPos  = worldTransform_.translation_ + horizontalOnlyOffset;
		rightPos.x       += kWidth / 2.0f;
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(rightPos);
		MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);

		float move      = rect.left - worldTransform_.translation_.x - kWidth / 2.0f - kBlank;
		info.velocity.x = std::max(0.0f, move);
		info.hitWall    = true;
	}
}

// ---- 左方向 ----
void Player::CheckMapCollisionLeft(CollisionMapInfo& info) {
	// ガード節：左移動でなければスキップ
	if (info.velocity.x >= 0.0f) {
		return;
	}

	// ※ ここではX移動のみを反映した位置で判定する（Y移動は含めない）。理由は右方向と同様。
	Vector3 horizontalOnlyOffset = {info.velocity.x, 0.0f, 0.0f};

	std::array<Vector3, kNumCorner> positionsNew{};
	for (uint32_t i = 0; i < positionsNew.size(); ++i) {
		positionsNew[i] = CornerPosition(worldTransform_.translation_ + horizontalOnlyOffset, static_cast<Corner>(i));
	}

	bool hit = false;
	for (Corner c : {kLeftTop, kLeftBottom}) {
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(positionsNew[c]);

		// セル境界判定
		MapChipField::IndexSet indexSetNow =
		    mapChipField_->GetMapChipIndexSetByPosition(CornerPosition(worldTransform_.translation_, c));
		if (indexSetNow.xIndex == indexSet.xIndex) {
			continue;
		}

		// 一つ右のセルもブロックなら連続した壁なので無視
		MapChipType typeRight = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex + 1, indexSet.yIndex);
		MapChipType type      = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
		if (type == MapChipType::kBlock && typeRight != MapChipType::kBlock) {
			hit = true;
		}
	}

	if (hit) {
		Vector3 leftPos  = worldTransform_.translation_ + horizontalOnlyOffset;
		leftPos.x       -= kWidth / 2.0f;
		MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(leftPos);
		MapChipField::Rect rect = mapChipField_->GetRectByIndex(indexSet.xIndex, indexSet.yIndex);

		float move      = rect.right - worldTransform_.translation_.x + kWidth / 2.0f + kBlank;
		info.velocity.x = std::min(0.0f, move);
		info.hitWall    = true;
	}
}

// ================================================================
//  ③ 判定結果を反映して移動させる
// ================================================================
void Player::ApplyCollisionResult(const CollisionMapInfo& info) {
	worldTransform_.translation_ += info.velocity;
}

// ================================================================
//  ④ 天井に接触している場合の処理
// ================================================================
void Player::OnCeilingCollision(const CollisionMapInfo& info) {
	if (info.hitCeiling) {
		DebugText::GetInstance()->ConsolePrintf("hit ceiling\n");
		velocity_.y = 0.0f;
	}
}

// ================================================================
//  ⑤ 壁に接触している場合の処理
// ================================================================
void Player::OnWallCollision(const CollisionMapInfo& info) {
	if (info.hitWall) {
		// 壁接触時は毎フレーム速度を減衰させる
		velocity_.x *= (1.0f - kAttenuationWall);
	}
}

// ================================================================
//  ⑥ 接地状態の切り替え
// ================================================================
void Player::UpdateGroundState(const CollisionMapInfo& info) {
	if (onGround_) {
		// Y速度が上向き = ジャンプ開始 → 空中へ
		if (velocity_.y > 0.0f) {
			onGround_ = false;
		} else {
			// 床がなくなっているか確認（吸着補正あり）
			bool onFloor = false;
			for (Corner c : {kLeftBottom, kRightBottom}) {
				Vector3 pos = CornerPosition(worldTransform_.translation_, c);
				pos.y -= kGroundCheckOffset;
				MapChipField::IndexSet indexSet = mapChipField_->GetMapChipIndexSetByPosition(pos);
				MapChipType type      = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex);
				MapChipType typeAbove = mapChipField_->GetMapChipTypeByIndex(indexSet.xIndex, indexSet.yIndex - 1);
				if (type == MapChipType::kBlock && typeAbove != MapChipType::kBlock) {
					onFloor = true;
				}
			}
			if (!onFloor) {
				onGround_ = false;
			}
		}
	} else {
		// 空中状態：着地フラグが立ったら接地状態へ
		if (info.landed) {
			DebugText::GetInstance()->ConsolePrintf("landed\n");
			velocity_.x *= (1.0f - kAttenuationLanding);
			velocity_.y  = 0.0f;
			onGround_    = true;
			// 着地したので2段ジャンプを再び使えるようにする
			canDoubleJump_ = true;
		}
	}
}

// ================================================================
//  Update
// ================================================================
void Player::Update() {
	// ⓪ 壁・床へのめり込みを解消（毎フレーム先頭でチェック）
	ResolveEmbeddedInBlock();

	// 被弾後の無敵時間を減らす
	if (hitInvincibleTimer_ > 0.0f) {
		hitInvincibleTimer_ -= 1.0f / 60.0f;
		if (hitInvincibleTimer_ < 0.0f) {
			hitInvincibleTimer_ = 0.0f;
		}
	}

	// ダッシュ回数の自動回復（満タンでなければ時間経過でストックが増える）
	if (dashCharges_ < maxDashCharges_) {
		dashRechargeTimer_ += 1.0f / 60.0f;
		if (dashRechargeTimer_ >= dashRechargeTime_) {
			++dashCharges_;
			// 経過分だけ差し引く（回復時間が短い機体でも取りこぼさないように）
			dashRechargeTimer_ -= dashRechargeTime_;
		}
	}

	// 穴に落ちて画面外まで落下していたら死亡扱いにする
	// （敵との接触以外に、穴に落ちた場合もデス演出へつなげるため。
	//   落下は「体力を問わず即座に致命的」な扱いとし、無敵時間の影響も受けない）
	if (!isDead_ && worldTransform_.translation_.y < kFallDeathY) {
		isDead_ = true;
	}

	// 外部からのノックバック要求を処理
	// （他のふるまいへの切り替えリクエストと競合する場合の兼ね合いをここで解決する）
	if (knockbackRequest_) {
		behaviorRequest_ = Behavior::kKnockback;
		// フラグをリセット
		knockbackRequest_ = false;
	}

	if (behaviorRequest_ != Behavior::kUnknown) {
		//ふるまいを変更する
		behavior_ = behaviorRequest_;
		//各振る舞いごとに初期化を実行
		switch(behavior_) {
		case Behavior::kRoot:
			// 通常行動の初期化処理
			BehaviorRootInitialize();
			break;

		case Behavior::kDash:
			// ダッシュ行動の初期化処理
			BehaviorDashInitialize();
			break;

		case Behavior::kAttack:
			// 攻撃行動の初期化処理
			BehaviorAttackInitialize();
			break;

		case Behavior::kKnockback:
			// ノックバック行動の初期化処理
			BehaviorKnockbackInitialize();
			break;
		}

		behaviorRequest_ = Behavior::kUnknown;
	}

	// 現在のビヘイビアに応じて更新処理を分岐
	switch (behavior_) {
	case Behavior::kRoot:
	default:
		BehaviorRootUpdate();
		break;

	case Behavior::kDash:
		BehaviorDashUpdate();
		break;

	case Behavior::kAttack:
		BehaviorAttackUpdate();
		break;

	case Behavior::kKnockback:
		BehaviorKnockbackUpdate();
		break;
	}

	// マップの左右・上方向の外に出られないようにする（見えない境界壁）
	// ※ どのビヘイビアで移動した後でも、最終的にここで一括してチェックする
	ClampToMapBounds();
}

// ================================================================
//  通常行動更新
// ================================================================
void Player::BehaviorRootUpdate() {
	// ① 移動入力
	InputMove();

	// ② 衝突情報を初期化し、衝突判定を行う
	CollisionMapInfo collisionInfo;
	collisionInfo.velocity = velocity_;

	if (mapChipField_) {
		CheckMapCollision(collisionInfo);
	}

	// すり抜け防止のクランプ等で実際のY移動量が変わった場合に備えて、
	// 内部のY速度もここで同期しておく（次フレームの重力計算のズレを防ぐため）
	// ※ X速度は壁接触時の減衰処理(OnWallCollision)が生の速度を前提にしているため、
	//   ここでは同期しない。
	velocity_.y = collisionInfo.velocity.y;

	// ③ 判定結果を反映して移動
	ApplyCollisionResult(collisionInfo);

	// ④ 天井接触処理
	OnCeilingCollision(collisionInfo);

	// ⑤ 壁接触処理
	OnWallCollision(collisionInfo);

	// ⑥ 接地状態の切り替え
	UpdateGroundState(collisionInfo);

	// ⑦ 旋回制御
	if (turnTimer_ > 0.0f) {
		turnTimer_ -= 1.0f / 60.0f;
		float destinationRotationYTable[] = {
		    std::numbers::pi_v<float> / 2.0f,
		    std::numbers::pi_v<float> * 3.0f / 2.0f,
		};
		float destinationRotationY = destinationRotationYTable[static_cast<uint32_t>(lrDirection_)];
		float t = 1.0f - (turnTimer_ / kTimeTurn);
		worldTransform_.rotation_.y =
		    Lerp(turnFirstRotationY_, destinationRotationY, EaseInOutQuad(t));
	}

	// ⑧ 行動への切り替えリクエスト
	// SPACE：ダッシュ（旧・攻撃のモーションを流用。突進+無敵。攻撃判定は持たない）
	//        ※ ストック制。残り回数が無いと発動できず、時間経過で1回ずつ回復する
	// Z　  ：攻撃（その場で繰り出す。移動しない。前方2マス分に攻撃判定を持つ）
	if (behaviorRequest_ == Behavior::kUnknown) {
		if (Input::GetInstance()->TriggerKey(DIK_SPACE) && dashCharges_ > 0) {
			behaviorRequest_ = Behavior::kDash;
			// ダッシュを1回分消費する
			--dashCharges_;
			// ダッシュSEを再生
			SoundManager::GetInstance()->PlaySEDash();
		} else if (Input::GetInstance()->TriggerKey(DIK_Z)) {
			behaviorRequest_ = Behavior::kAttack;
			// 攻撃SEを再生
			SoundManager::GetInstance()->PlaySEAttack();
		}
	}

	// ⑨ 行列計算
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

// ================================================================
//  ダッシュ行動更新（溜め→突進→余韻のサブフェーズを状態遷移で管理）
//  ※旧・攻撃行動のモーションをそのまま流用。攻撃判定は持たず、移動と無敵のみ提供する。
// ================================================================
void Player::BehaviorDashUpdate() {
	// 経過フレームを進める
	++dashParameter_;

	// ダッシュ動作用の速度（突進のときだけ値が入る）
	Vector3 velocity{};

	// ダッシュフェーズごとの更新処理
	switch (dashPhase_) {
		// 溜め動作
	case DashPhase::kCharge:
	default: {
		float t = static_cast<float>(dashParameter_) / kDashChargeTime;
		worldTransform_.scale_.z = EaseOut(1.0f, 0.3f, t);
		worldTransform_.scale_.y = EaseOut(1.0f, 1.6f, t);
		// 突進動作へ移行
		if (dashParameter_ >= kDashChargeTime) {
			dashPhase_      = DashPhase::kRush;
			dashParameter_  = 0; // カウンターをリセット
		}
		break;
	}
		// 突進動作
	case DashPhase::kRush: {
		float t = static_cast<float>(dashParameter_) / kDashRushTime;
		worldTransform_.scale_.z = EaseOut(0.3f, 1.3f, t);
		worldTransform_.scale_.y = EaseIn(1.6f, 0.7f, t);

		// 突進中だけ、向いている方向に決まった速度で移動する
		if (lrDirection_ == LRDirection::kRight) {
			velocity = {+dashVelocity_, 0.0f, 0.0f};
		} else {
			velocity = {-dashVelocity_, 0.0f, 0.0f};
		}

		// 余韻動作へ移行
		if (dashParameter_ >= kDashRushTime) {
			dashPhase_      = DashPhase::kAfterglow;
			dashParameter_  = 0; // カウンターをリセット
		}
		break;
	}
		// 余韻動作
	case DashPhase::kAfterglow: {
		float t = static_cast<float>(dashParameter_) / kDashAfterglowTime;
		worldTransform_.scale_.z = EaseOut(1.3f, 1.0f, t);
		worldTransform_.scale_.y = EaseOut(0.7f, 1.0f, t);

		// 最後のダッシュフェーズが終わったら通常状態へ戻す
		if (dashParameter_ >= kDashAfterglowTime) {
			behaviorRequest_ = Behavior::kRoot;
		}
		break;
	}
	}

	// 衝突情報を初期化（突進中以外はvelocityが0なので実質移動しない）
	CollisionMapInfo collisionInfo;
	collisionInfo.velocity = velocity;

	if (mapChipField_) {
		CheckMapCollision(collisionInfo);
	}

	// 判定結果を反映して移動
	ApplyCollisionResult(collisionInfo);

	// エフェクト用ワールドトランスフォームへ位置・向きをコピー
	worldTransformAttack_.translation_ = worldTransform_.translation_;
	worldTransformAttack_.rotation_    = worldTransform_.rotation_;

	// 行列計算
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	worldTransformAttack_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformAttack_.scale_, worldTransformAttack_.rotation_, worldTransformAttack_.translation_);
	worldTransformAttack_.TransferMatrix();

	// ダッシュ中（突進フェーズ）だけ、背後にパーティクルを発生させる
	if (dashPhase_ == DashPhase::kRush) {
		SpawnDashTrailParticle();
	}
	// 発生済みのパーティクルは、突進フェーズ以外でもフェードアウトを継続させる
	UpdateDashTrailParticles();
}

// ================================================================
//  攻撃行動更新（その場で繰り出す。移動しない。前方2マス分に攻撃判定を持つ）
// ================================================================
void Player::BehaviorAttackUpdate() {
	// 経過フレームを進める
	++attackTimer_;

	// 「振り」の勢いを表現するため、移動はせずスケールだけ一瞬伸縮させる
	float t = static_cast<float>(attackTimer_) / kAttackDuration;
	worldTransform_.scale_.z = 1.0f + 0.4f * std::sinf(t * std::numbers::pi_v<float>);

	// 攻撃演出が終わったら通常状態へ戻す
	if (attackTimer_ >= kAttackDuration) {
		behaviorRequest_ = Behavior::kRoot;
	}

	// 移動はしない（velocity=0のまま）。マップ衝突関連は一貫性のため空処理として通しておく。
	CollisionMapInfo collisionInfo;
	collisionInfo.velocity = {};

	if (mapChipField_) {
		CheckMapCollision(collisionInfo);
	}

	ApplyCollisionResult(collisionInfo);

	// エフェクト用ワールドトランスフォームへ位置・向きをコピー
	worldTransformAttack_.translation_ = worldTransform_.translation_;
	worldTransformAttack_.rotation_    = worldTransform_.rotation_;

	// 行列計算
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();

	worldTransformAttack_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransformAttack_.scale_, worldTransformAttack_.rotation_, worldTransformAttack_.translation_);
	worldTransformAttack_.TransferMatrix();
}

void Player::BehaviorRootInitialize() {
	// 通常行動へ戻るときはスケールを元に戻しておく
	worldTransform_.scale_ = {1.0f, 1.0f, 1.0f};
}

void Player::BehaviorDashInitialize() {
	// ダッシュフェーズを最初（溜め）から開始
	dashPhase_     = DashPhase::kCharge;
	dashParameter_ = 0;

	// 追従カメラの速度加算にダッシュ移動が影響しないよう、メンバのvelocity_をゼロクリアしておく
	velocity_ = {};
}

void Player::BehaviorAttackInitialize() {
	// 攻撃演出の経過フレームをリセット
	attackTimer_ = 0;

	// その場攻撃なので、念のため速度をゼロクリアしておく
	velocity_ = {};

	// 遠距離攻撃機体（ヘヴィボット等）の場合、攻撃開始と同時に発射フラグを立てる
	// （実際の弾の生成はGameScene側がConsumeJustFiredRangedAttack()で受け取って行う）
	if (isRangedAttack_) {
		justFiredRangedAttack_ = true;
	}
}

// ================================================================
//  ノックバック行動初期化
// ================================================================
void Player::BehaviorKnockbackInitialize() {
	// 「弾き飛ばされる」フェーズから開始
	knockbackPhase_     = KnockbackPhase::kFly;
	knockbackParameter_ = 0;

	// 向いている方向と逆向きに、強い初速で弾き飛ばす
	velocity_.x = (lrDirection_ == LRDirection::kRight) ? -kKnockbackInitialSpeed : kKnockbackInitialSpeed;
	velocity_.y = 0.0f;
}

// ================================================================
//  ノックバック行動更新（弾き飛ばされる→体勢を立て直す、の2フェーズ）
// ================================================================
void Player::BehaviorKnockbackUpdate() {
	// 経過フレームを進める
	++knockbackParameter_;

	switch (knockbackPhase_) {
		// 強い初速で弾き飛ばされるフェーズ
	case KnockbackPhase::kFly:
		// 摩擦で徐々に減速させる
		velocity_.x *= (1.0f - kAttenuation);

		// 体勢を立て直すフェーズへ移行
		if (knockbackParameter_ >= kKnockbackFlyTime) {
			knockbackPhase_     = KnockbackPhase::kRecover;
			knockbackParameter_ = 0; // カウンターをリセット
			velocity_.x         = 0.0f; // 弾き飛ばし終了、その場で停止させる
		}
		break;

		// 移動が停止し、体勢を立て直すフェーズ（硬直時間）
	case KnockbackPhase::kRecover:
		if (knockbackParameter_ >= kKnockbackRecoverTime) {
			// 立て直しが終わったら通常状態へ戻す
			behaviorRequest_ = Behavior::kRoot;
		}
		break;
	}

	// 衝突情報を初期化し、衝突判定を行う（壁や床にめり込まないようにする）
	CollisionMapInfo collisionInfo;
	collisionInfo.velocity = velocity_;

	if (mapChipField_) {
		CheckMapCollision(collisionInfo);
	}

	// 判定結果を反映して移動
	ApplyCollisionResult(collisionInfo);

	// 天井・壁接触処理、接地状態の切り替え
	OnCeilingCollision(collisionInfo);
	OnWallCollision(collisionInfo);
	UpdateGroundState(collisionInfo);

	// 行列計算
	worldTransform_.matWorld_ = Math::MakeAffineMatrix(
	    worldTransform_.scale_, worldTransform_.rotation_, worldTransform_.translation_);
	worldTransform_.TransferMatrix();
}

// ================================================================
//  Draw
// ================================================================
void Player::Draw() {
	// 被弾後の無敵時間中は点滅させる（一定間隔で表示をスキップする）
	if (IsBlinkVisible()) {
		model_->Draw(worldTransform_, *camera_);
	}

	// 近接攻撃中のみ、攻撃エフェクトモデルを一緒に描画する
	// （ダッシュ中は攻撃エフェクトを出さない。ダッシュの演出は背後の噴射パーティクルのみで表現する）
	// （遠距離攻撃機体は近接攻撃を持たないため、振りのエフェクトも出さない。弾自体の演出のみで表現する）
	if (behavior_ == Behavior::kAttack && !isRangedAttack_ && modelAttack_) {
		modelAttack_->Draw(worldTransformAttack_, *camera_);
	}

	// ダッシュ中の背後パーティクルの描画
	DrawDashTrailParticles();
}

// ================================================================
//  被弾後の無敵時間中、今のフレームでモデルを表示すべきか（点滅用）
// ================================================================
bool Player::IsBlinkVisible() const {
	// 無敵時間中でなければ常に表示する
	if (hitInvincibleTimer_ <= 0.0f) {
		return true;
	}

	// 残り時間を一定間隔で区切り、偶数区間だけ表示することで点滅させる
	int32_t interval = static_cast<int32_t>(hitInvincibleTimer_ / kBlinkInterval);
	return (interval % 2) == 0;
}

// ================================================================
//  ワールド座標を取得
// ================================================================
Vector3 Player::GetWorldPosition() const {
	// ワールド座標を入れる変数
	Vector3 worldPos;
	// ワールド行列の平行移動成分を取得（ワールド座標）
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}

// ================================================================
//  AABBを取得
// ================================================================
AABB Player::GetAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
	aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

	return aabb;
}

// ================================================================
//  攻撃判定用AABBを取得（前方2マス分。攻撃中のみ意味を持つ）
// ================================================================
AABB Player::GetAttackAABB() {
	Vector3 worldPos = GetWorldPosition();

	AABB aabb;

	if (lrDirection_ == LRDirection::kRight) {
		// 自キャラの右端から、さらに前方attackRange_分の範囲
		aabb.min = {worldPos.x + kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
		aabb.max = {worldPos.x + kWidth / 2.0f + attackRange_, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};
	} else {
		// 自キャラの左端から、さらに前方attackRange_分の範囲
		aabb.min = {worldPos.x - kWidth / 2.0f - attackRange_, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
		aabb.max = {worldPos.x - kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};
	}

	return aabb;
}

// ================================================================
//  衝突応答（自キャラ側）
// ================================================================
void Player::OnCollision(const Enemy* enemy) {
	(void)enemy;
	// ダッシュ中・攻撃中・被弾後の無敵時間中はダメージ無効
	if (IsInvincible()) {
		return;
	}
	// 被弾したことを記録し、無敵時間（点滅）を開始する。
	// 実際に体力を減らすか・死亡させるかはGameScene側が体力を見て判断する。
	justTookDamage_     = true;
	hitInvincibleTimer_ = kHitInvincibleDuration;
}

void Player::OnCollision(const ShieldEnemy* shieldEnemy) {
	(void)shieldEnemy;
	// ダッシュ中・攻撃中・被弾後の無敵時間中はダメージ無効
	if (IsInvincible()) {
		return;
	}
	// 被弾したことを記録し、無敵時間（点滅）を開始する。
	// 実際に体力を減らすか・死亡させるかはGameScene側が体力を見て判断する。
	justTookDamage_     = true;
	hitInvincibleTimer_ = kHitInvincibleDuration;
}

// ================================================================
//  ダッシュ中の背後パーティクル（トレイル）
// ================================================================
void Player::SpawnDashTrailParticle() {
	// モデルが設定されていなければ何もしない
	if (!modelDashTrail_) {
		return;
	}

	// 発生間隔タイマーを進める
	dashTrailSpawnTimer_ += 1.0f / 60.0f;
	if (dashTrailSpawnTimer_ < kDashTrailSpawnInterval) {
		return;
	}
	dashTrailSpawnTimer_ = 0.0f;

	// 非アクティブなパーティクルを1つ探して使い回す
	for (TrailParticle& particle : dashTrailParticles_) {
		if (particle.active) {
			continue;
		}

		// 進行方向の逆（背後）に少しオフセットした位置から発生させる
		float direction = IsFacingRight() ? 1.0f : -1.0f;
		Vector3 spawnPos = worldTransform_.translation_;
		spawnPos.x -= direction * (kWidth / 2.0f);

		particle.worldTransform.Initialize();
		particle.worldTransform.scale_       = {kDashTrailScale, kDashTrailScale, kDashTrailScale};
		particle.worldTransform.translation_ = spawnPos;

		particle.objectColor.Initialize();
		particle.color  = {1.0f, 1.0f, 1.0f, 1.0f};
		particle.timer  = 0.0f;
		particle.active = true;
		break;
	}
}

void Player::UpdateDashTrailParticles() {
	for (TrailParticle& particle : dashTrailParticles_) {
		if (!particle.active) {
			continue;
		}

		// 経過時間を進める
		particle.timer += 1.0f / 60.0f;
		float t = std::clamp(particle.timer / kDashTrailLifeTime, 0.0f, 1.0f);

		// 時間経過とともに縮小させながらフェードアウトさせる
		float scale = kDashTrailScale * (1.0f - t);
		particle.worldTransform.scale_ = {scale, scale, scale};
		particle.color.w               = 1.0f - t;
		particle.objectColor.SetColor(particle.color);

		// 行列の更新
		particle.worldTransform.matWorld_ = Math::MakeAffineMatrix(
		    particle.worldTransform.scale_, particle.worldTransform.rotation_, particle.worldTransform.translation_);
		particle.worldTransform.TransferMatrix();

		// 寿命が尽きたら非アクティブに戻す
		if (particle.timer >= kDashTrailLifeTime) {
			particle.active = false;
		}
	}
}

void Player::DrawDashTrailParticles() {
	if (!modelDashTrail_) {
		return;
	}

	for (TrailParticle& particle : dashTrailParticles_) {
		if (!particle.active) {
			continue;
		}
		modelDashTrail_->Draw(particle.worldTransform, *camera_, &particle.objectColor);
	}
}
