#include "StageManager.h"
#include <cassert>
#include <fstream>
#include <sstream>

void StageManager::ステージデータファイルの読み込み() {
	// ステージデータファイルのパス
	const std::string filePath = "Resources/stages/stageDatas.csv";

	// ifstreamでステージデータファイルを開く
	std::ifstream file(filePath);
	assert(file.is_open() && "ステージデータファイルが存在しません");

	// ファイルの内容を格納するstringstreamの宣言
	std::stringstream stageDataCsv;
	// ファイルの内容をstringstreamにコピーする
	stageDataCsv << file.rdbuf();
	// ファイルを閉じる
	file.close();

	// ステージデータを最終行まで1行ずつ読み込む
	// 1行分の内容を格納するstringの宣言
	std::string line;
	while (std::getline(stageDataCsv, line)) {
		// 1行分の内容を格納するstringstreamを宣言して、stringから変換
		std::istringstream lineStream(line);

		// 空行はスキップする
		if (line.empty()) {
			continue;
		}

		// ステージデータを格納する構造体
		StageData stageData;
		// カンマ区切りの一つ分を格納するstringの宣言
		std::string word;

		// カンマ区切りで次のデータを取得する
		std::getline(lineStream, word, ',');
		// ステージ名を格納する
		stageData.name = word;

		// カンマ区切りで次のデータを取得する
		std::getline(lineStream, word, ',');
		// 整数に変換して制限時間を格納する
		stageData.timeLimit = std::stoi(word);

		// ステージデータテーブルに格納する
		stageDatas_.push_back(stageData);
	}
}
