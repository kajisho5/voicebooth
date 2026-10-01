#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Motion.h"
#include <optional>

/*  動きの共通の時計（DESIGN 4.10）
    - 画面全体で 1 つのタイマー（60 Hz）だけを回す。部品ごとにタイマーを持たない
    - 動いている部品が無くなったら止まる（止まっている時は描かない）
    - 動くのはメッセージスレッドだけ。オーディオスレッドには何も足さない */

namespace vb::motion
{
/** OS の「視差効果を減らす」（Mac）/「アニメーションを表示しない」（Windows）が有効か。Linux は false。
    数秒ごとに読み直す（設定を変えたらすぐ反映） */
bool prefersReducedMotion();

/** 起動オプション --reduce-motion / --motion（確認用）。nullopt で OS に従う */
void setReducedMotionOverride (std::optional<bool>);

/** OS の設定を読む（ReducedMotion.cpp / .mm） */
bool systemPrefersReducedMotion();

/** 動く部品。startAnimating() で時計に載り、advanceAnimation() が false を返すと降りる */
class Animated
{
public:
    Animated() = default;
    virtual ~Animated();

    /** 時計に載せる（載っていれば何もしない） */
    void startAnimating();
    void stopAnimating();
    bool isAnimating() const { return animating; }

    /** 時計から：1 フレーム進める（dt 秒）。まだ動くなら true。描き直しは自分で repaint する */
    virtual bool advanceAnimation (float dt) = 0;

private:
    friend class Driver;
    bool animating = false;

    JUCE_DECLARE_NON_COPYABLE (Animated)
};

/** 時計の経過時間（秒）。呼吸・明滅の位相に使う */
double now();
} // namespace vb::motion
