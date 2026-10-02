#include "SystemCheck.h"

namespace vb::system
{
juce::String shortCpuName (juce::String s)
{
    for (auto* noise : { "(R)", "(r)", "(TM)", "(tm)", " CPU", " Processor", " processor" })
        s = s.replace (noise, "");
    s = s.upToFirstOccurrenceOf ("@", false, false);
    // "with Radeon Graphics" などの後ろ書きを落とす
    s = s.upToFirstOccurrenceOf (" with ", false, true);
    while (s.contains ("  "))
        s = s.replace ("  ", " ");
    return s.trim();
}

Info gather (const juce::File& disk)
{
    Info i;
    i.osName = juce::SystemStats::getOperatingSystemName();
    i.cpuModel = shortCpuName (juce::SystemStats::getCpuModel());
    if (i.cpuModel.isEmpty())
        i.cpuModel = juce::SystemStats::getCpuVendor();
   #if JUCE_ARM
    i.architecture = "arm64";
   #else
    i.architecture = "x86_64";
   #endif
    i.physicalCores = juce::SystemStats::getNumPhysicalCpus();
    i.memoryMB = juce::SystemStats::getMemorySizeInMegabytes();

    auto dir = disk;
    while (dir != juce::File() && ! dir.exists())
        dir = dir.getParentDirectory();
    if (dir.exists())
        i.freeDiskMB = dir.getBytesFreeOnVolume() / (1024 * 1024);
    return i;
}

Verdict evaluate (const Info& i, const Requirements& r)
{
    Verdict v;
    auto check = [&] (Item item, juce::int64 value, juce::int64 min, juce::int64 rec)
    {
        if (value < 0)
            return;   // 分からないものは判定しない
        if (value < min)      v.belowMinimum.add (item);
        else if (value < rec) v.belowRecommended.add (item);
    };
    check (Item::cores, i.physicalCores, r.minCores, r.recCores);
    check (Item::memory, i.memoryMB, r.minMemoryMB, r.recMemoryMB);
    check (Item::disk, i.freeDiskMB, r.minFreeDiskMB, r.recFreeDiskMB);

    v.level = ! v.belowMinimum.isEmpty() ? Level::belowMinimum
            : ! v.belowRecommended.isEmpty() ? Level::belowRecommended
            : Level::ok;
    return v;
}
} // namespace vb::system
