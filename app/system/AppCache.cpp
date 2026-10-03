#include "AppCache.h"

namespace vb::system
{
juce::File defaultCacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("VoiceBooth").getChildFile ("Cache");
}

juce::StringArray cacheSubfolders()
{
    // 増やした時はここにも足す（空にする・数える対象）
    return { "offvocal" };
}

juce::int64 cacheSize (const juce::File& folder)
{
    juce::int64 total = 0;
    for (auto& name : cacheSubfolders())
    {
        const auto dir = folder.getChildFile (name);
        if (! dir.isDirectory())
            continue;
        for (const auto& entry : juce::RangedDirectoryIterator (dir, true, "*", juce::File::findFiles))
            total += entry.getFileSize();
    }
    return total;
}

bool clearCache (const juce::File& folder)
{
    bool ok = true;
    for (auto& name : cacheSubfolders())
    {
        const auto dir = folder.getChildFile (name);
        if (dir.isDirectory() && ! dir.isSymbolicLink())
            ok = dir.deleteRecursively (false) && ok;   // リンクの先はたどらない（よそのファイルを消さない）
    }
    return ok;
}

juce::String formatSize (juce::int64 bytes)
{
    const double kb = 1024.0, mb = kb * 1024.0, gb = mb * 1024.0;
    const auto b = (double) juce::jmax ((juce::int64) 0, bytes);
    if (b >= gb)       return juce::String (b / gb, 1) + " GB";
    if (b >= 10 * mb)  return juce::String (juce::roundToInt (b / mb)) + " MB";
    if (b >= mb)       return juce::String (b / mb, 1) + " MB";
    return juce::String ((juce::int64) std::ceil (b / kb)) + " KB";   // 1 バイトでもあれば 1 KB（「0」と出して空と誤解させない）
}

juce::String displayPath (const juce::File& f)
{
    auto path = f.getFullPathName();
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
    if (home.isNotEmpty() && (path == home || path.startsWith (home + juce::File::getSeparatorString())))
        path = "~" + path.substring (home.length());
    return path;
}
} // namespace vb::system
