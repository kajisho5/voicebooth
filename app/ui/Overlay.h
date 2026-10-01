#pragma once

#include "parts/KeyButton.h"

/*  画面の上に重ねるダイアログ（入力セットアップ / 書き出し / 設定 / 確認）
    別ウィンドウにせず同じ画面内に出す（視線を切らない・見た目を揃える） */

namespace vb
{
/** ダイアログの外枠：見出し / 本文 / 下部のキー */
class DialogPanel : public juce::Component
{
public:
    DialogPanel (const juce::String& title, const juce::String& microTitle);

    enum class KeyRole { normal, primary, danger };

    /** 下部のキー（右から並ぶ）。primary は点灯色 */
    KeyButton& addFooterKey (const juce::String& text, KeyRole, std::function<void()> onClick);

    /** 右上の閉じるキー */
    std::function<void()> onCloseRequest;

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    /** 本文の領域（派生クラスはここに配置する） */
    juce::Rectangle<int> body() const;
    virtual void layoutBody (juce::Rectangle<int>) {}
    virtual void paintBody (juce::Graphics&, juce::Rectangle<int>) {}

    static constexpr int headerH = 58, footerH = 60, padding = 22;

private:
    juce::String title, microTitle;
    KeyButton closeKey;
    juce::OwnedArray<KeyButton> footerKeys;
    std::vector<KeyRole> roles;
};

/** はい / いいえ 等の確認 */
class ConfirmDialog : public DialogPanel
{
public:
    struct Option { juce::String text; KeyRole role; std::function<void()> action; };

    ConfirmDialog (const juce::String& title, const juce::String& message, std::vector<Option>);

protected:
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    juce::String message;
};

/** 重ね表示の土台。背景を暗くし、中央にパネルを置く */
class OverlayHost : public juce::Component
{
public:
    OverlayHost();

    /** パネルは自分の大きさを setSize しておく。dismissible なら背景クリックで閉じる */
    void show (std::unique_ptr<juce::Component> panel, bool dismissible = true);
    void close();
    bool isShowing() const { return content != nullptr; }
    bool isDismissible() const { return dismissible; }

    std::function<void()> onClosed;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    std::unique_ptr<juce::Component> content;
    bool dismissible = true;
};
} // namespace vb
