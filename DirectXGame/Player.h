#pragma once
#include "kamataEngine.h"
#include "MapChipField.h"
#include "AABB.h"
#include "CharacterData.h"
#include <array>

// 前方宣言（循環参照回避）
class Enemy;
class ShieldEnemy;

class Player {
public:
	void Initialize(
	    KamataEngine::Model* model, uint32_t textureHandle, KamataEngine::Model* modelAttack, KamataEngine::Camera* camera,
	    const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	// ダッシュ中の背後パーティクル用モデルをセット（"jetTrail" 専用モデル。Enemy/ShieldEnemyと共通）
	void SetDashTrailModel(KamataEngine::Model* model) { modelDashTrail_ = model; }
	const KamataEngine::WorldTransform& GetWorldTransform() const { return worldTransform_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }

	// ワールド座標を取得
	KamataEngine::Vector3 GetWorldPosition() const;

	// AABBを取得
	AABB GetAABB();

	// 攻撃判定用AABBを取得（前方2マス分。攻撃中のみ意味を持つ）
	AABB GetAttackAABB();

	// 衝突応答
	void OnCollision(const Enemy* enemy);
	void OnCollision(const ShieldEnemy* shieldEnemy);

	// デスフラグのゲッター
	bool IsDead() const { return isDead_; }

	// 体力を0にして、実際に死亡させる（HPをどう扱うかはGameScene側が管理しているため、
	// 「死亡させるかどうか」の最終判断はGameScene側で行い、ここではその実行だけを担う）
	void Kill() { isDead_ = true; }

	// このフレームで被弾したかどうかを取得し、フラグをリセットする
	// （実際に体力を減らすか・死亡させるかはGameScene側が判断するため、
	//   Player側では「無敵でなければ被弾した」ことを記録するだけにとどめている）
	bool ConsumeJustTookDamage() {
		bool v          = justTookDamage_;
		justTookDamage_ = false;
		return v;
	}

	// 攻撃中かどうかのゲッター（その場攻撃中のみtrue。敵への攻撃判定に使う）
	bool IsAttack() const { return behavior_ == Behavior::kAttack; }

	// ダッシュ中かどうかのゲッター
	bool IsDash() const { return behavior_ == Behavior::kDash; }

	// 無敵中かどうかのゲッター（ダッシュ中・攻撃中・被弾後の無敵時間中は無敵。被ダメージ判定に使う）
	// ※ 攻撃中も無敵にしないと、近距離で攻撃した際に敵の体との接触判定で
	//   「敵を倒すと同時に自分もダメージを受けて死ぬ」という事故が起きるため。
	bool IsInvincible() const {
		return behavior_ == Behavior::kDash || behavior_ == Behavior::kAttack || hitInvincibleTimer_ > 0.0f;
	}

	// 右を向いているかどうかのゲッター（敵の正面判定などに使う）
	bool IsFacingRight() const { return lrDirection_ == LRDirection::kRight; }

	// 外部（敵側）からノックバックを要求する
	// ※ 敵側のOnCollision()はconstポインタで呼ばれることが多いため、
	//    フラグを立てるだけのこの関数を呼ぶには非constポインタが必要になる点に注意。
	void RequestKnockback() { knockbackRequest_ = true; }

	// 機体（キャラクター）ごとの性能をセットする
	// ※ Initialize()の前後どちらで呼んでも良い（Initialize()側はこれらの値をリセットしない）
	void SetCharacterStats(const CharacterStats& stats) {
		acceleration_     = stats.acceleration;
		limitRunSpeed_    = stats.limitRunSpeed;
		jumpAcceleration_ = stats.jumpAcceleration;
		dashVelocity_     = stats.dashVelocity;
		attackRange_      = stats.attackRange;
		maxDashCharges_   = stats.maxDashCharges;
		dashRechargeTime_ = stats.dashRechargeTime;
		isRangedAttack_   = stats.isRangedAttack;
		// 機体を切り替えた際は、ダッシュ回数を満タンにしておく
		dashCharges_      = maxDashCharges_;
		dashRechargeTimer_ = 0.0f;
	}

	// 遠距離攻撃機体かどうかのgetter（true: ヘヴィボット等、弾を飛ばすタイプ）
	bool IsRangedAttacker() const { return isRangedAttack_; }

	// このフレームで遠距離攻撃を発射したかどうかを取得し、フラグをリセットする
	// （実際に弾を生成するのはGameScene側。Player側は「発射した」ことを記録するだけ）
	bool ConsumeJustFiredRangedAttack() {
		bool v                     = justFiredRangedAttack_;
		justFiredRangedAttack_     = false;
		return v;
	}

	// 遠距離攻撃の飛距離のgetter（弾生成時にGameScene側から使う）
	float GetAttackRange() const { return attackRange_; }

	// 現在のダッシュ使用可能回数のgetter（HUD表示等に使う）
	int32_t GetDashCharges() const { return dashCharges_; }

	// ダッシュの最大使用可能回数のgetter（HUD表示等に使う）
	int32_t GetMaxDashCharges() const { return maxDashCharges_; }


private:
	// --- 当たり判定サイズ ---
	static inline const float kWidth             = 0.8f;
	static inline const float kHeight            = 0.8f;
	// めり込み排除用微小余白
	static inline const float kBlank             = 0.001f;
	// 着地判定を少し下にずらす吸着補正
	static inline const float kGroundCheckOffset = 0.02f;

	// --- 角のenum ---
	enum Corner {
		kRightBottom,
		kLeftBottom,
		kRightTop,
		kLeftTop,
		kNumCorner,
	};

	enum class Behavior {
		kRoot,      // 通常行動
		kDash,      // ダッシュ（旧・攻撃のモーションを流用。突進+無敵。攻撃判定は持たない）
		kAttack,    // 攻撃行動（その場で繰り出す。前方2マス分に攻撃判定を持つ。移動しない）
		kKnockback, // ノックバック行動
		kUnknown,   //リクエストなし
	};

	// --- ダッシュフェーズ（型） ---
	// （旧AttackPhaseを、ダッシュ専用のフェーズとしてそのまま流用）
	enum class DashPhase {
		kCharge,    // 溜め
		kRush,      // 突進
		kAfterglow, // 余韻
	};

	// --- ノックバックフェーズ（型） ---
	enum class KnockbackPhase {
		kFly,     // 強い初速で弾き飛ばされる
		kRecover, // 移動が停止し、体勢を立て直す
	};

	// --- マップ衝突情報 ---
	struct CollisionMapInfo {
		bool hitCeiling  = false; // 天井衝突フラグ
		bool landed      = false; // 着地フラグ
		bool hitWall     = false; // 壁接触フラグ
		KamataEngine::Vector3 velocity;  // 移動量
	};

	// --- 内部関数 ---
	KamataEngine::Vector3 CornerPosition(const KamataEngine::Vector3& center, Corner corner);

	// ⓪ 壁・床へのめり込み解消（毎フレーム先頭で実行し、埋まっていたら押し出す）
	void ResolveEmbeddedInBlock();

	// マップの左右・上方向の外に出られないようにする（見えない境界壁）
	// ※ 下方向は穴に落ちて死亡する仕様のため、あえてクランプしない
	void ClampToMapBounds();

	// ① 移動入力
	void InputMove();
	// ② 衝突判定
	void CheckMapCollision(CollisionMapInfo& info);
	void CheckMapCollisionUp(CollisionMapInfo& info);
	void CheckMapCollisionDown(CollisionMapInfo& info);
	void CheckMapCollisionRight(CollisionMapInfo& info);
	void CheckMapCollisionLeft(CollisionMapInfo& info);
	// ③ 判定結果を反映して移動
	void ApplyCollisionResult(const CollisionMapInfo& info);
	// ④ 天井接触
	void OnCeilingCollision(const CollisionMapInfo& info);
	// ⑤ 壁接触
	void OnWallCollision(const CollisionMapInfo& info);
	// ⑥ 接地状態の切り替え
	void UpdateGroundState(const CollisionMapInfo& info);

	// 通常行動更新
	void BehaviorRootUpdate();
	// ダッシュ行動更新（旧・攻撃。突進+無敵。攻撃判定は持たない）
	void BehaviorDashUpdate();
	// 攻撃行動更新（その場で繰り出す。移動しない。前方2マス分に攻撃判定を持つ）
	void BehaviorAttackUpdate();
	// ノックバック行動更新
	void BehaviorKnockbackUpdate();

	void BehaviorRootInitialize();

	void BehaviorDashInitialize();

	void BehaviorAttackInitialize();

	void BehaviorKnockbackInitialize();


	// --- メンバ変数 ---
	enum class LRDirection { kRight, kLeft };
	LRDirection lrDirection_ = LRDirection::kRight;
	Behavior behavior_ = Behavior::kRoot;
	Behavior behaviorRequest_ = Behavior::kUnknown;


	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model*  model_         = nullptr;
	uint32_t              textureHandle_ = 0;
	KamataEngine::Camera* camera_        = nullptr;
	MapChipField*         mapChipField_  = nullptr;

	KamataEngine::Vector3 velocity_ = {};

	// --- ダッシュフェーズ（変数） ---
	// （旧・攻撃フェーズをそのままダッシュ用として流用）
	DashPhase dashPhase_     = DashPhase::kCharge;
	// 現在のダッシュフェーズ内での経過フレーム（アニメーション用カウンター）
	uint32_t  dashParameter_ = 0;

	// 各ダッシュフェーズの時間（フレーム）
	static inline const uint32_t kDashChargeTime    = 10; // 溜め動作時間
	static inline const uint32_t kDashRushTime      = 15; // 突進動作時間
	static inline const uint32_t kDashAfterglowTime = 10; // 余韻動作時間

	// 突進動作時の速度（機体ごとに変わる。デフォルトはバランサー相当）
	float dashVelocity_ = 0.3f;

	// --- ダッシュ中の背後パーティクル（トレイル） ---
	// 1個のパーティクル情報
	struct TrailParticle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor    objectColor;
		KamataEngine::Vector4        color  = {1.0f, 1.0f, 1.0f, 1.0f};
		float                        timer  = 0.0f;
		bool                         active = false;
	};

