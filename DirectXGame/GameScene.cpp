#include "GameScene.h"
#include "KamataEngine.h"
#include "Math.h"
#include "AABB.h"
#include "StageManager.h"
#include "SoundManager.h"
#include <cassert>
#include <cmath>

using namespace KamataEngine;

namespace {
// フェードにかける時間（秒）
constexpr float kFadeDuration = 1.0f;
} // namespace

void GameScene::Initialize(StageManager* stageDataManager) {

	// ゲームBGMを再生（既に再生中の場合は何もしないので、ステージ移行時も途切れない）
	SoundManager::GetInstance()->PlayBGMGame();

	// ステージマネージャを保持しておく
	stageManager_ = stageDataManager;

	// フェードイン フェーズから開始
	phase_ = Phase::kFadeIn;

	// 1. テクスチャ読み込み

	// 2. モデル生成
	// 機体（キャラクター）ごとのプレイヤーモデル読み込み
	// （Resources/heavyBot, /speedBot, /balancer フォルダに置いたモデル。
	//  他のモデルと同じく、Resources直下の1階層＝フォルダ名とファイル名が一致した構成にすること）
	modelPlayerHeavy_ = Model::CreateFromOBJ("heavyBot", true);
	modelPlayerSpeed_ = Model::CreateFromOBJ("speedBot", true);
	modelPlayerBalance_ = Model::CreateFromOBJ("balancer", true);
	blockmodel_ = Model::CreateFromOBJ("block", true);
	modelSkydome_ = Model::CreateFromOBJ("skydome", true);
	// 背景ビル用モデルの読み込み（Resources/bill フォルダに置いたモデル）
	modelBuilding_ = Model::CreateFromOBJ("bill", true);
	modelEnemy_ = Model::CreateFromOBJ("enemy", true);
	modelShieldEnemy_ = Model::CreateFromOBJ("shieldEnemy", true);
	modelDeathParticles_ = Model::CreateFromOBJ("deathParticle", true);
	modelAttackEffect_ = Model::CreateFromOBJ("attackEffect", true);
	// 背後トレイル（噴射炎）用モデルの読み込み（Resources/jetTrail フォルダに置いたモデル）
	modelJetTrail_ = Model::CreateFromOBJ("jetTrail", true);
	modelHitEffect_ = Model::CreateFromOBJ("hitEffect", true);
	// ヒットエフェクト用モデルの読み込み
	HitEffect::SetModel(modelHitEffect_);
	HitEffect::SetCamera(&camera_);

	modelGuardEffect_ = Model::CreateFromOBJ("guardEffect", true);
	// ガードエフェクト用モデルの読み込み
	GuardEffect::SetModel(modelGuardEffect_);
	GuardEffect::SetCamera(&camera_);

	// ゴール表示用モデルの読み込み（Resources/Goal フォルダに置いたモデル）
	modelGoal_ = Model::CreateFromOBJ("Goal", true);
	// 3. マップ生成
	mapChipField_ = new MapChipField();
	// 現在のステージデータを取得する
	const StageData& stageData = stageManager_->GetCurrentStageData();
	// ステージファイルパスの生成
	std::string stageFileName = "Resources/stages/" + stageData.name + ".csv";
	// ステージファイルの読み込み
	mapChipField_->LoadMapChipCsv(stageFileName);

	// 4. カメラ初期化
	camera_.Initialize();
	camera_.farZ = 1000.0f;

	PrimitiveDrawer::GetInstance()->SetCamera(&camera_);

	debugCamera_ = new DebugCamera(1280, 720);

	cameraController_ = new CameraController();
	cameraController_->Initialize();

	// --- 5. フィールドオブジェクト生成（ブロック・自キャラ・敵） ---
	// 自キャラ・敵はCSVのマップチップ配置から生成されるので、
	// カメラコントローラーへのターゲット設定より前に呼び出す必要がある
	GenerateFieldObjects();

	// ブロックの初期化直後の1フレーム目から正しい位置で表示されるよう、
	// ここで一度ブロックの行列を計算しておく
	// （UpdateBlocks()は本来PlayUpdate()内で毎フレーム呼ばれるが、
	//  それだとフェードイン中（Update()が呼ばれる前）は
	//  初期化直後のデフォルト行列＝座標(0,0,0)のまま描画されてしまう）
	UpdateBlocks();

	// CSVにプレイヤー配置（'P'のマス）が無いと自キャラが生成されず、
	// 後々Draw()などでnullptrアクセスして分かりにくいクラッシュになるので、
	// ここで早期に検出する。
	assert(player_ != nullptr && "CSVにプレイヤー配置（'P'のマス）が見つかりません");

	// プレイヤー生成後にカメラコントローラーの設定を行う
	cameraController_->SetTarget(player_);

	// マップの大きさに合わせてカメラの移動範囲を設定する
	// ※ 以前は固定値(100x100)でハードコードしていたが、マップサイズを変更した際に
	//   カメラがステージの途中までしか追従できなくなる事故が起きたため、
	//   マップチップフィールドの実際のサイズから動的に計算するよう変更した。
	float mapAreaWidth  = MapChipField::kMapWidth * static_cast<float>(mapChipField_->GetNumBlockHorizontal());
	float mapAreaHeight = MapChipField::kMapHeight * static_cast<float>(mapChipField_->GetNumBlockVertical());
	CameraController::Rect cameraArea = {0.0f, mapAreaWidth, 0.0f, mapAreaHeight};
	cameraController_->SetMovableArea(cameraArea);

	// 開幕の位置をプレイヤーに合わせる
	cameraController_->Reset();

	// カメラコントローラーが計算した行列を、この時点で一度メインカメラに反映しておく。
	// （これをしないと、フェードイン中はカメラが初期化直後のデフォルト位置＝
	//  プレイヤーから遠く離れた場所のままになってしまい、
	//  フェードが終わってプレイフェーズに入った瞬間に一気に正しい位置へ
	//  ワープしたように見えてしまう）
	UpdateCameraMatrix();

	// 6. 天球生成
	// マップの水平中心に合わせて配置する。
	// （原点(0,0,0)に固定していると、マップ全体の中心とズレてしまい、
	//  ステージ終盤でプレイヤーが天球の縁に近づきすぎて見えてしまうため）
	skydome_ = new Skydome();
	Vector3 skydomePosition = {mapAreaWidth / 2.0f, 0.0f, 0.0f};
	skydome_->Initialize(modelSkydome_, skydomePosition);

	// 6.5. 背景ビルの配置
	// ステージ幅全体に、ゲームプレイより奥（Z奥側）に並べて背景として配置する。
	// 高さ・奥行きに変化をつけて、単調にならないようにしている。
	{
		constexpr float kBuildingSpacing  = 9.0f;  // ビルを並べる間隔
		constexpr float kBuildingBaseZ    = 15.0f; // ゲームプレイより奥に配置する距離
		constexpr float kBuildingBaseY    = 0.0f;  // 地面と同じ高さを基準にする

		// 既存があれば解放してから作り直す（ステージ移行・リトライ時にGameSceneが作り直されるため）
		for (WorldTransform* wt : worldTransformBuildings_) {
			delete wt;
		}
		worldTransformBuildings_.clear();

		int32_t numBuildings = static_cast<int32_t>(mapAreaWidth / kBuildingSpacing) + 2;
		for (int32_t i = 0; i < numBuildings; ++i) {
			WorldTransform* wt = new WorldTransform();
			wt->Initialize();

			// 高さ・奥行き・横幅に、インデックスごとの規則的な変化をつけて、
			// 単調に同じビルが並ばないようにする
			float heightVariation = 3.0f + std::fmod(static_cast<float>(i) * 2.7f, 5.0f);
			float depthVariation  = std::fmod(static_cast<float>(i) * 1.9f, 6.0f);
			float widthVariation  = 1.5f + std::fmod(static_cast<float>(i) * 1.3f, 1.5f);

			wt->scale_ = {widthVariation, heightVariation, widthVariation};
			wt->translation_ = {
			    static_cast<float>(i) * kBuildingSpacing - kBuildingSpacing, kBuildingBaseY, kBuildingBaseZ + depthVariation};

			// 初期化直後の1フレーム目から正しい位置・大きさで表示されるよう、ここで一度計算しておく
			wt->matWorld_ = Math::MakeAffineMatrix(wt->scale_, wt->rotation_, wt->translation_);
			wt->TransferMatrix();

			worldTransformBuildings_.push_back(wt);
		}
	}

	// 8. フェードの生成と初期化
	fade_ = new Fade();
	fade_->Initialize();
	fade_->Start(Fade::Status::FadeIn, kFadeDuration);

	// 9. 体力HUDアイコンの生成（画面右上に、選択機体の最大体力ぶん用意しておく）
	// ※ 実際に描画するのは現在の体力の分だけ（DrawLifeIcons()参照）
	{
		constexpr float kIconSize    = 24.0f;
		constexpr float kIconSpacing = 32.0f;
		constexpr float kMarginRight = 20.0f;
		constexpr float kMarginTop   = 20.0f;
		constexpr float kScreenWidth = 1280.0f;

		// 解放してから作り直す（ステージ移行・リトライ時にGameSceneが作り直されるため）
		for (Sprite* icon : spriteLifeIcons_) {
			delete icon;
		}
		spriteLifeIcons_.clear();

		int32_t maxHP = stageManager_ ? stageManager_->GetMaxHP() : 3;

		for (int32_t i = 0; i < maxHP; ++i) {
			// 右詰めで配置：i=0が右上コーナーに一番近い位置
			float x = kScreenWidth - kMarginRight - kIconSize - static_cast<float>(i) * kIconSpacing;
			float y = kMarginTop;

			Sprite* icon = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {x, y});
			icon->SetSize(Vector2(kIconSize, kIconSize));
			// 機械系テーマに合わせた水色
			icon->SetColor(Vector4(0.6f, 0.85f, 1.0f, 1.0f));
			spriteLifeIcons_.push_back(icon);
		}
	}

	// 10. ダッシュ残数HUDアイコンの生成（画面左上に、選択機体の最大ダッシュ回数ぶん用意しておく）
	// ※ 実際に描画するのは現在の残数の分だけ（DrawDashIcons()参照）
	{
		constexpr float kIconSize    = 24.0f;
		constexpr float kIconSpacing = 32.0f;
		constexpr float kMarginLeft  = 20.0f;
		constexpr float kMarginTop   = 20.0f;

		for (Sprite* icon : spriteDashIcons_) {
			delete icon;
		}
		spriteDashIcons_.clear();

		int32_t maxDashCharges = player_ ? player_->GetMaxDashCharges() : 2;

		for (int32_t i = 0; i < maxDashCharges; ++i) {
			// 左詰めで配置：i=0が左上コーナーに一番近い位置
			float x = kMarginLeft + static_cast<float>(i) * kIconSpacing;
			float y = kMarginTop;

			Sprite* icon = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {x, y});
			icon->SetSize(Vector2(kIconSize, kIconSize));
			// 体力アイコン（水色）と見分けがつくよう、ダッシュは黄色にしておく
			icon->SetColor(Vector4(1.0f, 0.9f, 0.3f, 1.0f));
			spriteDashIcons_.push_back(icon);
		}
	}

	// 11. チュートリアルヒント画像の読み込み（画面下部中央。看板に近づいた時だけ表示する）
	{
		constexpr float kHintWidth  = 560.0f;
		constexpr float kScreenWidth = 1280.0f;
		constexpr float kHintY      = 560.0f;
		float hintX = (kScreenWidth - kHintWidth) / 2.0f;

		for (Sprite* hint : spriteHints_) {
			delete hint;
		}
		spriteHints_.clear();

		const char* hintFiles[] = {
		    "hints/hint_move.png",   // 0: 移動
		    "hints/hint_jump.png",   // 1: ジャンプ
		    "hints/hint_dash.png",   // 2: ダッシュ
		    "hints/hint_attack.png", // 3: 攻撃
		    "hints/hint_goal.png",   // 4: ゴール
		    "hints/hint_life.png",   // 5: 体力
		};

		for (const char* file : hintFiles) {
			Sprite* hint = Sprite::Create(TextureManager::GetInstance()->Load(file), {hintX, kHintY});
			spriteHints_.push_back(hint);
		}

		activeHintIndex_ = -1;
	}

	// 12. 一時停止メニュー用スプライトの生成
	{
		constexpr float kScreenWidth  = 1280.0f;
		constexpr float kScreenHeight = 720.0f;

		// 画面全体を暗くするオーバーレイ
		delete spritePauseOverlay_;
		spritePauseOverlay_ = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {0.0f, 0.0f});
		spritePauseOverlay_->SetSize(Vector2(kScreenWidth, kScreenHeight));
		spritePauseOverlay_->SetColor(Vector4(0.0f, 0.0f, 0.0f, 0.6f));

		// 「一時停止」タイトル
		delete spritePauseTitle_;
		spritePauseTitle_ = Sprite::Create(TextureManager::GetInstance()->Load("pause/pause_title.png"), {(kScreenWidth - 400.0f) / 2.0f, 180.0f});

		// 選択肢（つづける／タイトルへ戻る）
		delete spritePauseOptions_[0];
		spritePauseOptions_[0] =
		    Sprite::Create(TextureManager::GetInstance()->Load("pause/pause_resume.png"), {(kScreenWidth - 320.0f) / 2.0f, 340.0f});
		delete spritePauseOptions_[1];
		spritePauseOptions_[1] =
		    Sprite::Create(TextureManager::GetInstance()->Load("pause/pause_title_return.png"), {(kScreenWidth - 320.0f) / 2.0f, 420.0f});

		// 選択中カーソル用の四角（選択肢の左に表示する）
		delete spritePauseCursor_;
		spritePauseCursor_ = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {(kScreenWidth - 320.0f) / 2.0f - 30.0f, 340.0f + 16.0f});
		spritePauseCursor_->SetSize(Vector2(16.0f, 16.0f));
		spritePauseCursor_->SetColor(Vector4(1.0f, 0.9f, 0.3f, 1.0f));

		pauseSelectedIndex_ = 0;
	}
}

