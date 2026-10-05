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
    otherProblem      // そのほか（不具合の報告へ）
};
constexpr int numTopics = 13;

/** 項目から移れる先 */
enum class Action { none, openSetup, openLatency, downloadModels, report };

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

//==============================================================================
/*  ヘルプのメニュー（2026-10-05。持ち主の案「普通のソフトについているようなもの」）と不具合の報告。
    報告は GitHub の Issues に、題と本文（見出し＋アプリと機器の情報）を入れた状態でブラウザを開く。送るのは使う人が自分で行う */

constexpr const char* issuesUrl   = "https://github.com/kajisho5/voicebooth/issues";
constexpr const char* releasesUrl = "https://github.com/kajisho5/voicebooth/releases";

/** 使い方（いまの言語の README。日本語は README.md） */
juce::String guideUrl();

/** アプリと機器の情報（ラベルは英語で固定。翻訳しない：受け取る側が読めるように）。
    曲・プロジェクトのファイル名やパスは入れない（市販の曲名・個人のフォルダ名が入るため） */
juce::String environmentReport (const UiSession&);

/** Issue の本文：見出し（いまの言語）と environmentReport */
juce::String issueBody (const UiSession&);

/** 新しい Issue を題と本文を入れた状態で開く URL */
juce::String newIssueUrl (const juce::String& title, const juce::String& body);

/** 報告に開く URL。長すぎると開けないことがある（Windows のブラウザの渡し方など）ので、
    maxUrlLength を超えるときは情報を本文から外し、「貼り付けてください」と書く（情報はクリップボードにも入れる） */
constexpr int maxUrlLength = 2000;
juce::String reportUrl (const UiSession&);
juce::String reportUrl (const juce::String& environment);   // 情報を渡す形（テスト用）
} // namespace help
} // namespace vb
