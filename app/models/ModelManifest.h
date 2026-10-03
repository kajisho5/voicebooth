#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/*  モデルの一覧（manifest）と署名（DESIGN 11.7 / B16）
    置き場所の models/manifest.json と、その Ed25519 の署名 models/manifest.json.sig（64 バイトを base64 で 1 行）。
    アプリに入れた公開鍵で署名を確かめてから中身を信じる（HTTPS だけに頼らない。R2 / ドメインを乗っ取られても偽物を配らせない）。
    秘密鍵はリポジトリ・CI に置かない（持ち主の手元だけ。tools/models/）。

    manifest の形（format "voicebooth.models"、format_version 1）：
      { "format": "voicebooth.models", "format_version": 1, "serial": 3,
        "models": [ { "id": "bs-roformer-anvuew-ft1-int8-1", "role": "separation", "title": "...", "license": "GPL-3.0", "license_url": "...",
                      "files": [ { "name": "front.onnx", "url": "https://.../front.onnx", "size": 4210288, "sha256": "<64 桁>",
                                   "block_size": 8388608, "blocks": [ "<64 桁>", ... ] } ] } ] }
    blocks は block_size ごとの SHA-256（最後は半端）。壊れた所だけ取り直すために使う */

namespace vb::models
{
struct ModelFile
{
    juce::String name, url, sha256;
    juce::int64 size = 0;
    juce::int64 blockSize = 8 * 1024 * 1024;
    juce::StringArray blocks;                 // ブロックごとの SHA-256（小文字の 16 進）
};

struct ModelEntry
{
    juce::String id, role, title, license, licenseUrl;
    std::vector<ModelFile> files;
    juce::int64 totalSize() const;
};

struct Manifest
{
    int serial = 0;                           // 一覧の版（大きいほど新しい）
    std::vector<ModelEntry> models;
    const ModelEntry* find (const juce::String& id) const;
};

using PublicKey = std::array<juce::uint8, 32>;

/** 署名（Ed25519、64 バイトを base64）が manifest の中身に対して正しいか */
bool verifySignature (const juce::MemoryBlock& manifestBytes, const juce::String& signatureBase64, const PublicKey&);

/** 読めなければ error に理由（英語の短い文） */
bool parseManifest (const juce::String& json, Manifest& out, juce::String& error);

/** アプリに入れた公開鍵（持ち主が鍵を作ったら入れる。空 = まだ配布していない） */
std::vector<PublicKey> trustedKeys();

/** 一覧の置き場所（環境変数 VB_MODEL_MANIFEST_URL で差し替えられる。開発用） */
juce::String manifestUrl();

/** 16 進（64 桁）→ 32 バイト。形が違えば false */
bool parseHexKey (const juce::String& hex, PublicKey& out);

/** SHA-256 を小文字の 16 進で */
juce::String sha256Hex (const void* data, size_t size);
juce::String sha256Hex (const juce::File&);
} // namespace vb::models
