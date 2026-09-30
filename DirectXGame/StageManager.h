#pragma once
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include "CharacterData.h"

/// <summary>
/// ステージデータ
/// </summary>
struct StageData {
	std::string name;  // ステージ名（フィールドCSVファイル名）
	int32_t timeLimit; // 制限時間（秒）
};

/// <summary>
/// ステージ管理
/// </summary>
class StageManager {
public:
	/// <summary>
	/// ステージデータファイルの読み込み
	/// </summary>
	void ステージデータファイルの読み込み();

	/// <summary>
	/// ステージデータの取得
	/// </summary>
	/// <param name="index">ステージ番号</param>
	/// <returns>ステージデータ</returns>
	const StageData& GetStageData(int32_t index) const {
		assert(index >= 0 && index < static_cast<int32_t>(stageDatas_.size()) && "indexが正常な範囲である");
		return stageDatas_[index];
	}

	/// <summary>
	/// 現在ステージのステージデータ取得
	/// </summary>
	/// <returns>ステージデータ</returns>
	const StageData& GetCurrentStageData() const { return GetStageData(currentStageIndex_); }

	/// <summary>
	/// 現在ステージ番号のsetter
	/// </summary>
	/// <param name="index">ステージ番号</param>
	void SetCurrentStageIndex(int32_t index) {
		assert(index >= 0 && index < static_cast<int32_t>(stageDatas_.size()) && "indexが正常な範囲である");
		currentStageIndex_ = index;
	}

	/// <summary>
	/// 現在ステージ番号のgetter
	/// </summary>
	int32_t GetCurrentStageIndex() const { return currentStageIndex_; }

	/// <summary>
	/// ステージ総数のgetter
	/// </summary>
	int32_t GetStageCount() const { return static_cast<int32_t>(stageDatas_.size()); }

	// --- 体力 ---

	/// <summary>
	/// 体力を、選択中の機体の最大値にリセットする（新しい周回の開始時に呼ぶ）
	/// ※ SetSelectedCharacter()より後に呼ぶこと（先に機体を決めておく必要がある）
	/// </summary>
	void ResetHP() { hp_ = GetSelectedCharacterStats().maxHP; }

	/// <summary>
	/// 体力を1減らす
	/// </summary>
	/// <returns>減らした後の体力（0未満にはならない）</returns>
	int32_t DecrementHP() {
		if (hp_ > 0) {
			--hp_;
		}
		return hp_;
	}

	/// <summary>
	/// 現在の体力のgetter
	/// </summary>
	int32_t GetHP() const { return hp_; }

	/// <summary>
	/// 選択中の機体の最大体力のgetter
	/// </summary>
	int32_t GetMaxHP() const { return GetSelectedCharacterStats().maxHP; }

	// --- 選択機体 ---

	/// <summary>
	/// 選択中の機体タイプのsetter
	/// </summary>
	void SetSelectedCharacter(CharacterType type) { selectedCharacter_ = type; }

	/// <summary>
	/// 選択中の機体タイプのgetter
	/// </summary>
	CharacterType GetSelectedCharacter() const { return selectedCharacter_; }

	/// <summary>
	/// 選択中の機体のステータスを取得する
	/// </summary>
	CharacterStats GetSelectedCharacterStats() const { return GetCharacterStats(selectedCharacter_); }

private:
	// 全ステージデータ
	std::vector<StageData> stageDatas_;

	// 現在のステージ番号
	int32_t currentStageIndex_ = 0;

	// 現在の体力（機体選択前のデフォルト値として、バランサーの最大体力を初期値にしておく）
	int32_t hp_ = 3;

	// 選択中の機体タイプ（デフォルトはバランサー）
	CharacterType selectedCharacter_ = CharacterType::kBalance;
};