void GameScene::GenerateFieldObjects() {
	// --- 要素数の設定 ---
	uint32_t numBlockVertical = mapChipField_->GetNumBlockVertical();
	uint32_t numBlockHorizontal = mapChipField_->GetNumBlockHorizontal();

	worldTransformBlocks_.resize(numBlockVertical);
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		worldTransformBlocks_[i].resize(numBlockHorizontal);
	}

	// --- フィールドオブジェクトの生成 ---
	for (uint32_t i = 0; i < numBlockVertical; ++i) {
		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {

			MapChipType mapChipType = mapChipField_->GetMapChipTypeByIndex(j, i);

			switch (mapChipType) {

			case MapChipType::kBlock: {
				WorldTransform* worldTransform = new WorldTransform();
				worldTransform->Initialize();
				worldTransformBlocks_[i][j] = worldTransform;

				// 座標を設定
				worldTransformBlocks_[i][j]->translation_ = mapChipField_->GetMapChipPositionByIndex(j, i);
				break;
			}

			case MapChipType::kPlayer: {
				assert(player_ == nullptr && "自キャラを二重に配置しようとしています");

				// 自キャラの生成
				player_ = new Player();

				// 選択中の機体に応じて、使用するモデルを切り替える
				// （キャラクター選択シーンでまだ何も選ばれていない場合はバランサーを使う）
				CharacterType selectedCharacter = stageManager_ ? stageManager_->GetSelectedCharacter() : CharacterType::kBalance;
				switch (selectedCharacter) {
				case CharacterType::kHeavy:
					model_ = modelPlayerHeavy_;
					break;
				case CharacterType::kSpeed:
					model_ = modelPlayerSpeed_;
					break;
				case CharacterType::kBalance:
				default:
					model_ = modelPlayerBalance_;
					break;
				}

				// 座標を指定してキャラの初期化
				KamataEngine::Vector3 playerPosition = mapChipField_->GetMapChipPositionByIndex(j, i);
				player_->Initialize(model_, spriteTextureHandle_, modelAttackEffect_, &camera_, playerPosition);

				// 自キャラにマップチップ情報をセット
				player_->SetMapChipField(mapChipField_);

				// ダッシュ中の背後パーティクル用モデルをセット（"jetTrail" 専用モデル。Enemy/ShieldEnemyと共通）
				player_->SetDashTrailModel(modelJetTrail_);

				// キャラクター選択シーンで選ばれた機体の性能を反映する
				if (stageManager_) {
					player_->SetCharacterStats(stageManager_->GetSelectedCharacterStats());
				}
				break;
			}

			case MapChipType::kEnemy: {
				// サブIDで敵の種類を判別する
				// 0: 通常の敵(Enemy)、1: シールド敵(ShieldEnemy)
				uint8_t enemySubID = mapChipField_->GetMapChipSubIDByIndex(j, i);
				KamataEngine::Vector3 enemyPosition = mapChipField_->GetMapChipPositionByIndex(j, i);

				switch (enemySubID) {
				case 0: {
					Enemy* newEnemy = new Enemy();
					newEnemy->Initialize(modelEnemy_, &camera_, enemyPosition);
					newEnemy->SetGameScene(this);
					// 壁との当たり判定（反射）に使うため、マップチップ情報をセット
					newEnemy->SetMapChipField(mapChipField_);
					// 背後パーティクル用モデルをセット（"jetTrail" 専用モデル。Player/ShieldEnemyと共通）
					newEnemy->SetTrailModel(modelJetTrail_);
					enemies_.push_back(newEnemy);
					break;
				}
				case 1: {
					ShieldEnemy* newShieldEnemy = new ShieldEnemy();
					newShieldEnemy->Initialize(modelShieldEnemy_, &camera_, enemyPosition);
					newShieldEnemy->SetGameScene(this);
					// 壁との当たり判定（反射）に使うため、マップチップ情報をセット
					newShieldEnemy->SetMapChipField(mapChipField_);
					// 背後パーティクル用モデルをセット（"jetTrail" 専用モデル。Player/Enemyと共通）
					newShieldEnemy->SetTrailModel(modelJetTrail_);
					shieldEnemies_.push_back(newShieldEnemy);
					break;
				}
				default:
					// 未対応のサブIDは無視する
					break;
				}
				break;
			}

			case MapChipType::kGoal: {
				// 座標を記憶しておく（CheckAllCollisions()でプレイヤーとの判定に使う）
				hasGoal_ = true;
				goalPosition_ = mapChipField_->GetMapChipPositionByIndex(j, i);

				// ゴール表示用のワールドトランスフォームを初期化する
				worldTransformGoal_.Initialize();
				worldTransformGoal_.translation_ = goalPosition_;
				// 初期化直後の1フレーム目から正しい位置で表示されるよう、ここで一度計算しておく
				worldTransformGoal_.matWorld_ = Math::MakeAffineMatrix(
				    worldTransformGoal_.scale_, worldTransformGoal_.rotation_, worldTransformGoal_.translation_);
				worldTransformGoal_.TransferMatrix();
				break;
			}

			case MapChipType::kSign: {
				// サブIDでヒントの種類を判別する
				// 0:移動 1:ジャンプ 2:ダッシュ 3:攻撃 4:ゴール
				uint8_t hintIndex = mapChipField_->GetMapChipSubIDByIndex(j, i);
				Vector3 signPosition = mapChipField_->GetMapChipPositionByIndex(j, i);

				Signboard* newSign = new Signboard();
				// 見た目は仮実装：ブロックモデルを縦長に変形させて流用
				newSign->Initialize(blockmodel_, signPosition, hintIndex);
				signs_.push_back(newSign);
				break;
			}

			default:
				break;
			}
		}
	}
}

