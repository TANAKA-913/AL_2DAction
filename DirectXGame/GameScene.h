#pragma once
#include "Enemy.h"
#include "ShieldEnemy.h"
#include "HitEffect.h"
#include "GuardEffect.h"
#include "DeathParticles.h"
#include "MapChipField.h"
#include "Player.h"
#include "Skydome.h"
#include "Signboard.h"
#include "Projectile.h"
#include "kamataEngine.h"
#include "CameraController.h"
#include "Fade.h"
#include <vector>
#include <list>

// ステージマネージャクラスの前方宣言（軽い依存関係にするため）
class StageManager;

class GameScene {
public:
	// ゲームのフェーズ（型）
	enum class Phase {
		kFadeIn,  // フェードイン
		kPlay,    // ゲームプレイ
		kPause,   // 一時停止
		kDeath,   // デス演出
		kFadeOut, // フェードアウト
	};

	~GameScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(StageManager* stageDataManager);
	void Update();
	void Draw();

	// フィールドオブジェクト（ブロック・自キャラなど）の生成
	void GenerateFieldObjects();

	// 全ての当たり判定を行う
	void CheckAllCollisions();

	// 遠距離攻撃の弾を更新し、敵との当たり判定を行う
	void UpdateProjectiles();
	// 遠距離攻撃の弾を生成する
	void SpawnProjectile();

	// ヒットエフェクトを生成
	void CreateHitEffect(const KamataEngine::Vector3& position);

	// ガードエフェクトを生成
	void CreateGuardEffect(const KamataEngine::Vector3& position);

	// 終了フラグのgetter
	bool IsFinished() const { return finished_; }

	// ゴール到達フラグのgetter
	// （trueの場合、main側でゴールシーンへ遷移させる）
	bool IsGoal() const { return isGoal_; }

	// ゲームオーバー（死亡）フラグのgetter
	// （trueの場合、main側でゲームオーバーシーンへ遷移させる）
	bool IsGameOver() const { return isGameOver_; }

	// リトライフラグのgetter
	// （trueの場合、体力が残っているので、main側で同じステージを再生成させる）
	bool IsRetry() const { return isRetry_; }

	// タイトルへ戻るフラグのgetter
	// （trueの場合、ポーズメニューから「タイトルへ戻る」が選ばれたので、
	//   main側でタイトルシーンへ遷移させる）
	bool IsReturnToTitle() const { return isReturnToTitle_; }

private:
	// ゲームプレイフェーズの更新
	void PlayUpdate();
	// 一時停止フェーズの更新
	void PauseUpdate();
	// 一時停止メニューの描画
	void DrawPauseMenu();
	// デス演出フェーズの更新
	void DeathUpdate();
	// フェードインフェーズの更新
	void FadeInUpdate();
	// フェードアウトフェーズの更新
	void FadeOutUpdate();
	// ブロックの更新（両フェーズ共通）
	void UpdateBlocks();
	// カメラの更新（両フェーズ共通。デバッグカメラ/通常カメラの切り替えを含む）
	void UpdateCameraMatrix();
	// 体力HUDアイコンの描画（画面右上）
	void DrawLifeIcons();
	// ダッシュ残数HUDアイコンの描画（画面左上）
	void DrawDashIcons();
	// 看板の近くにいる場合、対応するヒントを画面下部に表示する
	void UpdateSignHint();
	// ヒント画像の描画（画面下部）
	void DrawSignHint();

	// ゲームの現在フェーズ（変数）
	Phase phase_ = Phase::kFadeIn;

	// 終了フラグ
	bool finished_ = false;

	// ゴール到達フラグ
	bool isGoal_ = false;

	// ゲームオーバー（死亡）フラグ
	bool isGameOver_ = false;

	// リトライフラグ（体力が残っている状態で死亡した場合にtrueになる）
	bool isRetry_ = false;

	// タイトルへ戻るフラグ（ポーズメニューから選ばれた場合にtrueになる）
	bool isReturnToTitle_ = false;

	// --- フェード ---
	Fade* fade_ = nullptr;

	// --- プレイヤー関連 ---
	Player* player_ = nullptr;
	uint32_t playerTextureHandle_ = 0;
	KamataEngine::Model* playermodel_ = nullptr;
	// 攻撃エフェクト用モデル（"attackEffect" という名前でOBJを用意してください）
	KamataEngine::Model* modelAttackEffect_ = nullptr;
	// 背後トレイル（噴射炎）用モデル（"jetTrail"。Enemy/ShieldEnemy/Playerダッシュで共通使用）
	KamataEngine::Model* modelJetTrail_ = nullptr;

	// --- エネミー関連 ---
	std::list<Enemy*> enemies_;

	std::list<ShieldEnemy*> shieldEnemies_;

	// --- ヒットエフェクト関連 ---
	std::list<HitEffect*> hitEffects_;
	// ヒットエフェクト用モデル（"hitEffect" という名前でOBJを用意してください）
	KamataEngine::Model* modelHitEffect_ = nullptr;

