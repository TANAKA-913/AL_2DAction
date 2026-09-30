#include "Fade.h"
#include <algorithm>

using namespace KamataEngine;

Fade::~Fade() {
	// スプライトの解放
	delete sprite_;
}

void Fade::Initialize() {
	// スプライト生成（真っ白の1x1テクスチャを黒く着色して使う）
	sprite_ = Sprite::Create(TextureManager::GetInstance()->Load("white1x1.png"), {0, 0});

	// 画面全体を覆うサイズに設定（画面横幅, 画面縦幅）
	sprite_->SetSize(Vector2(1280.0f, 720.0f));

	// 色指定。RGBAをそれぞれ0.0f～1.0fの間で指定する。
	// 黒以外の色でフェードを実装することもできる。（白フェードもよく使われる）
	sprite_->SetColor(Vector4(0, 0, 0, 1));
}

void Fade::Start(Status status, float duration) {
	status_ = status;
	duration_ = duration;
	counter_ = 0.0f;
}

void Fade::Stop() { status_ = Status::None; }

void Fade::Update() {
	// フェード状態による分岐
	switch (status_) {
	case Status::None:
		// 何もしない
		break;

	case Status::FadeIn:
		// 1フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 1.0fから0.0fの間で、経過時間がフェード継続時間に近づくほどアルファ値を小さくする
		// （フェードアウトとは増減が逆になる）
		sprite_->SetColor(Vector4(0, 0, 0, 1.0f - std::clamp(counter_ / duration_, 0.0f, 1.0f)));
		break;

	case Status::FadeOut:
		// 1フレーム分の秒数をカウントアップ
		counter_ += 1.0f / 60.0f;
		// フェード継続時間に達したら打ち止め
		if (counter_ >= duration_) {
			counter_ = duration_;
		}
		// 0.0fから1.0fの間で、経過時間がフェード継続時間に近づくほどアルファ値を大きくする
		sprite_->SetColor(Vector4(0, 0, 0, std::clamp(counter_ / duration_, 0.0f, 1.0f)));
		break;
	}
}

bool Fade::IsFinished() const {
	// フェード状態による分岐
	switch (status_) {
	case Status::FadeIn:
	case Status::FadeOut:
		if (counter_ >= duration_) {
			return true;
		} else {
			return false;
		}
	default:
		break;
	}

	return true;
}

void Fade::Draw() {
	// フェード状態でない場合は描画をスキップする
	// （透明なスプライトを全画面に描画するのは意外に描画コストが高い）
	if (status_ == Status::None) {
		return;
	}

	// スプライトの描画。
	// KamataEngineでは、スプライトのDraw()は必ずSprite::PreDraw()とSprite::PostDraw()の間で
	// 呼び出す必要がある（DirectXの内部処理として、スプライト描画用のパイプラインに切り替えている）。
	Sprite::PreDraw();

	sprite_->Draw();

	Sprite::PostDraw();
}