void GameScene::Update() {

	// --- デバッグカメラ切り替え ---
#ifdef _DEBUG
	if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
		isDebugCameraActive_ = !isDebugCameraActive_;
	}
#endif

	// --- フェーズごとの更新処理 ---
	switch (phase_) {
	case Phase::kFadeIn:
		FadeInUpdate();
		break;
	case Phase::kPlay:
		PlayUpdate();
		break;
	case Phase::kPause:
		PauseUpdate();
		break;
	case Phase::kDeath:
		DeathUpdate();
		break;
	case Phase::kFadeOut:
		FadeOutUpdate();
		break;
	}
}

void GameScene::FadeInUpdate() {
	// フェードの更新
	fade_->Update();
	// フェードイン中にフェードが終わったらプレイフェーズに切り替える
	if (fade_->IsFinished()) {
		phase_ = Phase::kPlay;
	}

	// カメラの更新
	UpdateCameraMatrix();
}

void GameScene::PlayUpdate() {
	// ESCキーで一時停止する
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		phaseBeforePause_  = phase_;
		phase_             = Phase::kPause;
		pauseSelectedIndex_ = 0;
		SoundManager::GetInstance()->PlaySESelect();
		return;
	}

	// 天球の更新
	skydome_->Update();

	// 自キャラの更新
	player_->Update();

	// 敵の更新（複数）
	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// シールド敵の更新（複数）
	for (ShieldEnemy* shieldEnemy : shieldEnemies_) {
		shieldEnemy->Update();
	}

	// ヒットエフェクトの更新（複数）
	for (HitEffect* hitEffect : hitEffects_) {
		hitEffect->Update();
	}

	// ガードエフェクトの更新（複数）
	for (GuardEffect* guardEffect : guardEffects_) {
		guardEffect->Update();
	}

	// カメラコントローラの更新
	cameraController_->Update();

	// カメラの更新
	UpdateCameraMatrix();

	// ブロックの更新
	UpdateBlocks();

	// 看板の近くにいるか確認し、表示するヒントを更新する
	UpdateSignHint();

	// このフレームで遠距離攻撃を発射したか確認し、発射していたら弾を生成する
	if (player_->ConsumeJustFiredRangedAttack()) {
		SpawnProjectile();
	}
	// 弾の更新と、敵との当たり判定
	UpdateProjectiles();

	// 全ての当たり判定を行う（毎フレーム処理の最後）
	CheckAllCollisions();

	// このフレームで被弾したかどうかを確認する
	// （ダッシュ中・攻撃中・被弾後の無敵時間中はPlayer側で無効化されているのでtrueにならない）
	if (player_->ConsumeJustTookDamage()) {
		// 体力を1減らす
		int32_t remainingHP = stageManager_->DecrementHP();

		// ダメージSEを再生（体力が残っていても削れていても再生する）
		SoundManager::GetInstance()->PlaySEPlayerDamage();

		if (remainingHP <= 0) {
			// 体力が尽きたので、実際に死亡させる
			// （この後の IsDead() のブロックでデス演出に繋がる）
			player_->Kill();
		}
		// 体力がまだ残っている場合はここでは何もしない。
		// Player側で既に被弾後の無敵時間・点滅が始まっているので、そのままプレイを継続する。
	}

	// デスフラグの立った敵を削除
	// 死んだ敵の更新や描画を呼び出し続けるのは無駄なので、死んだら消える仕組みを実装する。
	enemies_.remove_if([](Enemy* enemy) {
		if (enemy->IsDead()) {
			delete enemy;
			return true;
		}
		return false;
	});

	// デスフラグの立ったシールド敵を削除
	shieldEnemies_.remove_if([](ShieldEnemy* shieldEnemy) {
		if (shieldEnemy->IsDead()) {
			delete shieldEnemy;
			return true;
		}
		return false;
	});

	// デスフラグの立ったヒットエフェクトを削除
	hitEffects_.remove_if([](HitEffect* hitEffect) {
		if (hitEffect->IsDead()) {
			delete hitEffect;
			return true;
		}
		return false;
	});

	// デスフラグの立ったガードエフェクトを削除
	guardEffects_.remove_if([](GuardEffect* guardEffect) {
		if (guardEffect->IsDead()) {
			delete guardEffect;
			return true;
		}
		return false;
	});

	// プレイヤーが死亡したらデス演出フェーズに移行する
	if (player_->IsDead()) {
		phase_ = Phase::kDeath;

		// 体力が残っている状態でここに来るのは、被弾以外の即死要因（穴に落ちた等）の場合のみ
		// （被弾による死亡は、この直前の被弾処理で既に体力が0まで減らされ、SEも再生済み）。
		// その場合はここで初めて体力を1減らし、SEも再生してから、残り数によって
		// 「同じステージをやり直す」か「ゲームオーバー」かを決める
		// （main側は IsRetry()/IsGameOver() を見て、フェード終了後の遷移先を判断する）
		int32_t remainingHP = stageManager_->GetHP();
		if (remainingHP > 0) {
			remainingHP = stageManager_->DecrementHP();
			SoundManager::GetInstance()->PlaySEPlayerDamage();
		}

		if (remainingHP > 0) {
			// まだ体力が残っているので、リトライフラグを立てる
			isRetry_ = true;
		} else {
			// 体力が尽きたのでゲームオーバーフラグを立てる
			isGameOver_ = true;
		}

		// デスパーティクルの生成
		deathParticles_ = new DeathParticles();

		// 機体（キャラクター）ごとに演出の色を変える
		// バランサー：緑 / スピードボット：青 / ヘヴィボット：赤
		Vector4 deathColor = {0.4f, 1.0f, 0.4f, 1.0f}; // デフォルト（緑・バランサー相当）
		if (stageManager_) {
			switch (stageManager_->GetSelectedCharacter()) {
			case CharacterType::kHeavy:
				deathColor = {1.0f, 0.3f, 0.3f, 1.0f}; // 赤
				break;
			case CharacterType::kSpeed:
				deathColor = {0.3f, 0.5f, 1.0f, 1.0f}; // 青
				break;
			case CharacterType::kBalance:
			default:
				deathColor = {0.4f, 1.0f, 0.4f, 1.0f}; // 緑
				break;
			}
		}
		deathParticles_->SetColor(deathColor);

		// 敵・プレイヤーのダッシュと共通のトレイルモデル（"jetTrail"）も一緒に使う
		deathParticles_->SetTrailModel(modelJetTrail_);

		deathParticles_->Initialize(modelDeathParticles_, &camera_, player_->GetWorldPosition());
	}
}

