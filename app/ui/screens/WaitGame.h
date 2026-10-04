#pragma once

#include "../UiSession.h"
#include "../parts/KeyButton.h"

namespace vb
{
/** 待ち時間のゲーム（2026-10-04）。起動画面で原曲から分離している間（数分〜十数分）に遊べる
    - 閉じている間：［音程あて］［リズムタップ］［おまかせ］を 1 行に並べる。遊ぶかは自由（押した時だけ開く）
    - 音程あて：表示された音を声で当てる（どのオクターブでもよい）。合わせ続けると次の音へ。［音を聴く］で目標の音を鳴らす
      声域を測ってあれば、その中で選ぶ。マイクの音程は曲を開く前でも取る（UiSession::startGameVoice）
    - リズムタップ：一定のテンポのクリックに合わせて Space（または枠のクリック）。最初の 4 拍は聴くだけ、16 回で結果
      ずれは「耳に届いているクリックの位置」から測る（エンジンの時計・出力の遅延を引く）。音が出せなければ LED だけで遊ぶ
    - おまかせ：どちらかをランダムに。マイクが無ければリズムタップ
    - 分離を遅くしないよう、描き直すのはこの部品だけ（30 Hz）。分離が終わったら呼ぶ側が close() する */
class WaitGame : public juce::Component,
                 private juce::Timer
{
public:
    explicit WaitGame (UiSession&);
    ~WaitGame() override;

    bool isOpen() const { return kind != Kind::none; }
    /** 遊んでいるゲームを閉じて、音・マイクの取り込みを止める */
    void close();
    /** 開いた・閉じた（呼ぶ側が並べ直す） */
    std::function<void()> onOpenChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** 1 回分の結果（リズムタップ：ずれの ms の並び → 平均・ばらつき）。テスト用に外から呼べる */
    struct TapStats { double mean = 0.0, spread = 0.0; };
    static TapStats tapStats (const std::vector<double>& offsetsMs);
    /** 2 つの音の高さの差（セント）。オクターブ違いは同じ音として -600〜+600 に畳む */
    static double pitchClassCents (float sungMidi, float targetMidi);

private:
    enum class Kind { none, pitch, rhythm };

    void open (Kind);
    void timerCallback() override;
    const dummy::Session& state() const { return session.get(); }

    // 音程あて
    void nextTarget();
    void playTarget();
    void tickPitch (double dt);
    void paintPitch (juce::Graphics&, juce::Rectangle<float>);

    // リズムタップ
    void startRhythm();
    double rhythmClock() const;   // クリックの 0 拍目から（秒）
    void tap();
    void paintRhythm (juce::Graphics&, juce::Rectangle<float>);

    UiSession& session;
    Kind kind = Kind::none;
    KeyButton pitchKey, rhythmKey, randomKey, closeKey, listenKey, againKey;
    juce::Random rng;
    juce::Rectangle<int> body;
    double lastTickMs = 0.0;

    // 音程あて
    float target = 0.0f;
    int hits = 0;
    double hold = 0.0;                // 合っている間の秒（離れるとゆっくり戻る）
    double ignoreVoiceUntilMs = 0.0;  // 目標の音を鳴らしている間は自分の声として数えない
    double hitFlashMs = 0.0;
    float lastCents = 0.0f;
    bool hasVoice = false;

    // リズムタップ
    double bpm = 100.0;
    bool audioClock = false;          // エンジンの時計で測る（音が出る）。false なら画面の時計と LED だけ
    double visualStartMs = 0.0;
    std::vector<double> offsets;      // ms（+ は遅い）
    double lastOffset = 0.0;
    double lastTapMs = 0.0;

    static constexpr int countInBeats = 4, tapsPerRound = 16;
    static constexpr double holdToHit = 0.7, hitCents = 40.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaitGame)
};
} // namespace vb