	// ダッシュ中パーティクル用モデル（借りてくる用）
	KamataEngine::Model* modelDashTrail_ = nullptr;

	// パーティクルの最大同時発生数
	static inline const uint32_t kNumDashTrailParticles = 8;
	// パーティクルを発生させる間隔[秒]
	static inline const float kDashTrailSpawnInterval = 0.03f;
	// パーティクル1個あたりの寿命[秒]
	static inline const float kDashTrailLifeTime = 0.3f;
	// パーティクルの大きさ
	static inline const float kDashTrailScale = 0.8f;

	// パーティクルの配列
	std::array<TrailParticle, kNumDashTrailParticles> dashTrailParticles_;
	// 次のパーティクルを発生させるまでのタイマー
	float dashTrailSpawnTimer_ = 0.0f;

	// ダッシュ中パーティクルを1つ発生させる
	void SpawnDashTrailParticle();
	// ダッシュ中パーティクル全体を更新する
	void UpdateDashTrailParticles();
	// ダッシュ中パーティクルを描画する
	void DrawDashTrailParticles();

	// --- ダッシュ回数（ストック制）関連 ---
	// 現在使用可能なダッシュ回数
	int32_t dashCharges_ = 2;
	// ダッシュの最大使用可能回数（機体ごとに変わる）
	int32_t maxDashCharges_ = 2;
	// ダッシュ1回分が回復するまでの時間[秒]（機体ごとに変わる）
	float dashRechargeTime_ = 1.5f;
	// 次の1回分が回復するまでの経過時間[秒]
	float dashRechargeTimer_ = 0.0f;