void GameScene::PauseUpdate() {
	// 上下キーで選択項目を切り替える
	if (Input::GetInstance()->TriggerKey(DIK_UP) || Input::GetInstance()->TriggerKey(DIK_DOWN)) {
		pauseSelectedIndex_ = 1 - pauseSelectedIndex_;
		SoundManager::GetInstance()->PlaySESelect();
	}

	// SPACEキーで決定する
	if (Input::GetInstance()->TriggerKey(DIK_SPACE)) {
		SoundManager::GetInstance()->PlaySESelect();

		if (pauseSelectedIndex_ == 0) {
			// つづける：一時停止に入る前のフェーズへ戻す
			phase_ = phaseBeforePause_;
		} else {
			// タイトルへ戻る：フェードアウトしてシーンを終了する
			isReturnToTitle_ = true;
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
		return;
	}

	// もう一度ESCキーで「つづける」と同じ扱いにする
	if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
		phase_ = phaseBeforePause_;
		SoundManager::GetInstance()->PlaySESelect();
	}
}

void GameScene::DeathUpdate() {
	// 天球の更新
	skydome_->Update();

	// 敵の更新（複数）
	for (Enemy* enemy : enemies_) {
		enemy->Update();
	}

	// シールド敵の更新（複数）
	for (ShieldEnemy* shieldEnemy : shieldEnemies_) {
		shieldEnemy->Update();
	}

	// デスパーティクルの更新
	if (deathParticles_) {
		deathParticles_->Update();
	}

	// デスパーティクルが有効で、なおかつパーティクルの演出が終了したら
	// フェードアウトを開始してフェードアウトフェーズに切り替える
	if (deathParticles_ && deathParticles_->IsFinished()) {
		fade_->Start(Fade::Status::FadeOut, kFadeDuration);
		phase_ = Phase::kFadeOut;
	}

	// カメラの更新
	// （自キャラは死んでいるのでカメラコントローラは更新しない。
	//   直前の行列を使い続けることで、カメラが勝手に動き回らないようにする）
	UpdateCameraMatrix();

	// ブロックの更新
	UpdateBlocks();
}

