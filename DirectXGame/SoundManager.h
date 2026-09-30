#pragma once
#include <cstdint>

/// <summary>
/// BGM・効果音の読み込みと再生をまとめて管理するクラス（シングルトン）
/// ※ KamataEngineのAudioクラスのAPI（LoadWave/PlayWave/StopWave）を想定して実装しています。
///   使用しているエンジンのバージョンによってメソッド名が異なる場合は、
///   SoundManager.cpp内の該当箇所を実際のAPIに合わせて調整してください。
/// </summary>
class SoundManager {
public:
	// シングルトンインスタンスの取得
	static SoundManager* GetInstance();

	// 全てのサウンドデータを読み込む（起動時に一度だけ呼ぶ）
	void Initialize();

	// --- BGM ---
	// タイトルBGMをループ再生する（既に再生中なら何もしない）
	void PlayBGMTitle();
	// ゲームBGMをループ再生する（既に再生中なら何もしない）
	void PlayBGMGame();
	// ゲームオーバーBGMをループ再生する（既に再生中なら何もしない）
	void PlayBGMGameOver();
	// 再生中のBGMを停止する
	void StopBGM();

	// --- 効果音（SE） ---
	void PlaySEJump();          // ジャンプ
	void PlaySEDash();          // ダッシュ
	void PlaySEAttack();        // 攻撃（斬撃）
	void PlaySEEnemyDefeat();   // 敵を倒した
	void PlaySEPlayerDamage();  // 自キャラがダメージを受けた（デス演出開始）
	void PlaySEGoal();          // ゴール到達
	void PlaySEClear();         // ゲームクリア（GoalScene開始時に1回再生）
	void PlaySESelect();        // 決定音（タイトル/ゴール/ゲームオーバー画面の決定）

private:
	SoundManager()  = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&)            = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	bool initialized_ = false;

	// --- BGM用サウンドデータハンドル ---
	uint32_t soundBGMTitle_    = 0;
	uint32_t soundBGMGame_     = 0;
	uint32_t soundBGMGameOver_ = 0;

	// 現在再生中のBGMのボイスハンドル（停止に使う）
	uint32_t voiceBGM_    = 0;
	bool     isBGMPlaying_ = false;

	// --- 効果音用サウンドデータハンドル ---
	uint32_t soundSEJump_         = 0;
	uint32_t soundSEDash_         = 0;
	uint32_t soundSEAttack_       = 0;
	uint32_t soundSEEnemyDefeat_  = 0;
	uint32_t soundSEPlayerDamage_ = 0;
	uint32_t soundSEGoal_         = 0;
	uint32_t soundSEClear_        = 0;
	uint32_t soundSESelect_       = 0;
};
