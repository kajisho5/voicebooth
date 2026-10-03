#include "ModelManifest.h"
#include <juce_cryptography/juce_cryptography.h>

extern "C"
{
#include <optional/monocypher-ed25519.h>
}

namespace vb::models
{
juce::int64 ModelEntry::totalSize() const
{
    juce::int64 n = 0;
    for (auto& f : files) n += f.size;
    return n;
}

const ModelEntry* Manifest::find (const juce::String& id) const
{
    for (auto& m : models)
        if (m.id == id) return &m;
    return nullptr;
}

bool verifySignature (const juce::MemoryBlock& bytes, const juce::String& signatureBase64, const PublicKey& key)
{
    juce::MemoryOutputStream sig;
    if (! juce::Base64::convertFromBase64 (sig, signatureBase64.trim()) || sig.getDataSize() != 64)
        return false;
    return crypto_ed25519_check (static_cast<const uint8_t*> (sig.getData()), key.data(),
                                 static_cast<const uint8_t*> (bytes.getData()), bytes.getSize()) == 0;
}

bool parseHexKey (const juce::String& hex, PublicKey& out)
{
    const auto h = hex.trim().toLowerCase();
    if (h.length() != 64 || ! h.containsOnly ("0123456789abcdef"))
        return false;
    for (int i = 0; i < 32; ++i)
        out[(size_t) i] = (juce::uint8) h.substring (i * 2, i * 2 + 2).getHexValue32();
    return true;
}

namespace
{
    bool isHash (const juce::String& s) { return s.length() == 64 && s.toLowerCase().containsOnly ("0123456789abcdef"); }
}

bool parseManifest (const juce::String& json, Manifest& out, juce::String& error)
{
    out = {};
    const auto root = juce::JSON::parse (json);
    if (! root.isObject() || root.getProperty ("format", {}).toString() != "voicebooth.models")
    {
        error = "not a model list";
        return false;
    }
    if ((int) root.getProperty ("format_version", 0) > 1)
    {
        error = "newer model list";
        return false;
    }
    out.serial = (int) root.getProperty ("serial", 0);
    if (const auto* models = root.getProperty ("models", {}).getArray())
        for (auto& m : *models)
        {
            ModelEntry e;
            e.id = m.getProperty ("id", {}).toString();
            e.role = m.getProperty ("role", {}).toString();
            e.title = m.getProperty ("title", {}).toString();
            e.license = m.getProperty ("license", {}).toString();
            e.licenseUrl = m.getProperty ("license_url", {}).toString();
            if (const auto* files = m.getProperty ("files", {}).getArray())
                for (auto& f : *files)
                {
                    ModelFile mf;
                    mf.name = f.getProperty ("name", {}).toString();
                    mf.url = f.getProperty ("url", {}).toString();
                    mf.size = (juce::int64) f.getProperty ("size", 0);
                    mf.sha256 = f.getProperty ("sha256", {}).toString().toLowerCase();
                    mf.blockSize = (juce::int64) f.getProperty ("block_size", (juce::int64) 8 * 1024 * 1024);
                    if (const auto* blocks = f.getProperty ("blocks", {}).getArray())
                        for (auto& b : *blocks) mf.blocks.add (b.toString().toLowerCase());

                    // ファイル名は 1 段だけ（../ や / で外へ書かせない）・URL は https だけ・ハッシュは 64 桁・ブロックの数が合う
                    const auto expectedBlocks = mf.blockSize > 0 ? (mf.size + mf.blockSize - 1) / mf.blockSize : 0;
                    // http は開発用の手元のサーバーだけ（VB_MODEL_ALLOW_HTTP=1）
                    const bool allowHttp = juce::SystemStats::getEnvironmentVariable ("VB_MODEL_ALLOW_HTTP", {}) == "1";
                    const bool urlOk = mf.url.startsWithIgnoreCase ("https://") || (allowHttp && mf.url.startsWithIgnoreCase ("http://"));
                    if (mf.name.isEmpty() || mf.name.containsAnyOf ("/\\:") || mf.name.startsWith (".")
                        || ! urlOk || mf.size <= 0 || ! isHash (mf.sha256)
                        || mf.blockSize < 65536 || mf.blocks.size() != (int) expectedBlocks)
                    {
                        error = "bad file entry in " + e.id;
                        return false;
                    }
                    for (auto& b : mf.blocks)
                        if (! isHash (b)) { error = "bad block hash in " + e.id; return false; }
                    e.files.push_back (mf);
                }
            if (e.id.isEmpty() || e.id.containsAnyOf ("/\\:") || e.files.empty())
            {
                error = "bad model entry";
                return false;
            }
            out.models.push_back (e);
        }
    return true;
}

std::vector<PublicKey> trustedKeys()
{
    // 持ち主が鍵を作ったら、公開鍵（64 桁の 16 進）をここに足す（tools/models/README.md）。秘密鍵は入れない。
    // 開発用：環境変数 VB_MODEL_PUBKEY（64 桁の 16 進）
    std::vector<PublicKey> keys;
    static const char* const builtIn[] = {
        "25881bbba9aaf8e45aaac8eb610d7c651548453fa9c5b4b54b3cf8465e0711ac",   // models-ed25519（2026-10-03 作成。秘密鍵は持ち主が保管）
    };
    for (auto* hex : builtIn)
    {
        PublicKey k;
        if (parseHexKey (hex, k)) keys.push_back (k);
    }
    PublicKey dev;
    if (parseHexKey (juce::SystemStats::getEnvironmentVariable ("VB_MODEL_PUBKEY", {}), dev))
        keys.push_back (dev);
    return keys;
}

juce::String manifestUrl()
{
    const auto env = juce::SystemStats::getEnvironmentVariable ("VB_MODEL_MANIFEST_URL", {});
    return env.isNotEmpty() ? env : juce::String ("https://voicebooth-dl.sw-ars.com/models/manifest.json");
}

juce::String sha256Hex (const void* data, size_t size)
{
    return juce::SHA256 (data, size).toHexString().toLowerCase();
}

juce::String sha256Hex (const juce::File& f)
{
    return juce::SHA256 (f).toHexString().toLowerCase();
}
} // namespace vb::models
