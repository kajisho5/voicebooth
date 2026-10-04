#pragma once

#include "../Overlay.h"
#include "../UiSession.h"
#include "../parts/LedMeter.h"
#include "../parts/Dropdown.h"

namespace vb
{
class DeviceListView;

/** DESIGN 5 入力セットアップ（デバイス → レベル → レイテンシ）
    エンジンがあれば（B3）実デバイスの一覧・入力メーター・申告レイテンシを出し、選ぶと切り替わる。
    UI_MOCK では静的モックのまま。レイテンシ（B6）：測定音で往復の遅れを測る・ms を手入力する。値は機器の組み合わせごとに保存 */
class SetupWizard : public DialogPanel, private SessionView
{
public:
    SetupWizard (UiSession&, int step);
    ~SetupWizard() override;

protected:
    void layoutBody (juce::Rectangle<int>) override;
    void paintBody (juce::Graphics&, juce::Rectangle<int>) override;

private:
    void onSessionChanged (juce::uint32) override;
    void setStep (int);
    void paintStepper (juce::Graphics&, juce::Rectangle<int>);
    void paintDevice (juce::Graphics&, juce::Rectangle<int>);
    void paintDeviceLive (juce::Graphics&, juce::Rectangle<int>);
    void paintLevel (juce::Graphics&, juce::Rectangle<int>);
    void paintLatency (juce::Graphics&, juce::Rectangle<int>);

    bool live() const { return state().engineAttached; }

    /** 実デバイスの一覧から部品を作り直す（一覧が変わった時だけ） */
    void rebuildDeviceControls();
    void updateNextKey();

    /** 部品のコールバックの外で実行する（切り替えで部品を作り直すため） */
    void later (std::function<void (SetupWizard&)>);
    void selected (const juce::String& error);

    int step = 0;
    LedMeter meter;
    KeyButton measure;
    juce::TextEditor manualField;               // 手入力（ms）
    KeyButton manualUse, manualClear;
    void commitManual();
    bool manualEdited() const;   // 手入力の欄が保存した値と違う（入れたまま使っていない）
    void refreshLatencyControls();
    KeyButton* back = nullptr;
    KeyButton* next = nullptr;
    juce::Rectangle<int> meterArea, measureArea;

    // 実デバイス（B3）
    audio::DeviceList devices;
    juce::String devicesKey;                    // 一覧の中身（変わった時だけ作り直す）
    std::unique_ptr<DeviceListView> inputList, outputList;
    std::unique_ptr<Dropdown> driverPick, bufferPick, channelPick, ratePick;   // ratePick：録音の SR
    std::unique_ptr<SegmentedKeys> bitKeys;                                      // 24bit / 32bit float
    juce::Array<double> rateChoices;                                             // ratePick の並び（0 = 曲に合わせる）
    std::unique_ptr<SegmentedKeys> channelKeys;
    juce::String selectError;                   // 最後の切り替えの失敗（OS / ドライバの文言）
};
} // namespace vb