void GameScene::FadeOutUpdate() {
	// フェードの更新
	fade_->Update();
	// フェードアウト中にフェードが終わったらゲームシーンの終了フラグを立てる
	if (fade_->IsFinished()) {
		finished_ = true;
	}
}

void GameScene::UpdateCameraMatrix() {
	if (isDebugCameraActive_) {

		debugCamera_->Update();
		camera_.matView = debugCamera_->GetCamera().matView;
		camera_.matProjection = debugCamera_->GetCamera().matProjection;
		camera_.TransferMatrix();

	} else {

		//  コントローラーが計算した最新の行列を描画用メインカメラにコピーする
		camera_.matView = cameraController_->GetCamera().matView;
		camera_.matProjection = cameraController_->GetCamera().matProjection;

		//  コピーした行列をシェーダーへ転送
		camera_.TransferMatrix();
	}
}

void GameScene::DrawLifeIcons() {
	if (!stageManager_) {
		return;
	}

	// 現在の体力（表示するアイコンの個数）
	int32_t hp = stageManager_->GetHP();

	// KamataEngineでは、スプライトのDraw()は必ずSprite::PreDraw()とSprite::PostDraw()の間で
	// 呼び出す必要がある（DirectXの内部処理として、スプライト描画用のパイプラインに切り替えている）。
	Sprite::PreDraw();
	for (int32_t i = 0; i < hp && i < static_cast<int32_t>(spriteLifeIcons_.size()); ++i) {
		spriteLifeIcons_[i]->Draw();
	}
	Sprite::PostDraw();
}