	// --- ガードエフェクト関連 ---
	std::list<GuardEffect*> guardEffects_;
	// ガードエフェクト用モデル（"guardEffect" という名前でOBJを用意してください）
	KamataEngine::Model* modelGuardEffect_ = nullptr;

	uint32_t spriteTextureHandle_ = 0;
	KamataEngine::Model* model_ = nullptr;

	// --- 機体（キャラクター）ごとのプレイヤーモデル ---
	// "player/heavyBot"・"player/speedBot"・"player/balancer" を読み込み、
	// 選択中の機体に応じてどれを使うか決める。
	KamataEngine::Model* modelPlayerHeavy_ = nullptr;
	KamataEngine::Model* modelPlayerSpeed_ = nullptr;
	KamataEngine::Model* modelPlayerBalance_ = nullptr;

	// --- ブロック関連 ---
	uint32_t blockTextureHandle_ = 0;
	KamataEngine::Model* blockmodel_ = nullptr;

	std::vector<std::vector<KamataEngine::WorldTransform*>> worldTransformBlocks_;

	// --- 天球関連 ---
	Skydome* skydome_ = nullptr;
	KamataEngine::Model* modelSkydome_ = nullptr;

	// --- 背景（ビル）関連 ---
	// "bill" モデル（Resources/bill フォルダ）をステージ背景に並べて配置する
	KamataEngine::Model* modelBuilding_ = nullptr;
	std::vector<KamataEngine::WorldTransform*> worldTransformBuildings_;


	KamataEngine::Model* modelEnemy_ = nullptr;

	KamataEngine::Model* modelShieldEnemy_ = nullptr;

	// --- デスパーティクル関連 ---
	DeathParticles* deathParticles_ = nullptr;
	KamataEngine::Model* modelDeathParticles_ = nullptr;

	// --- マップ関連 ---
	MapChipField* mapChipField_ = nullptr;

	// --- ステージ管理関連 ---
	// ステージマネージャ（他クラスが所有しているものを参照するだけなので、GameScene側では生成・破棄しない）
	StageManager* stageManager_ = nullptr;

	// --- 体力HUD関連 ---
	// 体力アイコン（画面右上。選択機体の最大体力ぶん生成しておき、現在の体力の分だけ描画する）
	std::vector<KamataEngine::Sprite*> spriteLifeIcons_;

	// --- ダッシュ残数HUD関連 ---
	// ダッシュ残数アイコン（画面左上。選択機体の最大ダッシュ回数ぶん生成しておき、現在の残数の分だけ描画する）
	std::vector<KamataEngine::Sprite*> spriteDashIcons_;

	// --- 看板（チュートリアルヒント）関連 ---
	// マップ上に配置された看板（借り物ではなく、GameScene側で生成・破棄する）
	std::vector<Signboard*> signs_;
	// 看板に近づいた時に画面下部へ表示するヒント画像（5種類固定で読み込んでおく）
	std::vector<KamataEngine::Sprite*> spriteHints_;
	// 現在表示すべきヒントのインデックス（-1: 表示なし）
	int32_t activeHintIndex_ = -1;
	// 看板に反応する距離（この範囲内に入るとヒントが表示される）
	static inline const float kSignTriggerRange = 2.0f;

	// --- 遠距離攻撃（弾）関連 ---
	// 発生中の弾（ヘヴィボット等、遠距離攻撃機体専用）
	std::list<Projectile*> projectiles_;

	// --- 一時停止メニュー関連 ---
	// 背景を暗くする半透明オーバーレイ
	KamataEngine::Sprite* spritePauseOverlay_ = nullptr;
	// 「一時停止」タイトル画像
	KamataEngine::Sprite* spritePauseTitle_ = nullptr;
	// 選択肢画像（0:つづける 1:タイトルへ戻る）
	KamataEngine::Sprite* spritePauseOptions_[2] = {nullptr, nullptr};
	// 選択中カーソル用の四角スプライト
	KamataEngine::Sprite* spritePauseCursor_ = nullptr;
	// 現在選択中の項目（0:つづける 1:タイトルへ戻る）
	int32_t pauseSelectedIndex_ = 0;
	// 一時停止に入る直前のフェーズ（解除時に元へ戻すため）
	Phase phaseBeforePause_ = Phase::kPlay;

	// --- ゴール関連 ---
	// ゴールがCSVに配置されているかどうか
	bool hasGoal_ = false;
	// ゴールのワールド座標（マップチップ上の'G'のマスの座標）
	KamataEngine::Vector3 goalPosition_ = {};
	// ゴール表示用モデル（"Goal" フォルダに置いたモデルを読み込む）
	KamataEngine::Model* modelGoal_ = nullptr;
	// ゴール表示用ワールドトランスフォーム
	KamataEngine::WorldTransform worldTransformGoal_;

	// --- カメラ・共通システム ---
	KamataEngine::Camera camera_;
	KamataEngine::DebugCamera* debugCamera_ = nullptr;

	CameraController* cameraController_ = nullptr;

	bool isDebugCameraActive_ = false;



	// その他パラメータ
	float inputFloat3[3] = {0.0f, 0.0f, 0.0f};
};