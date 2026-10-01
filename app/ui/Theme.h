#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "i18n/I18n.h"

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
    inline const juce::Colour bg0      { 0xff141311 };   // 地
    inline const juce::Colour bgDeep   { 0xff0d0c0b };   // レーン・表示窓の内側
    inline const juce::Colour panel    { 0xff1b1a17 };   // ラック・バー
    inline const juce::Colour raised   { 0xff262420 };   // キーキャップ
    inline const juce::Colour raisedHi { 0xff302d28 };   // キーキャップ（ホバー）
    inline const juce::Colour grid     { 0xff24221e };   // 罫線
    inline const juce::Colour line     { 0xff34312b };   // 境界
    inline const juce::Colour lineHi   { 0xff4a463e };
    inline const juce::Colour text     { 0xfff2ede3 };   // 暖かい白
    inline const juce::Colour textDim  { 0xffa9a295 };
    inline const juce::Colour textMute { 0xff6f6a60 };
    inline const juce::Colour signal   { 0xffc6ee6a };   // ライム：自分ピッチ OK / 再生ヘッド / 点灯 LED / 選択
    inline const juce::Colour ref      { 0xff8cc1ee };   // アイスブルー：お手本
    inline const juce::Colour warn     { 0xfff4b942 };   // アンバー
    inline const juce::Colour bad      { 0xffff6b5e };   // コーラル
    inline const juce::Colour rec      { 0xffff3b30 };   // タリー赤（録音のみ）
}

namespace metrics
{
    constexpr int pad        = 14;    // 画面端の余白
    constexpr int gutter     = 64;    // ピッチ/波形レーン左の固定幅（時間軸を揃える）
    constexpr int rackWidth  = 312;   // 右ラック
    constexpr float keyRadius    = 4.0f;
    constexpr float windowRadius = 3.0f;
}

//==============================================================================
enum class Weight { regular, medium, semibold };

/** 本文・ラベル。日本語・英語は IBM Plex Sans JP、韓国語・中国語は OS の標準フォント */
juce::Font sans (float height, Weight = Weight::regular);

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
