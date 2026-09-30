#pragma once
#include "kamataEngine.h"

/// <summary>
/// チュートリアル用の看板。
/// マップ上に置かれ、プレイヤーが近づくと対応するヒント画像がHUDに表示される。
/// 見た目は仮実装として、既存のブロックモデルを縦長に変形させて流用している。
/// </summary>
class Signboard {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="model">表示に使うモデル（ブロックモデルを流用）</param>
	/// <param name="position">設置座標</param>
	/// <param name="hintIndex">表示するヒントの種類（0:移動 1:ジャンプ 2:ダッシュ 3:攻撃 4:ゴール）</param>
	void Initialize(KamataEngine::Model* model, const KamataEngine::Vector3& position, int32_t hintIndex);

	void Draw(const KamataEngine::Camera& camera);

	KamataEngine::Vector3 GetWorldPosition() const;

	int32_t GetHintIndex() const { return hintIndex_; }

private:
	KamataEngine::WorldTransform worldTransform_;
	KamataEngine::Model* model_ = nullptr;
	int32_t hintIndex_ = 0;
};
