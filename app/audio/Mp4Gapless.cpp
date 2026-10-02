#include "Mp4Gapless.h"

namespace vb::audio
{
namespace
{
    using int64 = juce::int64;

    juce::uint32 be32 (const juce::uint8* p) { return ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16) | ((juce::uint32) p[2] << 8) | p[3]; }
    juce::uint64 be64 (const juce::uint8* p) { return ((juce::uint64) be32 (p) << 32) | be32 (p + 4); }

    struct Box { juce::String type; const juce::uint8* body; size_t size; };

    /** [data, data + size) の箱を順に返す（壊れていたら途中で止める） */
    std::vector<Box> children (const juce::uint8* data, size_t size)
    {
        std::vector<Box> out;
        size_t pos = 0;
        while (pos + 8 <= size)
        {
            juce::uint64 boxSize = be32 (data + pos);
            size_t header = 8;
            if (boxSize == 1)
            {
                if (pos + 16 > size) break;
                boxSize = be64 (data + pos + 8);
                header = 16;
            }
            else if (boxSize == 0)
            {
                boxSize = size - pos;
            }
            if (boxSize < header || boxSize > size - pos)
                break;

            out.push_back ({ juce::String::fromUTF8 ((const char*) data + pos + 4, 4), data + pos + header, (size_t) boxSize - header });
            pos += (size_t) boxSize;
        }
        return out;
    }

    /** 見つかった箱を指す（boxes が生きている間だけ有効。一時オブジェクトに使わない） */
    const Box* find (const std::vector<Box>& boxes, const char* type)
    {
        for (auto& b : boxes)
            if (b.type == type)
                return &b;
        return nullptr;
    }

    /** mvhd / mdhd の timescale（version 0 / 1） */
    juce::uint32 timescaleOf (const Box& b)
    {
        if (b.size < 24) return 0;
        const auto version = b.body[0];
        if (version == 1)
            return b.size >= 32 ? be32 (b.body + 20) : 0;
        return be32 (b.body + 12);
    }

    /** 音声トラックの edit list から */
    bool fromEditList (const std::vector<Box>& moov, double sampleRate, Mp4Gapless& g)
    {
        const auto* mvhd = find (moov, "mvhd");
        const auto movieScale = mvhd != nullptr ? timescaleOf (*mvhd) : 0;
        if (movieScale == 0) return false;

        for (auto& trak : moov)
        {
            if (trak.type != "trak") continue;
            const auto t = children (trak.body, trak.size);
            const auto* mdia = find (t, "mdia");
            const auto* edts = find (t, "edts");
            if (mdia == nullptr || edts == nullptr) continue;

            const auto m = children (mdia->body, mdia->size);
            const auto* hdlr = find (m, "hdlr");
            const auto* mdhd = find (m, "mdhd");
            if (hdlr == nullptr || hdlr->size < 12 || juce::String::fromUTF8 ((const char*) hdlr->body + 8, 4) != "soun" || mdhd == nullptr)
                continue;
            const auto mediaScale = timescaleOf (*mdhd);

            const auto edits = children (edts->body, edts->size);   // find が指す先を生かしておく
            const auto* elst = find (edits, "elst");
            if (elst == nullptr || elst->size < 8 || mediaScale == 0) continue;

            const auto version = elst->body[0];
            const auto count = be32 (elst->body + 4);
            const size_t entrySize = version == 1 ? 20 : 12;
            int64 mediaTime = -1, segment = 0;
            for (juce::uint32 i = 0; i < count && 8 + (i + 1) * entrySize <= elst->size; ++i)
            {
                const auto* entry = elst->body + 8 + i * entrySize;
                const auto dur = version == 1 ? (int64) be64 (entry) : (int64) be32 (entry);
                const auto mt = version == 1 ? (int64) be64 (entry + 8) : (int64) (juce::int32) be32 (entry + 4);
                if (mt < 0) continue;   // 空の編集（無音の挿入）は扱わない
                mediaTime = mt;
                segment = dur;
                break;
            }
            if (mediaTime < 0) continue;

            g.found = true;
            g.source = "elst";
            g.priming = (int64) std::llround ((double) mediaTime * sampleRate / mediaScale);
            if (segment > 0)
                g.validSamples = (int64) std::llround ((double) segment * sampleRate / movieScale);
            return g.priming > 0 || g.validSamples > 0;
        }
        return false;
    }

    /** iTunes の iTunSMPB（" 00000000 00000840 0000037C 000000000000AC44 ..."：詰め物 / 尻の詰め物 / 長さ） */
    bool fromITunSMPB (const std::vector<Box>& moov, Mp4Gapless& g)
    {
        const auto* udta = find (moov, "udta");
        if (udta == nullptr) return false;
        const auto u = children (udta->body, udta->size);
        const auto* meta = find (u, "meta");
        if (meta == nullptr || meta->size < 4) return false;
        const auto me = children (meta->body + 4, meta->size - 4);   // meta は先頭 4 バイトが version / flags
        const auto* ilst = find (me, "ilst");
        if (ilst == nullptr) return false;

        const auto items = children (ilst->body, ilst->size);
        for (auto& item : items)
        {
            if (item.type != "----") continue;
            const auto parts = children (item.body, item.size);
            const auto* name = find (parts, "name");
            const auto* data = find (parts, "data");
            if (name == nullptr || data == nullptr || name->size < 4 || data->size < 8) continue;
            if (juce::String::fromUTF8 ((const char*) name->body + 4, (int) name->size - 4) != "iTunSMPB") continue;

            const auto text = juce::String::fromUTF8 ((const char*) data->body + 8, (int) data->size - 8);
            const auto fields = juce::StringArray::fromTokens (text.trim(), " ", {});
            if (fields.size() < 4) return false;

            g.found = true;
            g.source = "iTunSMPB";
            g.priming = fields[1].getHexValue64();
            g.validSamples = fields[3].getHexValue64();
            return true;
        }
        return false;
    }
}

Mp4Gapless readMp4Gapless (juce::InputStream& in, double sampleRate)
{
    Mp4Gapless g;
    if (sampleRate <= 0.0)
        return g;

    // 最上位の箱を見て moov だけ読む（mdat は大きいので読まずに飛ばす）
    in.setPosition (0);
    for (int guard = 0; guard < 64 && ! in.isExhausted(); ++guard)
    {
        juce::uint8 head[16];
        const auto start = in.getPosition();
        if (in.read (head, 8) != 8) break;

        juce::uint64 size = be32 (head);
        int64 header = 8;
        if (size == 1)
        {
            if (in.read (head + 8, 8) != 8) break;
            size = be64 (head + 8);
            header = 16;
        }
        else if (size == 0)
        {
            size = (juce::uint64) (in.getTotalLength() - start);
        }
        if (size < (juce::uint64) header) break;

        const auto type = juce::String::fromUTF8 ((const char*) head + 4, 4);
        if (type == "moov")
        {
            const auto bodySize = (size_t) (size - (juce::uint64) header);
            if (bodySize > 64u * 1024u * 1024u) break;   // 異常に大きい moov は読まない
            juce::HeapBlock<juce::uint8> body (bodySize);
            if (in.read (body.get(), (int) bodySize) != (int) bodySize) break;

            const auto moov = children (body.get(), bodySize);
            if (! fromEditList (moov, sampleRate, g))
                fromITunSMPB (moov, g);
            break;
        }
        if (! in.setPosition (start + (int64) size)) break;
    }
    return g;
}
} // namespace vb::audio
