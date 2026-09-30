#pragma once
#include <string>

/// <summary>
/// プレイヤーが選べる機体の種類
/// </summary>
enum class CharacterType {
	kHeavy = 0,   // ヘヴィボット（重装型）
	kSpeed = 1,   // スピードボット（俊敏型）
	kBalance = 2, // バランサー（標準型）
};

// 選べる機体の総数
static inline const int32_t kCharacterTypeCount = 3;

/// <summary>
/// 機体ごとの性能差分
/// </summary>
struct CharacterStats {
	std::string name;       // 機体名
	std::string description; // 機体の特徴（一言）
	float acceleration;     // 加速度
	float limitRunSpeed;    // 最高速度
	float jumpAcceleration; // ジャンプ初速
	float dashVelocity;     // 突進（ダッシュ）速度
	float attackRange;      // 攻撃の届く範囲（マス数）。遠距離攻撃機体の場合は弾の飛距離になる
	int32_t maxHP;          // 最大体力
	int32_t maxDashCharges; // ダッシュの最大使用可能回数（ストック数）
	float dashRechargeTime; // ダッシュ1回分が回復するまでの時間[秒]
	bool isRangedAttack;    // true: 弾を飛ばす遠距離攻撃 / false: その場の近接攻撃
};

/// <summary>
/// 機体タイプに対応するステータスを取得する
/// </summary>
inline CharacterStats GetCharacterStats(CharacterType type) {
	switch (type) {
	case CharacterType::kHeavy:
		return CharacterStats{
		    "ヘヴィボット",
		    "重装甲・遠距離砲撃の重戦車タイプ",
		    0.03f, // acceleration（やや遅い加速）
		    0.16f, // limitRunSpeed（やや遅い最高速）
		    0.75f, // jumpAcceleration（低めのジャンプ）
		    0.22f, // dashVelocity（短めの突進）
		    6.0f,  // attackRange（遠距離攻撃の飛距離。他2機体より大幅に長い）
		    5,     // maxHP（重装甲で耐久力が高い）
		    1,     // maxDashCharges（ダッシュは1回のみ、燃費が悪い）
		    2.5f,  // dashRechargeTime（回復も遅い）
		    true,  // isRangedAttack（弾を飛ばす遠距離攻撃。3機体中これだけ）
		};

	case CharacterType::kSpeed:
		return CharacterStats{
		    "スピードボット",
		    "高速機動・一点集中攻撃の俊敏タイプ",
		    0.06f, // acceleration（速い加速）
		    0.30f, // limitRunSpeed（速い最高速）
		    1.05f, // jumpAcceleration（高いジャンプ）
		    0.40f, // dashVelocity（速く長い突進）
		    1.5f,  // attackRange（狭い攻撃範囲）
		    2,     // maxHP（機動力と引き換えに打たれ弱い）
		    3,     // maxDashCharges（ダッシュを3回ストックできる）
		    1.0f,  // dashRechargeTime（回復も速い）
		    false, // isRangedAttack（近接攻撃）
		};

	case CharacterType::kBalance:
	default:
		return CharacterStats{
		    "バランサー",
		    "扱いやすい標準タイプ",
		    0.04f, // acceleration（標準）
		    0.22f, // limitRunSpeed（標準）
		    0.9f,  // jumpAcceleration（標準）
		    0.3f,  // dashVelocity（標準）
		    2.0f,  // attackRange（標準）
		    3,     // maxHP（標準）
		    2,     // maxDashCharges（標準）
		    1.5f,  // dashRechargeTime（標準）
		    false, // isRangedAttack（近接攻撃）
		};
	}
}
