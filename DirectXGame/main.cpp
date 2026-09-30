#include "kamataEngine.h"
#include <Windows.h>
#include "GameScene.h"
#include "TitleScene.h"
#include "GoalScene.h"
#include "GameOverScene.h"
#include "CharacterSelectScene.h"
#include "StageManager.h"
#include "SoundManager.h"

using namespace KamataEngine;

// シーン（型）
enum class Scene {
	kUnknown = 0,

	kTitle,
	kCharacterSelect,
	kGame,
	kGoal,
	kGameOver,
};

// 現在シーン（変数）
Scene scene = Scene::kUnknown;

// 各シーンのポインタ（グローバル変数）
GameScene* gameScene = nullptr;
TitleScene* titleScene = nullptr;
GoalScene* goalScene = nullptr;
GameOverScene* gameOverScene = nullptr;
CharacterSelectScene* characterSelectScene = nullptr;

// ステージマネージャのポインタ（グローバル変数）
StageManager* stageManager = nullptr;

// シーンを切り替える処理
void ChangeScene();
// 現在シーンを更新する処理
void UpdateScene();
// 現在シーンを描画する処理
void DrawScene();

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	//エンジンの初期化
	KamataEngine::Initialize(L"LE2C_15_タナカ_トモヤ_スリーボッツ！");

	//DirectXCommonのインスタンス取得
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// BGM・効果音の読み込み（起動時に一度だけ）
	SoundManager::GetInstance()->Initialize();

	// ステージマネージャの生成
	stageManager = new StageManager;
	// ステージデータファイルの読み込み
	stageManager->ステージデータファイルの読み込み();

	// 最初のシーンの初期化
	scene = Scene::kTitle;
	titleScene = new TitleScene();
	titleScene->Initialize();

#ifdef _DEBUG
	// デバッグビルドでは直接ゲームシーンから始まるようにしておくと
	// 効率的にゲームシーンを開発することができる
	scene = Scene::kGame;
	delete titleScene;
	titleScene = nullptr;
	gameScene = new GameScene();
	gameScene->Initialize(stageManager);
	// タイトルBGMを飛ばして、直接ゲームBGMを再生しておく
	SoundManager::GetInstance()->PlayBGMGame();
#endif

	//メインループ
	while (true) {
		//エンジンの更新
		if (KamataEngine::Update()) {
			break;
		}

		// シーン切り替え
		ChangeScene();

		// 現在シーン更新
		UpdateScene();

		//描画開始
		dxCommon->PreDraw();

		// 現在シーンの描画
		DrawScene();

		// 描画終了
		dxCommon->PostDraw();
	}

	// シーンの解放
	// ポインタがnullptrの場合は、deleteしても何も起こらないので安全。
	delete titleScene;
	delete gameScene;
	delete goalScene;
	delete gameOverScene;
	delete characterSelectScene;

	// ステージマネージャの解放
	delete stageManager;

	//エンジンの終了処理
	KamataEngine::Finalize();
	return 0;
}

