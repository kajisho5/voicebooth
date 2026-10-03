#pragma once

#include "UpdateCheck.h"

/*  アプリ内の更新（DESIGN 11.7）：取って照合したインストーラーで入れ替える。
    使う人が［今すぐ更新］を押した時だけ。録音・書き出し中はしない（UiSession が確かめる）。
    - Windows：Inno Setup のインストーラーを画面なし（/VERYSILENT）で動かす。ユーザーごとの入れ方なので管理者の確認は出ない。
      このアプリは閉じる。入れ終わったらインストーラーが起動し直す（VoiceBooth.iss の /relaunch=1）
    - Mac：このアプリが終わるのを待つ小さなスクリプト（sh）を起こす。DMG を開いて VoiceBooth.app を入れ替え、起動し直す。
      入れ替えに失敗したら元に戻し、DMG を Finder で開く（手で入れられる）
    アプリ自身が取ったファイルにはブラウザの「ネットから来た」印が付かないので、署名なしでも警告は出ない見込み（実機で確かめる） */

namespace vb::update
{
/** この OS・置き場所でアプリ内の入れ替えができるか。できなければ why に理由（英語の短い文） */
bool canInstallInPlace (juce::String* why = nullptr);

/** 入れ替えを始める（別プロセスを起こす）。成功したら、呼んだ側はすぐアプリを終えること */
bool launchInstaller (const juce::File& installer, juce::String& error);

/** Windows のインストーラーに渡す引数（テストする） */
juce::String windowsInstallerArguments();

/** Mac で入れ替えるスクリプト（sh。引数：待つ PID・DMG・入れ替える .app。テストする） */
juce::String macSwapScript();

/** Mac：入れ替えてよい .app の場所か（書ける・DMG の中や一時的な隔離場所でない） */
bool macBundleReplaceable (const juce::File& bundle, juce::String* why = nullptr);
} // namespace vb::update
