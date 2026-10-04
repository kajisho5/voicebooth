#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "i18n/I18n.h"
#include "skin/Skin.h"

/*  見た目トークン v2 "Booth"（DESIGN 4.9）
    コンセプト: 夜の録音ブース。暖色グラファイトに機材の LED とタリーランプが灯る。
    色・書体・寸法はここ以外にハードコードしない。 */

namespace vb
{
using int64 = juce::int64;

/** UTF-8 のデータ（歌詞・機器名など、翻訳しない文字列）→ juce::String
    画面の文言は直書きせず tr("key") を使う（app/i18n/I18n.h） */
inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

namespace colours
{
    /** スキンで差し替わる色（DESIGN 4.11）。juce::Colour としてそのまま使える。
        値は applySkin() が書き換える。既定は Booth（DESIGN 4.9） */
    class Token
    {
    public:
        Token (int i, juce::uint32 argbValue) : index (i), value (argbValue) {}
        Token& operator= (juce::Colour c) { value = c; return *this; }

        operator juce::Colour() const { return value; }

        // よく使う juce::Colour の操作（juce::Colour は継承できないので取り次ぐ）
        juce::Colour withAlpha (float a) const              { return value.withAlpha (a); }
        juce::Colour withMultipliedAlpha (float a) const    { return value.withMultipliedAlpha (a); }
        juce::Colour brighter (float amount = 0.4f) const   { return value.brighter (amount); }
        juce::Colour darker (float amount = 0.4f) const     { return value.darker (amount); }
        juce::Colour interpolatedWith (juce::Colour other, float proportion) const { return value.interpolatedWith (other, proportion); }
        float getPerceivedBrightness() const                { return value.getPerceivedBrightness(); }
        juce::uint32 getARGB() const                        { return value.getARGB(); }

        int index;   // skin::Token の並び

    private:
        juce::Colour value;
    };

    inline Token bg0      {  0, 0xff141311 };   // 地
    inline Token bgDeep   {  1, 0xff0d0c0b };   // レーン・表示窓の内側
    inline Token panel    {  2, 0xff1b1a17 };   // ラック・バー
    inline Token raised   {  3, 0xff262420 };   // キーキャップ
    inline Token raisedHi {  4, 0xff302d28 };   // キーキャップ（ホバー）
    inline Token grid     {  5, 0xff24221e };   // 罫線
    inline Token line     {  6, 0xff34312b };   // 境界
    inline Token lineHi   {  7, 0xff4a463e };
    inline Token text     {  8, 0xfff2ede3 };   // 暖かい白
    inline Token textDim  {  9, 0xffa9a295 };
    inline Token textMute { 10, 0xff878278 };   // 小さい字にも使う。地・表示窓・パネルの上で 4.5:1 以上（#28）
    inline Token signal   { 11, 0xffc6ee6a };   // ライム：自分ピッチ OK / 再生ヘッド / 点灯 LED / 選択
    inline Token ref      { 12, 0xff8cc1ee };   // アイスブルー：お手本
    inline Token warn     { 13, 0xfff4b942 };   // アンバー
    inline Token bad      { 14, 0xffff6b5e };   // コーラル
    inline Token rec      { 15, 0xffff3b30 };   // タリー赤（録音のみ）

    /** いまのスキンでの色（index は Token::index） */
    juce::Colour current (int index);

    /** 部品が覚えておく色（LED の色など）。トークンから作ると、スキンを変えた後も今の色を返す
        （スキンエディタでその場の画面に反映するため） */
    class Tone
    {
    public:
        Tone (juce::Colour c) : colour (c) {}
        Tone (const Token& t) : colour (t), token (t.index) {}

        juce::Colour get() const { return token >= 0 ? current (token) : colour; }
        operator juce::Colour() const { return get(); }

    private:
        juce::Colour colour;
        int token = -1;
    };

    /** 明るいスキン（地が明るい）か。影・照りの強さを変える */
    bool isLight();

    /** 影（黒）と照り（白）。明るいスキンでは影を弱め、暗いスキンでは従来どおり */
    juce::Colour shadow (float alpha);
    juce::Colour highlight (float alpha);

    /** 色で塗った面（区間の札・音名・CLIP など）に載せる文字の色：text と bgDeep のうち読みやすい方 */
    juce::Colour onFill (juce::Colour fill);