void ChangeScene() {
	switch (scene) {
	case Scene::kTitle:
		if (titleScene->IsFinished()) {
			// シーン変更（キャラ選択より先にチュートリアルをプレイしてもらう）
			scene = Scene::kGame;
			// 旧シーンの解放
			delete titleScene;
			titleScene = nullptr;
			// チュートリアルはステージ番号0番固定。機体はまだ選んでいないので
			// StageManagerのデフォルト（バランサー）のステータスでプレイする。
			stageManager->SetCurrentStageIndex(0);
			stageManager->ResetHP();
			// 新シーンの生成と初期化
			gameScene = new GameScene();
			gameScene->Initialize(stageManager);
		}
		break;
	case Scene::kCharacterSelect:
		if (characterSelectScene->IsFinished()) {
			// シーン変更
			scene = Scene::kGame;
			// 選択された機体をステージマネージャに記録しておく
			stageManager->SetSelectedCharacter(characterSelectScene->GetSelectedCharacter());
			// 旧シーンの解放
			delete characterSelectScene;
			characterSelectScene = nullptr;
			// チュートリアル（ステージ0番）は既にプレイ済みなので、stage1（1番）から開始する
			stageManager->SetCurrentStageIndex(1);
			// 体力も選択機体の最大値にリセットしておく
			stageManager->ResetHP();
			// 新シーンの生成と初期化
			gameScene = new GameScene();
			gameScene->Initialize(stageManager);
		}
		break;
	case Scene::kGame:
		if (gameScene->IsFinished()) {
			if (gameScene->IsReturnToTitle()) {
				// ポーズメニューから「タイトルへ戻る」が選ばれた場合
				scene = Scene::kTitle;
				// 旧シーンの解放
				delete gameScene;
				gameScene = nullptr;
				// 新シーンの生成と初期化
				titleScene = new TitleScene();
				titleScene->Initialize();
			} else if (gameScene->IsGoal()) {
				int32_t currentStageIndex = stageManager->GetCurrentStageIndex();

				if (currentStageIndex == 0) {
					// チュートリアル（ステージ0番）をクリアしたので、キャラクター選択へ
					scene = Scene::kCharacterSelect;
					// 旧シーンの解放
					delete gameScene;
					gameScene = nullptr;
					// 新シーンの生成と初期化
					characterSelectScene = new CharacterSelectScene();
					characterSelectScene->Initialize();
				} else {
					// 次のステージ番号
					int32_t nextStageIndex = currentStageIndex + 1;

					if (nextStageIndex < stageManager->GetStageCount()) {
						// まだ残りのステージがある場合は、そのまま次のステージへ進む
						// （kGameシーンのまま、内部でGameSceneだけ作り直す）
						stageManager->SetCurrentStageIndex(nextStageIndex);
						// 旧シーンの解放
						delete gameScene;
						gameScene = nullptr;
						// 新シーンの生成と初期化
						gameScene = new GameScene();
						gameScene->Initialize(stageManager);
					} else {
						// 最終ステージをクリアした場合はゴールシーンへ
						scene = Scene::kGoal;
						// 旧シーンの解放
						delete gameScene;
						gameScene = nullptr;
						// 新シーンの生成と初期化
						goalScene = new GoalScene();
						goalScene->Initialize();
					}
				}
			} else if (gameScene->IsGameOver()) {
				// 死亡していたらゲームオーバーシーンへ
				scene = Scene::kGameOver;
				// 旧シーンの解放
				delete gameScene;
				gameScene = nullptr;
				// 新シーンの生成と初期化
				gameOverScene = new GameOverScene();
				gameOverScene->Initialize();
			} else if (gameScene->IsRetry()) {
				// 死亡したが体力が残っているので、同じステージを最初からやり直す
				// （kGameシーンのまま、内部でGameSceneだけ作り直す。ステージ番号は変更しない）
				// 旧シーンの解放
				delete gameScene;
				gameScene = nullptr;
				// 新シーンの生成と初期化
				gameScene = new GameScene();
				gameScene->Initialize(stageManager);
			} else {
				// それ以外の終了要因の場合はタイトルへ戻す
				scene = Scene::kTitle;
				// 旧シーンの解放
				delete gameScene;
				gameScene = nullptr;
				// 新シーンの生成と初期化
				titleScene = new TitleScene();
				titleScene->Initialize();
			}
		}
		break;
	case Scene::kGoal:
		if (goalScene->IsFinished()) {
			// シーン変更
			scene = Scene::kTitle;
			// 旧シーンの解放
			delete goalScene;
			goalScene = nullptr;
			// 新シーンの生成と初期化
			titleScene = new TitleScene();
			titleScene->Initialize();
		}
		break;
	case Scene::kGameOver:
		if (gameOverScene->IsFinished()) {
			// 特定のボタン（SPACEキー）でタイトルへ戻る
			scene = Scene::kTitle;
			// 旧シーンの解放
			delete gameOverScene;
			gameOverScene = nullptr;
			// 新シーンの生成と初期化
			titleScene = new TitleScene();
			titleScene->Initialize();
		}
		break;
	}
}

void UpdateScene() {
	switch (scene) {
	case Scene::kTitle:
		titleScene->Update();
		break;
	case Scene::kCharacterSelect:
		characterSelectScene->Update();
		break;
	case Scene::kGame:
		gameScene->Update();
		break;
	case Scene::kGoal:
		goalScene->Update();
		break;
	case Scene::kGameOver:
		gameOverScene->Update();
		break;
	}
}

void DrawScene() {
	switch (scene) {
	case Scene::kTitle:
		titleScene->Draw();
		break;
	case Scene::kCharacterSelect:
		characterSelectScene->Draw();
		break;
	case Scene::kGame:
		gameScene->Draw();
		break;
	case Scene::kGoal:
		goalScene->Draw();
		break;
	case Scene::kGameOver:
		gameOverScene->Draw();
		break;
	}
}
