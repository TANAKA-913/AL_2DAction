#pragma once
#include "kamataEngine.h"
#include "AABB.h"
#include <array>

// 前方宣言（循環参照回避）
class Player;
class GameScene;
class ShieldEnemy;
class MapChipField;

class Enemy {

public:
	void Initialize(KamataEngine::Model* model, KamataEngine::Camera* camera, const KamataEngine::Vector3& position);
	void Update();
	void Draw();

	// ワールド座標を取得
	KamataEngine::Vector3 GetWorldPosition();

	// AABBを取得
	AABB GetAABB();

	// 衝突応答
	void OnCollision(const Player* player);

	// 攻撃状態に関わらず、強制的に撃破する（遠距離攻撃の弾が着弾した時などに使う）
	void Defeat();

	// ゲームシーンのポインタをセット（前方宣言 + setterでの相互依存回避）
	void SetGameScene(GameScene* gameScene) { gameScene_ = gameScene; }

	// マップチップフィールドのポインタをセット（壁との当たり判定に使う）
	void SetMapChipField(MapChipField* mapChipField) { mapChipField_ = mapChipField; }

	// 背後パーティクル用モデルをセット（"jetTrail" 専用モデル。Player/ShieldEnemyと共通）
	void SetTrailModel(KamataEngine::Model* model) { modelTrail_ = model; }

	// デスフラグのゲッター
	bool IsDead() const { return isDead_; }

	// コリジョン無効フラグのゲッター
	bool IsCollisionDisabled() const { return isCollisionDisabled_; }

private:
	// --- ふるまい ---
	enum class Behavior {
		kWalk,    // 歩行
		kDeath,   // デス演出
		kUnknown, // リクエストなし
	};

	// 各ふるまいの初期化・更新
	void BehaviorWalkInitialize();
	void BehaviorWalkUpdate();
	void BehaviorDeathInitialize();
	void BehaviorDeathUpdate();

	// 現在のふるまい
	Behavior behavior_ = Behavior::kWalk;
	// 次のふるまいリクエスト
	Behavior behaviorRequest_ = Behavior::kUnknown;

	// --- 当たり判定サイズ ---
	static inline const float kWidth  = 0.8f;
	static inline const float kHeight = 0.8f;

	// ワールドトランスフォーム（所有）
	KamataEngine::WorldTransform worldTransform_;
	// モデルのポインタ（借りてくる用）
	KamataEngine::Model* model_ = nullptr;
	// カメラのポインタ（借りてくる用）
	KamataEngine::Camera* camera_ = nullptr;

	// 歩行の速さ
	static inline const float kWalkSpeed = 0.05f;

	// 速度
	KamataEngine::Vector3 velocity_ = {};

	// 経過時間（歩行アニメーションが無くなった今は主にパーティクル用のタイマーとして使用）
	float walkTimer_ = 0.0f;

	// --- 背後パーティクル（トレイル） ---
	// 1個のパーティクル情報
	struct TrailParticle {
		KamataEngine::WorldTransform worldTransform;
		KamataEngine::ObjectColor    objectColor;
		KamataEngine::Vector4        color  = {1.0f, 1.0f, 1.0f, 1.0f};
		float                        timer  = 0.0f;
		bool                         active = false;
	};

	// 背後パーティクル用モデル（借りてくる用。"jetTrail" 専用モデル）
	KamataEngine::Model* modelTrail_ = nullptr;

	// パーティクルの最大同時発生数
	static inline const uint32_t kNumTrailParticles = 6;
	// パーティクルを発生させる間隔[秒]
	static inline const float kTrailSpawnInterval = 0.08f;
	// パーティクル1個あたりの寿命[秒]
	static inline const float kTrailLifeTime = 0.4f;
	// パーティクルの大きさ（アタックエフェクトと同程度＝等倍）
	static inline const float kTrailScale = 1.0f;
	// 背後へのオフセット距離
	static inline const float kTrailOffset = 0.3f;

	// パーティクルの配列
	std::array<TrailParticle, kNumTrailParticles> trailParticles_;
	// 次のパーティクルを発生させるまでのタイマー
	float trailSpawnTimer_ = 0.0f;

	// 背後パーティクルを1つ発生させる
	void SpawnTrailParticle();
	// 背後パーティクル全体を更新する
	void UpdateTrailParticles();
	// 背後パーティクルを描画する
	void DrawTrailParticles();

	// --- デス演出 ---
	// デス演出の所要時間[秒]
	static inline const float kDeathTime = 1.0f;
	// デス演出の経過時間
	float deathTimer_ = 0.0f;
	// Y軸まわりの回転数（ぐるぐる回転させる）
	static inline const float kDeathSpinTurns = 3.0f;
	// X軸まわりの回転角（ひっくり返る）[度]
	static inline const float kDeathFlipAngle = 180.0f;

	// デスフラグ
	bool isDead_ = false;

	// コリジョン無効フラグ（デス演出中は衝突判定をスキップする）
	bool isCollisionDisabled_ = false;

	// ゲームシーンのポインタ（借りてくる用。エフェクト生成に使う）
	GameScene* gameScene_ = nullptr;

	// マップチップフィールドのポインタ（借りてくる用。壁との当たり判定に使う）
	MapChipField* mapChipField_ = nullptr;

	// 壁判定用の先読みマージン（進行方向の当たり判定を少し先まで見る）
	static inline const float kWallCheckMargin = 0.05f;

	// 反転直後、再び反転できるようになるまでのクールダウン時間[秒]
	// （わずかな座標のズレなどで毎フレーム反転を繰り返してしまうのを防ぐための安全策）
	static inline const float kTurnCooldown = 0.2f;
	// 反転クールダウンの残り時間
	float turnCooldownTimer_ = 0.0f;
};
