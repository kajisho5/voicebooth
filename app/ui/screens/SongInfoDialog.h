#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../Actions.h"
#include "../parts/Dropdown.h"

/*  曲の情報（B4b。DESIGN 7.5.1 / 7.5.2）：テンポ・拍子・1 小節目・キーと区間の一覧
    画面の右に出す（背景を暗くしない。ルーラー・BAR.BEAT がその場で変わるのを見ながら直せる）
    パネルを開いたままでも Space で再生 / 一時停止、T でタップテンポ、M で区間の頭を打てる */

namespace vb
{
/** 区間の一覧の 1 行：推定 / 確定の LED、位置、名前（押すと一覧）、ループ、消す */
class SectionRow : public juce::Component
{
public:
    SectionRow (UiSession&, Actions&, int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    static constexpr int height = 32;

private:
    UiSession& session;
    Actions& actions;
    int index;
    KeyButton nameKey { {}, KeyButton::Kind::ghost };
    KeyButton loopKey, removeKey;
    juce::Rectangle<int> posArea;
};

class SongInfoDialog : public DialogPanel,
                       private SessionView,
                       private juce::AsyncUpdater
{
public:
    SongInfoDialog (UiSession&, Actions&);
    ~SongInfoDialog() override;

    bool keyPressed (const juce::KeyPress&) override;

    /** タップテンポ（T と TAP キー） */
    void tap();

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;
    void handleAsyncUpdate() override;   // 区間の一覧を作り直す（押したキーの中で消さないよう次のメッセージで）
    void refresh();
    void commitBpm();
    void styleField (juce::TextEditor&);

    Actions& actions;

    // テンポ
    juce::TextEditor bpmField;
    KeyButton doubleKey, halfKey, tapKey;
    KeyButton clearTempoKey { {}, KeyButton::Kind::ghost };
    SegmentedKeys signature;
    KeyButton downbeatKey, earlierKey, laterKey;

    // キー
    Dropdown tonic;
    SegmentedKeys keyMode;

    // 区間
    KeyButton addSectionKey;
    juce::Viewport sectionView;
    juce::Component sectionList;
    juce::OwnedArray<SectionRow> rows;

    juce::Rectangle<int> tempoHeader, bpmLabel, tapHint, sigLabel, downbeatLabel, downbeatValue,
                         keyHeader, keyLabel, sectionHeader, emptyArea, hintArea;
};

/** 区間の名前を自由に入れる（一覧に無い名前。DESIGN 7.5.2） */
class SectionNameDialog : public DialogPanel
{
public:
    SectionNameDialog (UiSession&, int index);

    std::function<void()> onDone;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void commit();

    UiSession& session;
    int index;
    juce::TextEditor field;
};
} // namespace vb