void GameScene::DrawDashIcons() {
	if (!player_) {
		return;
	}

	// 現在のダッシュ残数（表示するアイコンの個数）
	int32_t dashCharges = player_->GetDashCharges();

	Sprite::PreDraw();
	for (int32_t i = 0; i < dashCharges && i < static_cast<int32_t>(spriteDashIcons_.size()); ++i) {
		spriteDashIcons_[i]->Draw();
	}
	Sprite::PostDraw();
}

void GameScene::UpdateSignHint() {
	if (!player_) {
		activeHintIndex_ = -1;
		return;
	}

	Vector3 playerPos = player_->GetWorldPosition();

	// 一番近い看板を探し、範囲内に入っていればそのヒントを表示対象にする
	float nearestDistance = kSignTriggerRange;
	int32_t nearestHintIndex = -1;

	for (Signboard* sign : signs_) {
		Vector3 signPos = sign->GetWorldPosition();
		float dx = playerPos.x - signPos.x;
		float dy = playerPos.y - signPos.y;
		float distance = std::sqrt(dx * dx + dy * dy);

		if (distance < nearestDistance) {
			nearestDistance  = distance;
			nearestHintIndex = sign->GetHintIndex();
		}
	}

	activeHintIndex_ = nearestHintIndex;
}

void GameScene::DrawSignHint() {
	if (activeHintIndex_ < 0 || activeHintIndex_ >= static_cast<int32_t>(spriteHints_.size())) {
		return;
	}

	Sprite::PreDraw();
	spriteHints_[activeHintIndex_]->Draw();
	Sprite::PostDraw();
}

void GameScene::SpawnProjectile() {
	if (!player_) {
		return;
	}

	Projectile* projectile = new Projectile();
	// 見た目は仮実装：既存のattackEffectモデルを流用
	projectile->Initialize(modelAttackEffect_, player_->GetWorldPosition(), player_->IsFacingRight(), player_->GetAttackRange());
	projectiles_.push_back(projectile);
}

void GameScene::UpdateProjectiles() {
	for (Projectile* projectile : projectiles_) {
		projectile->Update();

		if (projectile->IsDead()) {
			continue;
		}

		AABB projectileAabb = projectile->GetAABB();

		// 通常敵との当たり判定
		for (Enemy* enemy : enemies_) {
			if (enemy->IsCollisionDisabled()) {
				continue;
			}
			if (IsCollision(projectileAabb, enemy->GetAABB())) {
				// 弾が当たったら、プレイヤーの現在の攻撃状態に関わらず確実に撃破する
				// （弾は着弾までに時間がかかり、攻撃アニメーション終了後に当たることもあるため）
				enemy->Defeat();
				projectile->Kill();
				break;
			}
		}

		if (projectile->IsDead()) {
			continue;
		}

		// シールド敵との当たり判定
		for (ShieldEnemy* shieldEnemy : shieldEnemies_) {
			if (shieldEnemy->IsCollisionDisabled()) {
				continue;
			}
			if (IsCollision(projectileAabb, shieldEnemy->GetAABB())) {
				// 近接攻撃と同じくガード判定を行う（正面ならガード、背後なら撃破）
				shieldEnemy->OnProjectileHit(player_);
				projectile->Kill();
				break;
			}
		}
	}

	// 消滅した弾を削除する
	projectiles_.remove_if([](Projectile* projectile) {
		if (projectile->IsDead()) {
			delete projectile;
			return true;
		}
		return false;
	});
}

void GameScene::DrawPauseMenu() {
	if (phase_ != Phase::kPause) {
		return;
	}

	// カーソル位置を選択中の項目に合わせる
	constexpr float kScreenWidth   = 1280.0f;
	constexpr float kOptionBaseY   = 340.0f;
	constexpr float kOptionSpacing = 80.0f;
	if (spritePauseCursor_) {
		float cursorY = kOptionBaseY + static_cast<float>(pauseSelectedIndex_) * kOptionSpacing + 16.0f;
		spritePauseCursor_->SetPosition(Vector2((kScreenWidth - 320.0f) / 2.0f - 30.0f, cursorY));
	}

	Sprite::PreDraw();
	if (spritePauseOverlay_) {
		spritePauseOverlay_->Draw();
	}
	if (spritePauseTitle_) {
		spritePauseTitle_->Draw();
	}
	for (Sprite* option : spritePauseOptions_) {
		if (option) {
			option->Draw();
		}
	}
	if (spritePauseCursor_) {
		spritePauseCursor_->Draw();
	}
	Sprite::PostDraw();
}

