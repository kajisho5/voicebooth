#pragma once

#include <juce_events/juce_events.h>
#include "AudioEngine.h"

/*  マイクの使用許可（DESIGN 5 / 13「Mac 権限なし：セットアップで止める」）
    Mac だけ OS（AVFoundation）に聞く。許可が無いと Core Audio は無音を返すだけで、エラーにならないため。
    Win / Linux は聞かない（notNeeded）。Windows のプライバシー設定で拒否されている時は、入力が開けないか無音になる
    （InputMeter の「完全な無音」で知らせる）。
    Mac では MicPermission.mm 経由で Objective-C++ としてコンパイルする（CMakeLists.txt）。 */

namespace vb::audio
{
/** いまの許可の状態（すぐ返る。ダイアログは出さない） */
MicPermission checkMicPermission();

/** 許可を求める（Mac：未回答ならダイアログを出す）。答えはメッセージスレッドで返る */
void requestMicPermission (std::function<void (bool granted)>);
} // namespace vb::audio
