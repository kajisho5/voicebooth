#pragma once

#include <functional>

/*  画面をまたぐ操作の窓口。確認ダイアログが要る操作は MainComponent が判断する。
    部品は「何をしたいか」だけ伝え、ダイアログの出し方は知らない。 */

namespace vb
{
struct Actions
{
    std::function<void()> toggleRecord;           // 納品 REC 前のテンポ/キー確認を含む
    std::function<void (int)> requestTempo;       // 納品 REC 中なら確認
    std::function<void (int)> requestKey;

    std::function<void()> openStart;
    std::function<void()> openSetup;
    std::function<void()> openExport;
    std::function<void()> openSettings;
    std::function<void()> openVoiceRange;         // 声域を測る（おすすめのキー）

    // 曲の情報（B4b。DESIGN 7.5）
    std::function<void()> openSongInfo;           // テンポ・拍子・キー・区間のパネル
    std::function<void()> openLyrics;             // 歌詞パッド（読み込み・貼り付け）
    std::function<void (int)> editSectionName;    // 区間の名前を入力する（番号）
};
} // namespace vb
