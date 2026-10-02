#pragma once

#include "UiSession.h"
#include "Actions.h"

/*  区間の名前と、区間・ルーラーのメニュー（B4b。DESIGN 7.5.2）
    ルーラー・曲の情報パネルの区間の一覧で同じものを使う */

namespace vb::marks
{
/** 種類の名前（画面の言葉）。custom / 知らない種類は空 */
juce::String kindName (const juce::String& kindId);

/** 表示名：種類の名前か自由入力の名前、同じ名前が複数あれば番号（サビ 1、サビ 2）、generic は「区間 1」 */
juce::String sectionName (const song::Sections&, int index);

/** 位置の表示：テンポが分かっていれば「小節.拍」、分からなければ m:ss.mmm */
juce::String positionText (const dummy::Session&, int64 sample);

/** 区間のメニュー（名前を選ぶ / 名前を入力… / サビへ / この区間をループ / 消す）。area は画面座標 */
void showSectionMenu (UiSession&, Actions&, int index, juce::Rectangle<int> screenArea);

/** 名前だけのメニュー（一覧から選ぶ / 名前を入力…）。area は画面座標 */
void showNameMenu (UiSession&, Actions&, int index, juce::Rectangle<int> screenArea);

/** ルーラーを右クリック：「ここから○○」と「ここを 1 小節目の頭に」。snap は Alt / Option を押していない時 */
void showRulerMenu (UiSession&, Actions&, int64 sample, bool snap, juce::Rectangle<int> screenArea);
} // namespace vb::marks