void GameScene::UpdateBlocks() {
	for (auto& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			if (!worldTransformBlock) {
				continue;
			}
			worldTransformBlock->matWorld_ = Math::MakeAffineMatrix(worldTransformBlock->scale_, worldTransformBlock->rotation_, worldTransformBlock->translation_);
			worldTransformBlock->TransferMatrix();
		}
	}
}

void GameScene::CheckAllCollisions() {
	// 判定対象1と2の座標
	AABB aabb1, aabb2;

#pragma region 
	// 自キャラの座標
	aabb1 = player_->GetAABB();

	// 攻撃中かどうか、および前方2マス分の攻撃判定AABBを取得しておく
	// （その場攻撃は移動しないため、体の接触とは別に前方の攻撃判定で敵とのヒットを見る必要がある）
	// ※ 遠距離攻撃機体（ヘヴィボット等）は弾の方で当たり判定を行うため、
	//   ここでの前方近接判定は近接タイプの機体だけに限定する。
	bool playerIsAttacking = player_->IsAttack();
	bool useMeleeAttackAabb = playerIsAttacking && !player_->IsRangedAttacker();
	AABB attackAabb{};
	if (useMeleeAttackAabb) {
		attackAabb = player_->GetAttackAABB();
	}

	// 自キャラと全ての敵キャラの当たり判定
	for (Enemy* enemy : enemies_) {
		// デス演出中などコリジョンが無効な敵はスキップ
		if (enemy->IsCollisionDisabled()) {
			continue;
		}

		// 敵キャラの座標
		aabb2 = enemy->GetAABB();

		// 体の接触、または（近接攻撃中なら）前方の攻撃判定のどちらかが当たっていれば衝突とみなす
		bool hit = IsCollision(aabb1, aabb2) || (useMeleeAttackAabb && IsCollision(attackAabb, aabb2));

		if (hit) {
			// 衝突応答
			// 自キャラの衝突時コールバックを呼び出す（被ダメージ判定。攻撃タイプに関わらず常に行う）
			player_->OnCollision(enemy);

			// 敵の衝突時コールバックを呼び出す（敵を倒す判定）。
			// 遠距離攻撃機体（ヘヴィボット）は近接攻撃を持たないため、
			// 体当たり・攻撃モーション中の接触では敵を倒さない（弾でのみ倒せる）。
			if (!player_->IsRangedAttacker()) {
				enemy->OnCollision(player_);
			}
		}
	}

	// 自キャラと全てのシールド敵キャラの当たり判定
	for (ShieldEnemy* shieldEnemy : shieldEnemies_) {
		// デス演出中などコリジョンが無効な敵はスキップ
		if (shieldEnemy->IsCollisionDisabled()) {
			continue;
		}

		// 敵キャラの座標
		aabb2 = shieldEnemy->GetAABB();

		// 体の接触、または（近接攻撃中なら）前方の攻撃判定のどちらかが当たっていれば衝突とみなす
		bool hit = IsCollision(aabb1, aabb2) || (useMeleeAttackAabb && IsCollision(attackAabb, aabb2));

		if (hit) {
			// 衝突応答
			// 自キャラの衝突時コールバックを呼び出す（被ダメージ判定。攻撃タイプに関わらず常に行う）
			player_->OnCollision(shieldEnemy);

			// 敵の衝突時コールバックを呼び出す（敵を倒す判定）。
			// 遠距離攻撃機体（ヘヴィボット）は近接攻撃を持たないため、
			// 体当たり・攻撃モーション中の接触では敵を倒さない（弾でのみ倒せる）。
			if (!player_->IsRangedAttacker()) {
				shieldEnemy->OnCollision(player_);
			}
		}
	}
#pragma endregion

#pragma region ゴール判定
	// ゴールがCSVに配置されていて、まだゴールしていない場合のみ判定する
	if (hasGoal_ && !isGoal_) {
		// ゴールのマス1つ分のAABBを組み立てる
		AABB aabbGoal;
		aabbGoal.min = {goalPosition_.x - MapChipField::kMapWidth / 2.0f, goalPosition_.y - MapChipField::kMapHeight / 2.0f, goalPosition_.z - 1.0f};
		aabbGoal.max = {goalPosition_.x + MapChipField::kMapWidth / 2.0f, goalPosition_.y + MapChipField::kMapHeight / 2.0f, goalPosition_.z + 1.0f};

		// 自キャラとゴールの交差判定
		if (IsCollision(aabb1, aabbGoal)) {
			// ゴール到達フラグを立てる
			isGoal_ = true;
			// ゴールSEを再生
			SoundManager::GetInstance()->PlaySEGoal();
			// フェードアウトを開始してゲームシーンを終了させる
			// （main側はIsGoal()を見てゴールシーンへ遷移する）
			fade_->Start(Fade::Status::FadeOut, kFadeDuration);
			phase_ = Phase::kFadeOut;
		}
	}
#pragma endregion
}

// ================================================================
//  ヒットエフェクトを生成
// ================================================================
void GameScene::CreateHitEffect(const Vector3& position) {
	HitEffect* newHitEffect = HitEffect::Create(position);
	hitEffects_.push_back(newHitEffect);
}

// ================================================================
//  ガードエフェクトを生成
// ================================================================
void GameScene::CreateGuardEffect(const Vector3& position) {
	GuardEffect* newGuardEffect = GuardEffect::Create(position);
	guardEffects_.push_back(newGuardEffect);
}

