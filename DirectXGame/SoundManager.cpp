#include "SoundManager.h"
#include "KamataEngine.h"

using namespace KamataEngine;

SoundManager* SoundManager::GetInstance() {
	static SoundManager instance;
	return &instance;
}

void SoundManager::Initialize() {
	// 二重読み込みを防ぐ
	if (initialized_) {
		return;
	}

	// --- BGM読み込み ---
	soundBGMTitle_ = Audio::GetInstance()->LoadWave("sound/BGM/bgm_title.mp3");
	soundBGMGame_ = Audio::GetInstance()->LoadWave("sound/BGM/bgm_game.mp3");
	soundBGMGameOver_ = Audio::GetInstance()->LoadWave("sound/BGM/bgm_gameover.mp3");

	// --- 効果音読み込み ---
	// WAVファイルを読み込む
	soundSEJump_ = Audio::GetInstance()->LoadWave("sound/SE/se_jump.wav");
	// WAVファイルを読み込む
	soundSEDash_ = Audio::GetInstance()->LoadWave("sound/SE/se_dash.wav");
	// WAVファイルを読み込む
	soundSEAttack_ = Audio::GetInstance()->LoadWave("sound/SE/se_attack.wav");
	// WAVファイルを読み込む
	soundSEEnemyDefeat_ = Audio::GetInstance()->LoadWave("sound/SE/se_enemyDefeat.wav");
	// WAVファイルを読み込む
	soundSEPlayerDamage_ = Audio::GetInstance()->LoadWave("sound/SE/se_playerDamage.wav");
	// WAVファイルを読み込む
	soundSEGoal_ = Audio::GetInstance()->LoadWave("sound/SE/se_goal.wav");
	// WAVファイルを読み込む
	soundSEClear_ = Audio::GetInstance()->LoadWave("sound/SE/se_clear.wav");
	// WAVファイルを読み込む
	soundSESelect_ = Audio::GetInstance()->LoadWave("sound/SE/se_select.wav");

	initialized_ = true;
}

// ================================================================
//  BGM
// ================================================================
void SoundManager::PlayBGMTitle() {
	if (isBGMPlaying_) {
		return;
	}
	// 読み込んだWAVファイルを再生（ループ再生）
	voiceBGM_ = Audio::GetInstance()->PlayWave(soundBGMTitle_, true);
	isBGMPlaying_ = true;
}

void SoundManager::PlayBGMGame() {
	if (isBGMPlaying_) {
		return;
	}
	// 読み込んだWAVファイルを再生（ループ再生）
	voiceBGM_ = Audio::GetInstance()->PlayWave(soundBGMGame_, true);
	isBGMPlaying_ = true;
}

void SoundManager::PlayBGMGameOver() {
	if (isBGMPlaying_) {
		return;
	}
	voiceBGM_ = Audio::GetInstance()->PlayWave(soundBGMGameOver_, true);
	isBGMPlaying_ = true;
}

void SoundManager::StopBGM() {
	if (!isBGMPlaying_) {
		return;
	}
	// 音声を停止する
	Audio::GetInstance()->StopWave(voiceBGM_);
	isBGMPlaying_ = false;
}

// ================================================================
//  効果音（SE）
// ================================================================
void SoundManager::PlaySEJump() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEJump_);
}

void SoundManager::PlaySEDash() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEDash_);
}

void SoundManager::PlaySEAttack() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEAttack_);
}

void SoundManager::PlaySEEnemyDefeat() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEEnemyDefeat_);
}

void SoundManager::PlaySEPlayerDamage() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEPlayerDamage_);
}

void SoundManager::PlaySEGoal() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEGoal_);
}

void SoundManager::PlaySEClear() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSEClear_);
}

void SoundManager::PlaySESelect() {
	// 読み込んだWAVファイルを再生
	Audio::GetInstance()->PlayWave(soundSESelect_);
}