	// --- 攻撃（その場攻撃）関連（変数） ---
	// 攻撃の経過フレーム
	uint32_t attackTimer_ = 0;
	// 攻撃演出の時間（フレーム）
	static inline const uint32_t kAttackDuration = 12;
	// 攻撃判定の届く範囲（マス数換算。機体ごとに変わる。デフォルトはバランサー相当）
	float attackRange_ = 2.0f;

	// 遠距離攻撃機体かどうか（機体ごとに変わる。trueならその場近接ではなく弾を飛ばす）
	bool isRangedAttack_ = false;
	// このフレームで遠距離攻撃を発射したかどうか（GameScene側が消費して弾を生成する）
	bool justFiredRangedAttack_ = false;

	// --- ノックバック関連（変数） ---
	// 外部からのノックバック要求フラグ
	bool knockbackRequest_ = false;
	// 現在のノックバックフェーズ
	KnockbackPhase knockbackPhase_ = KnockbackPhase::kFly;
	// 現在のノックバックフェーズ内での経過フレーム
	uint32_t knockbackParameter_ = 0;

	// 弾き飛ばされるフェーズの時間（フレーム）
	static inline const uint32_t kKnockbackFlyTime = 10;
	// 体勢を立て直すフェーズの時間（フレーム）
	static inline const uint32_t kKnockbackRecoverTime = 15;
	// 弾き飛ばされる初速
	static inline const float kKnockbackInitialSpeed = 0.4f;

