#pragma once

#include "../Overlay.h"
#include "../parts/Dropdown.h"
#include "../parts/ColourPad.h"

namespace vb
{
/** DESIGN 4.11 スキンエディタ。設定の「スキン」→「編集」、または「新しく作る」でテンプレートを選んで開く
    - 16 色をグループ（背景 / 線 / 文字 / 意味の色）で並べ、色見本を押して選ぶ（色の面＋HEX）
    - グループごとに「ほかのテンプレートから借りる」「テンプレートに戻す」
    - 変えた色はその場で後ろの画面に反映（onPreview）。キャンセル・閉じるで元に戻す
    - 内蔵スキンは書き換えず、コピーを作って編集する
    - 見やすさの点検は保存を止めない（アンバーで知らせる）
    - 書き出し / 読み込みは .vbskin（ファイル選択は非同期） */
class SkinEditor : public DialogPanel
{
public:
    /** active：いま画面に出ているスキン（キャンセルで戻す）
        source：編集するスキン。copy なら source をテンプレートにして新しいスキンを作る（内蔵は必ずコピー） */
    SkinEditor (skin::Library&, const skin::Skin& active, const skin::Skin& source, bool copy);
    ~SkinEditor() override;

    /** 色を画面に反映する（保存しない） */
    std::function<void (const skin::Skin&)> onPreview;
    /** 保存した（このスキンを選んで画面を作り直す） */
    std::function<void (const juce::String& id)> onSaved;
    /** 自作スキンを消した */
    std::function<void (const juce::String& id)> onDeleted;

    /** 最初の色を画面に出す（onPreview を付けてから呼ぶ） */
    void applyPreview() { preview(); }

    /** 色見本を選ぶ（スクリーンショットにも使う） */
    void selectToken (skin::Token);

    /** グループ（0 背景 / 1 線 / 2 文字 / 3 意味の色）の「テンプレート」メニューを開く */
    void showGroupMenu (int group);

    /** グループの色だけを from から写す（「ほかのテンプレートから借りる」「テンプレートに戻す」） */
    void copyGroup (int group, const skin::Skin& from);

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void setTokenColour (skin::Token, juce::uint32 argb, bool fromPicker, bool fromHex);
    void setTemplate (int index);
    void loadWorking (const skin::Skin&, bool existing);
    void coloursChanged();
    void preview();
    void refreshFields();
    void styleField (juce::TextEditor&);
    void setStatus (const juce::String&, juce::Colour);

    void save();
    void exportFile();
    void importFile();
    void deletePressed();

    int swatchAt (juce::Point<int>) const;
    int templateIndex (const juce::String& id) const;
    juce::String warningText (const skin::Warning&) const;

    skin::Library& library;
    std::vector<skin::Skin> templates;   // 内蔵＋自作
    skin::Skin original;                 // 開いた時の色（キャンセルで戻す）
    skin::Skin working;
    skin::Skin templateSkin;             // 「テンプレートに戻す」の戻り先
    bool editingExisting = false;        // 自作スキンをそのまま編集（false = テンプレートのコピー・読み込んだもの）
    bool previewed = false;              // 画面の色を変えた
    bool committed = false;              // 保存・削除した（閉じても戻さない）
    bool deleteArmed = false;
    skin::Token selected = skin::Token::signal;
    int hover = -1;

    Dropdown templatePicker;
    juce::TextEditor nameField, authorField, hexField;
    ColourPad picker;
    juce::OwnedArray<KeyButton> groupKeys;
    KeyButton* deleteKey = nullptr;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<skin::Warning> warnings;
    juce::String status;
    juce::Colour statusColour;

    juce::Rectangle<int> nameLabel, authorLabel, templateLabel, noteArea, groupAreas[4], pickerLabelArea, hexLabel, checksArea;
    std::array<juce::Rectangle<int>, skin::numTokens> swatchAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkinEditor)
};
} // namespace vb
