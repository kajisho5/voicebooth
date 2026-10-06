#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <map>
#include <vector>

/*  スキン（DESIGN 4.11）。色トークン 16 個だけを差し替える。書体・寸法・配置・動きは変えない
    画面に依存しない（juce_core だけ）。色は ARGB の uint32 で持つ（アルファは常に FF）

    .vbskin（UTF-8 の JSON）
      { "format": "voicebooth-skin", "version": 1, "id": "my-skin", "name": "…", "author": "…",
        "base": "booth", "colours": { "bg0": "#141311", … } }
      - 書いていない色・読めない値は base（内蔵スキン。知らない base は booth）の色
      - 知らないキーは無視。1 ファイル 64 KB まで、文字列は 64 文字まで（超えた分は切る） */

namespace vb::skin
{
/** 色トークン（DESIGN 4.9 の並び） */
enum class Token
{
    bg0, bgDeep, panel, raised, raisedHi, grid, line, lineHi,
    text, textDim, textMute,
    signal, ref, warn, bad, rec
};

constexpr int numTokens = 16;

/** JSON のキー（"bg0" "bgDeep" …） */
const char* tokenKey (Token);
inline const char* tokenKey (int index) { return tokenKey ((Token) index); }

/** エディタでの並び（背景 / 線 / 文字 / 意味の色） */
enum class Group { background, lines, text, meaning };
Group groupOf (Token);

using Colours = std::array<juce::uint32, numTokens>;

struct Skin
{
    juce::String id;          // 小文字英数字と '-'（ファイル名にも使う）
    juce::String name;
    juce::String author;
    juce::String base = "booth";
    Colours colours {};
    bool builtIn = false;

    juce::uint32 get (Token t) const      { return colours[(size_t) t]; }
    void set (Token t, juce::uint32 argb) { colours[(size_t) t] = argb | 0xff000000u; }
};

/** 内蔵スキン 10 種（DESIGN 4.11 の表の順。先頭が既定の booth） */
const std::vector<Skin>& builtInSkins();
const Skin* findBuiltIn (const juce::String& id);
const Skin& defaultSkin();

/** 内蔵スキンの副題の翻訳キー（"skin.booth.subtitle" など）。内蔵でなければ空 */
juce::String subtitleKey (const juce::String& id);

//==============================================================================
// ファイル（.vbskin）
constexpr const char* fileExtension = ".vbskin";
constexpr const char* formatName = "voicebooth-skin";
constexpr int formatVersion = 1;
constexpr juce::int64 maxFileBytes = 64 * 1024;
constexpr int maxStringLength = 64;

/** 読めなかった理由（画面で訳す） */
enum class ReadError { none, tooLarge, notJson, notSkin, newerVersion, unreadable };

struct ReadResult
{
    ReadError error = ReadError::none;
    Skin skin;
    bool ok() const { return error == ReadError::none; }
};

/** JSON の文字列から。サイズも見る（UTF-8 のバイト数） */
ReadResult parse (const juce::String& json);
ReadResult readFile (const juce::File&);

/** 16 色すべてを書く（base が後で変わっても見た目が変わらないように） */
juce::String toJson (const Skin&);
bool writeFile (const Skin&, const juce::File&);

/** "#RRGGBB"（大文字小文字どちらでも）→ ARGB。読めなければ false */
bool parseHex (const juce::String&, juce::uint32& argbOut);
juce::String toHex (juce::uint32 argb);   // "#RRGGBB"（大文字）

/** id に使える形（小文字英数字と '-'、64 文字まで）。空になったら "skin" */
juce::String sanitiseId (const juce::String&);

//==============================================================================
// 見やすさの点検（保存は止めない。DESIGN 4.11）
double contrastRatio (juce::uint32 a, juce::uint32 b);   // WCAG 2.x（1〜21）
double deltaE76 (juce::uint32 a, juce::uint32 b);        // CIE76（sRGB / D65 → L*a*b*）
double hueDegrees (juce::uint32);                          // HSV の色相（0〜360）
double saturation (juce::uint32);                          // HSV の彩度（0〜1）

struct Limits
{
    double text = 4.5, textDim = 4.5, textMute = 4.5, lines = 3.0, deltaE = 25.0;   // 小さい字は 4.5:1（WCAG 1.4.3。#28）
    double recHueFrom = 330.0, recHueTo = 20.0, recMinSaturation = 0.35;
};

struct Warning
{
    enum class Kind { contrast, deltaE, recHue };
    Kind kind;
    Token a, b;        // recHue は a = b = rec
    double value, required;
};

std::vector<Warning> check (const Skin&, const Limits& = {});

//==============================================================================
/** 自作スキンの置き場（アプリのデータフォルダの Skins/）。メッセージスレッドから使う */
class Library
{
public:
    explicit Library (juce::File folder);

    /** フォルダを読み直す（壊れたファイルは飛ばす） */
    void reload();

    const juce::File& folder() const { return dir; }
    const std::vector<Skin>& userSkins() const { return user; }

    /** 内蔵 → 自作の順 */
    std::vector<Skin> all() const;

    /** id で探す（内蔵も）。無ければ nullptr */
    const Skin* find (const juce::String& id) const;

    /** 内蔵・自作のどれとも重ならない id（wanted を元に -2, -3 …） */
    juce::String uniqueId (const juce::String& wanted, const juce::String& except = {}) const;

    /** 保存（同じ id は上書き）。内蔵の id には保存しない */
    bool save (const Skin&);

    /** 自作スキンを消す。内蔵は消せない */
    bool remove (const juce::String& id);

    juce::File fileFor (const juce::String& id) const;

private:
    juce::File dir;
    std::vector<Skin> user;
    std::map<juce::String, juce::File> files;   // 読んだ id → 実際のファイル（手で置いた「My Skin.vbskin」は id が my-skin になる）
};
} // namespace vb::skin
