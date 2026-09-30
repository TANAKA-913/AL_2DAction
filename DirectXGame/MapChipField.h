#pragma once
#include "KamataEngine.h"
#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum class MapChipType {
	kBlank,
	kBlock,
	kPlayer, // プレイヤー
	kEnemy,  // 敵
	kGoal,   // ゴール（仮実装：見た目は持たず当たり判定のみ）
	kSign,   // 看板（サブIDでヒント内容を判別。チュートリアル用）
};

// 1マス分のデータ
struct MapChipDataUnit {
	MapChipType type; // マップチップの種別
	uint8_t subID;    // 種類ごとのサブID
};

// ステージ全体のマップチップデータ
struct MapChipData {
	std::vector<std::vector<MapChipDataUnit>> data;
};

class MapChipField {
public:
	// マップチップCSVの文字番号
	enum MapChipCharIndex {
		kChipType = 0,  // マップチップタイプ
		kChipSubID = 1, // タイプごとのサブID
	};

	static inline const float kMapWidth = 1.0f;
	static inline const float kMapHeight = 1.0f;

	// 内部の定数は元の Virtical のまま維持します
	static inline const uint32_t kNumBlockVirtical = 40;
	static inline const uint32_t kNumBlockHorizontal = 200;

	// CSV読み込み
	void LoadMapChipCsv(const std::string& filePath);

	// 初期化
	void ResetMapChipData();

	// マップチップ取得
	MapChipType GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex);

	// マップチップサブID取得
	uint8_t GetMapChipSubIDByIndex(uint32_t xIndex, uint32_t yIndex);

	// マップチップ座標取得 (KamataEngine:: を明示)
	KamataEngine::Vector3 GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex);

	// --- 以下の2つの関数（ゲッター）を追加しました ---
	uint32_t GetNumBlockVertical() const { return kNumBlockVirtical; }
	uint32_t GetNumBlockHorizontal() const { return kNumBlockHorizontal; }

	// マップチップ番号のセット
	struct IndexSet {
		uint32_t xIndex;
		uint32_t yIndex;
	};

	// 範囲矩形
	struct Rect {
		float left;   // 左端
		float right;  // 右端
		float bottom; // 下端
		float top;    // 上端
	};

	// 座標からマップチップ番号を取得
	IndexSet GetMapChipIndexSetByPosition(const KamataEngine::Vector3& position);

	// マップチップ番号から境界矩形を取得
	Rect GetRectByIndex(uint32_t xIndex, uint32_t yIndex);

private:
	MapChipData mapChipData_;
};