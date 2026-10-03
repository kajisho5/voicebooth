#pragma once

#include <juce_core/juce_core.h>

/*  アプリ共通のキャッシュ（DESIGN 8）。消しても作り直せる物だけを置く（いまは「原曲だけで始める」で作ったオフボ：offvocal/）。
    場所は設定で変えられる。曲ごとのキャッシュ（<プロジェクト>/Cache/ の分離・歌詞用の声）はここではない（プロジェクトと一緒に動く）。

    使う人が選んだフォルダ（書類フォルダなど）をそのまま使うので、大きさを数える・空にするのは
    アプリが作るサブフォルダ（subfolders）の中だけ。ほかのファイルには触れない */

namespace vb::system
{
/** 既定の場所：アプリのデータ/VoiceBooth/Cache（Win: %APPDATA%、Mac: ~/Library/Application Support） */
juce::File defaultCacheFolder();

/** アプリがキャッシュの場所の中に作るフォルダ */
juce::StringArray cacheSubfolders();

/** キャッシュの大きさ（バイト。アプリのサブフォルダの中だけ数える） */
juce::int64 cacheSize (const juce::File& folder);

/** 空にする（アプリのサブフォルダだけを消す）。全部消せたら true */
bool clearCache (const juce::File& folder);

/** 大きさの表示（1024 単位。"0 KB" / "820 KB" / "4.2 MB" / "350 MB" / "1.2 GB"） */
juce::String formatSize (juce::int64 bytes);

/** 表示用のパス（ホームの下なら ~ にする） */
juce::String displayPath (const juce::File&);
} // namespace vb::system
