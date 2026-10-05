#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/*  困ったときのヘルプ（2026-10-04。持ち主の案）：よくある困りごとと直し方。上のバーの ? と F1 で開く。
    いまの状態に関係する項目（出力が開いていない・入力がない・分離中・モデルがない など）を先に並べ、印を付けて最初の 1 つを開いておく。
    項目によっては、関係する画面へ移るキーを添える */

namespace vb
{
class UiSession;

namespace help
{
enum class Topic
{
    noSound,          // 再生しても音が聞こえない
    noMic,            // マイクの声が入らない（メーターが動かない）
    monitorLate,      // 自分の声が遅れて聞こえる
    recordOffset,     // 録った声が伴奏とずれる
    recordNotStarting,// REC を押しても録音が始まらない
    noGuideLine,      // お手本の線が表示されない
    guideOffset,      // お手本の線がずれている
    separationSlow,   // 分離に時間がかかる
    noHarmonyLine,    // ハモリのお手本の線が表示されない
    modelDownload,    // 分離モデルのダウンロード
    dropouts,         // 音が途切れる・プツプツする
    exportWhere,      // 書き出したファイルの場所
    otherProblem      // そのほか（Issues へ）
};
constexpr int numTopics = 13;

/** 項目から移れる先 */
enum class Action { none, openSetup, openLatency, downloadModels, openIssues };

const char* titleKey (Topic);
const char* bodyKey (Topic);
Action action (Topic);
/** 移る先のキーの文字の翻訳キー（Action::none なら nullptr） */
const char* actionKey (Action);

struct Item
{
    Topic topic;
    bool relevant;   // いまの状態に関係している（先に並べて印を付ける）
};

/** 並べる順：いまの状態に関係するもの → そのほか（決まった順）。全部の項目を 1 回ずつ */
std::vector<Item> items (const UiSession&);
} // namespace help
} // namespace vb