	// --- エフェクト用のデータ ---
	KamataEngine::Model*         modelAttack_ = nullptr;
	KamataEngine::WorldTransform worldTransformAttack_;

	// 加速度・最高速度・ジャンプ初速（機体ごとに変わる。デフォルトはバランサー相当）
	float acceleration_     = 0.04f;
	float limitRunSpeed_    = 0.22f;
	float jumpAcceleration_ = 0.9f;

	static inline const float kAttenuation          = 0.1f;
	static inline const float kAttenuationWall      = 0.3f;
	static inline const float kAttenuationLanding   = 0.1f;
	static inline const float kGravityAcceleration  = 0.12f;
	static inline const float kMaxFallSpeed         = 0.5f;
	static inline const float kTimeTurn             = 0.3f;

	// 穴（マップの底）に落下したとみなすY座標のしきい値
	// （床のY座標は0が最低なので、そこから十分下まで落ちたら死亡扱いにする）
	static inline const float kFallDeathY = -6.0f;

	float turnFirstRotationY_ = 0.0f;
	float turnTimer_          = 0.0f;
	bool  onGround_           = true;

	// 2段ジャンプがまだ使えるかどうか
	// （地面に着地するたびtrueに戻り、空中で一度使うとfalseになる）
	bool canDoubleJump_ = true;
	//デスフラグ
	bool isDead_ = false;

	// --- 被弾関連（体力が残っている状態でダメージを受けた場合の無敵・点滅） ---
	// このフレームで被弾したことを表すフラグ（GameScene側がConsumeJustTookDamage()で読み取る）
	bool justTookDamage_ = false;
	// 被弾後の無敵時間の残り[秒]（0より大きい間は無敵＆点滅する）
	float hitInvincibleTimer_ = 0.0f;
	// 被弾後の無敵時間の長さ[秒]
	static inline const float kHitInvincibleDuration = 0.3f;
	// 点滅の切り替え間隔[秒]
	static inline const float kBlinkInterval = 0.06f;

	// 被弾後の無敵時間中、今のフレームでモデルを表示すべきかどうか（点滅用）
	bool IsBlinkVisible() const;

};
