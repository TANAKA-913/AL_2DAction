#include "MapChipField.h"
#include <cassert>
#include <fstream>
#include <map>
#include <sstream>

namespace {

// マップチップ種別テーブル
// ('B'だったらkBlock、みたいな組み合わせを登録するためのテーブル)
std::map<char, MapChipType> mapChipTypeTable = {
    {'B', MapChipType::kBlock},
    {'P', MapChipType::kPlayer},
    {'E', MapChipType::kEnemy},
    {'G', MapChipType::kGoal},
    {'S', MapChipType::kSign},
};

} // namespace

// マップチップデータをリセット
void MapChipField::ResetMapChipData() {
	mapChipData_.data.clear();
	mapChipData_.data.resize(kNumBlockVirtical);
	for (std::vector<MapChipDataUnit>& mapChipDataLine : mapChipData_.data) {
		mapChipDataLine.resize(kNumBlockHorizontal);
	}
}

// CSV読み込み
void MapChipField::LoadMapChipCsv(const std::string& filePath) {
	ResetMapChipData();

	std::ifstream file;
	file.open(filePath);

	assert(file.is_open());

	std::stringstream mapChipCsv;
	mapChipCsv << file.rdbuf();

	file.close();

	for (uint32_t i = 0; i < kNumBlockVirtical; ++i) {

		std::string line;
		getline(mapChipCsv, line);

		std::istringstream lineStream(line);

		for (uint32_t j = 0; j < kNumBlockHorizontal; ++j) {

			std::string word;
			getline(lineStream, word, ',');

			// 空白の場合はスキップ（空白マスが確定する）
			if (word.empty()) {
				continue;
			}

			// 先頭文字がいずれかのマップチップ種別に該当するか確認
			if (!mapChipTypeTable.contains(word[kChipType])) {
				continue;
			}

			// 先頭文字でマップチップのタイプを判別
			mapChipData_.data[i][j].type = mapChipTypeTable[word[kChipType]];

			// サブIDを含まない場合はスキップ（0番で確定）
			if (word.size() <= kChipSubID) {
				continue;
			}

			// マップチップのサブIDを設定
			mapChipData_.data[i][j].subID = static_cast<uint8_t>(word[kChipSubID] - '0');
		}
	}
}

// マップチップ種類取得
MapChipType MapChipField::GetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex) {
	if (xIndex >= kNumBlockHorizontal) {
		return MapChipType::kBlank;
	}

	if (yIndex >= kNumBlockVirtical) {
		return MapChipType::kBlank;
	}

	return mapChipData_.data[yIndex][xIndex].type;
}

// マップチップサブID取得
uint8_t MapChipField::GetMapChipSubIDByIndex(uint32_t xIndex, uint32_t yIndex) {
	if (xIndex >= kNumBlockHorizontal) {
		return 0;
	}

	if (yIndex >= kNumBlockVirtical) {
		return 0;
	}

	return mapChipData_.data[yIndex][xIndex].subID;
}

// 座標からマップチップ番号（IndexSet）を取得
MapChipField::IndexSet MapChipField::GetMapChipIndexSetByPosition(const KamataEngine::Vector3& position) {
	MapChipField::IndexSet indexSet = {};
	// X番号：(X座標 + ブロック幅/2) / ブロック幅（小数点切り捨て）
	indexSet.xIndex = static_cast<uint32_t>((position.x + kMapWidth / 2.0f) / kMapWidth);
	// Y番号：反転前Y番号を求めてから反転
	uint32_t yIndexBeforeFlip = static_cast<uint32_t>((position.y + kMapHeight / 2.0f) / kMapHeight);
	indexSet.yIndex = kNumBlockVirtical - 1 - yIndexBeforeFlip;
	return indexSet;
}

// マップチップ番号から境界矩形（Rect）を取得
MapChipField::Rect MapChipField::GetRectByIndex(uint32_t xIndex, uint32_t yIndex) {
	KamataEngine::Vector3 center = GetMapChipPositionByIndex(xIndex, yIndex);
	Rect rect;
	rect.left   = center.x - kMapWidth  / 2.0f;
	rect.right  = center.x + kMapWidth  / 2.0f;
	rect.bottom = center.y - kMapHeight / 2.0f;
	rect.top    = center.y + kMapHeight / 2.0f;
	return rect;
}

// マップチップ座標取得 (KamataEngine:: を追加してヘッダーと一致させました)
KamataEngine::Vector3 MapChipField::GetMapChipPositionByIndex(uint32_t xIndex, uint32_t yIndex) {
	if (xIndex >= kNumBlockHorizontal) {
		return KamataEngine::Vector3{0.0f, 0.0f, 0.0f};
	}

	if (yIndex >= kNumBlockVirtical) {
		return KamataEngine::Vector3{0.0f, 0.0f, 0.0f};
	}

	return KamataEngine::Vector3{kMapWidth * xIndex, kMapHeight * (kNumBlockVirtical - 1 - yIndex), 0.0f};
}