    /** タリー赤（REC のキー・ランプ）に載せる文字の色：text と bgDeep のうち明るい方（赤には白い文字） */
    juce::Colour onRec();
}

/** スキンの 16 色を差し替える（DESIGN 4.11）。描き直し・LookAndFeel の色の入れ直しは呼び出し側 */
void applySkin (const skin::Skin&);
/** スキンを当てるたびに増える番号（色を画像にためている部品が、作り直すかを決めるため。スキンエディタのプレビューでも増える） */
int skinSerial();

/** いま使っている 16 色 */
skin::Colours currentSkinColours();

namespace metrics
{
    constexpr int pad        = 14;    // 画面端の余白
    constexpr int gutter     = 64;    // ピッチ/波形レーン左の固定幅（時間軸を揃える）
    constexpr int rackWidth  = 312;   // 右ラック（文字の換算が満額の時は rackExtra を足す）
    constexpr int rackExtra  = 48;
    constexpr float keyRadius    = 4.0f;
    constexpr float windowRadius = 3.0f;
}

//==============================================================================
enum class Weight { regular, medium, semibold };

/** 文字の大きさ（1920x1080 基準）。各部品は 1440x900 の頃の数値（JUCE の高さ＝字の上下の幅）で書いてあり、
    そのままだと字の実寸が 7〜8 px になる。sans / mono はこの換算を通して、小さい字ほど大きくする
    （12 以下は 1.6 倍、大きい見出しは 1.15 倍まで）。ダイアログを実寸（px）で組む時は sansExact / monoExact */
float uiTextHeight (float designHeight);

/** 換算の効き（0 = 換算しない〜1 = 1920x1080 の等倍で満額）。起動時に画面の作業領域の高さから決める。
    小さい画面や拡大 125 % 以上では OS がすでに大きく描くので、ほとんど効かせない（部品が収まらなくなるため） */
void setTextBoost (float amount);
float textBoostAmount();
float textBoostFor (int userAreaHeight);

/** 本文・ラベル。日本語・英語は IBM Plex Sans JP、韓国語・中国語は OS の標準フォント */
juce::Font sans (float height, Weight = Weight::regular);

/** 換算しない（設定画面などデジタル庁の目安で実寸を決めている所）。height は JUCE の高さ（字の上下の幅） */
juce::Font sansExact (float height, Weight = Weight::regular);
juce::Font monoExact (float height, Weight = Weight::medium, float tracking = 0.0f);

/** 指定した言語の本文フォント（UI の言語と無関係に） */
juce::Font sansIn (i18n::Language, float height, Weight = Weight::regular);

/** 言語名（"한국어" "简体中文" …）をその言語の字形で。言語名でなければ sans() */
juce::Font sansForLanguageName (const juce::String& text, float height, Weight = Weight::regular);

/** データ（歌詞・曲名）用。かなを含む文字列は UI の言語に関係なく日本語の字形で描く */
juce::Font sansFor (const juce::String& text, float height, Weight = Weight::regular);

/** 数値・時間・英字の小見出し（IBM Plex Mono。日本語は入れない） */
juce::Font mono (float height, Weight = Weight::medium, float tracking = 0.0f);

juce::Typeface::Ptr sansTypeface (Weight);

float textWidth (const juce::Font&, const juce::String&);

//==============================================================================
/** 部品の描画状態。ギャラリーで状態を固定表示するのにも使う */
struct KeyState
{
    bool over = false, down = false, on = false, enabled = true;
};

namespace paint
{
    /** キーキャップ本体（押すと沈む） */
    void keycap (juce::Graphics&, juce::Rectangle<float>, const KeyState&, float radius = metrics::keyRadius);

    /** 沈んだ表示窓（LCD・メーター・レーンの枠） */
    void inset (juce::Graphics&, juce::Rectangle<float>, float radius = metrics::windowRadius);

    /** LED。消灯時もうっすら色を残す */
    void led (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, bool lit);

    /** LED（明るさ 0..1）。点く・消える途中（余韻）を描く。1 なら lit=true と同じ */
    void led (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour, float level);

    /** やわらかい光（グロー）。作っておいた画像を色で塗って重ねる（毎回ぼかさない。DESIGN 4.10.1）。
        area は光の外形（楕円に収まる）。色のアルファがそのまま強さ */
    void glow (juce::Graphics&, juce::Rectangle<float> area, juce::Colour);

    /** 角型 LED（メーターのセグメント等） */
    void ledBar (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float level /*0=消灯 1=点灯*/);

    /** ラックの見出し：英字（Mono・大文字）＋日本語 */
    void sectionHeader (juce::Graphics&, juce::Rectangle<int>, const juce::String& en, const juce::String& ja);

    /** 小さな英字ラベル（Mono・大文字・字間広め） */
    void microLabel (juce::Graphics&, juce::Rectangle<float>, const juce::String&, juce::Colour,
                     juce::Justification = juce::Justification::centredLeft);

    /** 1px の水平・垂直の細線 */
    void hline (juce::Graphics&, float y, float x0, float x1, juce::Colour = colours::line);
    void vline (juce::Graphics&, float x, float y0, float y1, juce::Colour = colours::line);
}
} // namespace vb