void GameScene::Draw() {

	// 描画開始
	Model::PreDraw();

	// 天球描画
	skydome_->Draw(camera_);

	// 背景ビル描画
	if (modelBuilding_) {
		for (WorldTransform* worldTransformBuilding : worldTransformBuildings_) {
			modelBuilding_->Draw(*worldTransformBuilding, camera_);
		}
	}

	// プレイヤー描画
	if (player_ && !player_->IsDead()) {
		player_->Draw();
	}
	// エネミー描画（複数体まとめて描画）
	for (Enemy* enemy : enemies_) {
		enemy->Draw();
	}
	
	// シールド敵描画（複数体まとめて描画）
	for (ShieldEnemy* shieldEnemy : shieldEnemies_) {
		shieldEnemy->Draw();
	}
	
	// ヒットエフェクト描画（複数体まとめて描画）
	for (HitEffect* hitEffect : hitEffects_) {
		hitEffect->Draw();
	}

	// ガードエフェクト描画（複数体まとめて描画）
	for (GuardEffect* guardEffect : guardEffects_) {
		guardEffect->Draw();
	}

	// ブロック描画
	for (auto& worldTransformBlockLine : worldTransformBlocks_) {
		for (WorldTransform* worldTransformBlock : worldTransformBlockLine) {
			if (!worldTransformBlock) {
				continue;
			}
			blockmodel_->Draw(*worldTransformBlock, camera_);
		}
	}

	// 看板描画
	for (Signboard* sign : signs_) {
		sign->Draw(camera_);
	}

	// ゴール描画
	if (hasGoal_ && modelGoal_) {
		modelGoal_->Draw(worldTransformGoal_, camera_);
	}

	// 遠距離攻撃の弾を描画
	for (Projectile* projectile : projectiles_) {
		projectile->Draw(camera_);
	}

	// --- デスパーティクル描画 ---
	// インスタンスが存在するときだけ描画する
	if (deathParticles_) {
		deathParticles_->Draw();
	}

	// 描画終了
	Model::PostDraw();

	// 体力HUDの描画（画面右上）
	DrawLifeIcons();
	// ダッシュ残数HUDの描画（画面左上）
	DrawDashIcons();
	// チュートリアルヒントの描画（画面下部。看板に近い時だけ表示される）
	DrawSignHint();
	// 一時停止メニューの描画（一時停止中のみ）
	DrawPauseMenu();

	// フェードの描画（フェードイン中/フェードアウト中のみ）
	switch (phase_) {
	case Phase::kFadeIn:
	case Phase::kFadeOut:
		fade_->Draw();
		break;
	default:
		break;
	}
}

// デストラクタ
GameScene::~GameScene() {

	// ゲームBGMを停止（ゴール・ゲームオーバーどちらの終了でもここで確実に止める）
	SoundManager::GetInstance()->StopBGM();

	delete player_;
	for (Enemy* enemy : enemies_) {
		delete enemy;
	}
	enemies_.clear();
	delete debugCamera_;
	delete skydome_;
	delete modelSkydome_;
	// 背景ビルモデルの解放
	delete modelBuilding_;
	for (WorldTransform* wt : worldTransformBuildings_) {
		delete wt;
	}
	worldTransformBuildings_.clear();
	// model_ は modelPlayerHeavy_/Speed_/Balance_ のいずれかを指しているだけの
	// エイリアスなので、ここではdeleteしない（実体の解放は下記3行で行う）
	delete modelPlayerHeavy_;
	delete modelPlayerSpeed_;
	delete modelPlayerBalance_;
	delete blockmodel_;
	delete mapChipField_;
	delete cameraController_;
	delete modelEnemy_;
	delete modelShieldEnemy_;

	// デスパーティクルの解放（本チャンの処理）
	delete deathParticles_;
	delete modelDeathParticles_;

	// 攻撃エフェクトモデルの解放
	delete modelAttackEffect_;

	// 背後トレイル（噴射炎）モデルの解放
	delete modelJetTrail_;

	// ヒットエフェクトの解放
	for (HitEffect* hitEffect : hitEffects_) {
		delete hitEffect;
	}
	hitEffects_.clear();
	delete modelHitEffect_;

	// ガードエフェクトの解放
	for (GuardEffect* guardEffect : guardEffects_) {
		delete guardEffect;
	}
	guardEffects_.clear();
	delete modelGuardEffect_;

	// ゴールモデルの解放
	delete modelGoal_;

	// フェードの解放
	delete fade_;

	// 体力HUDアイコンの解放
	for (Sprite* icon : spriteLifeIcons_) {
		delete icon;
	}
	spriteLifeIcons_.clear();

	// ダッシュ残数HUDアイコンの解放
	for (Sprite* icon : spriteDashIcons_) {
		delete icon;
	}
	spriteDashIcons_.clear();

	// 看板の解放
	for (Signboard* sign : signs_) {
		delete sign;
	}
	signs_.clear();

	// 遠距離攻撃の弾の解放
	for (Projectile* projectile : projectiles_) {
		delete projectile;
	}
	projectiles_.clear();

	// ヒント画像の解放
	for (Sprite* hint : spriteHints_) {
		delete hint;
	}
	spriteHints_.clear();

	// 一時停止メニュー用スプライトの解放
	delete spritePauseOverlay_;
	delete spritePauseTitle_;
	delete spritePauseOptions_[0];
	delete spritePauseOptions_[1];
	delete spritePauseCursor_;

	// ブロック解放
	for (auto& row : worldTransformBlocks_) {
		for (WorldTransform* block : row) {
			if (block) {
				delete block;
			}
		}
	}
	worldTransformBlocks_.clear();
}