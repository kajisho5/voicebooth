#include "system/SystemCheck.h"

namespace vb::system
{
class SystemCheckTests : public juce::UnitTest
{
public:
    SystemCheckTests() : juce::UnitTest ("SystemCheck", "VoiceBooth") {}

    void runTest() override
    {
        beginTest ("CPU names are shortened");
        {
            expectEquals (shortCpuName ("Intel(R) Core(TM) i5-8250U CPU @ 1.60GHz"), juce::String ("Intel Core i5-8250U"));
            expectEquals (shortCpuName ("AMD Ryzen 5 5600X 6-Core Processor"), juce::String ("AMD Ryzen 5 5600X 6-Core"));
            expectEquals (shortCpuName ("AMD Ryzen 7 7840U w/ Radeon 780M Graphics"), juce::String ("AMD Ryzen 7 7840U w/ Radeon 780M Graphics"));
            expectEquals (shortCpuName ("AMD Ryzen 5 4500U with Radeon Graphics"), juce::String ("AMD Ryzen 5 4500U"));
            expectEquals (shortCpuName ("Apple M2"), juce::String ("Apple M2"));
        }

        beginTest ("thresholds follow DESIGN 11.6.1");
        {
            Info rec { "macOS", "Apple M2", "arm64", 8, 16384, 200 * 1024 };
            expect (evaluate (rec).level == Level::ok);

            Info eightGb { "Windows 11", "Intel Core i5-8250U", "x86_64", 4, 7900, 50 * 1024 };   // 8 GB 機は 7.9 GB と出る
            const auto v = evaluate (eightGb);
            expect (v.level == Level::belowRecommended);
            expect (v.belowMinimum.isEmpty());
            expect (v.belowRecommended.contains (Item::cores) && v.belowRecommended.contains (Item::memory));
            expect (! v.belowRecommended.contains (Item::disk));

            Info old { "Windows 10", "Intel Core i3", "x86_64", 2, 4096, 1500 };
            const auto o = evaluate (old);
            expect (o.level == Level::belowMinimum);
            expectEquals (o.belowMinimum.size(), 3);

            Info unknownDisk { "Linux", "x", "x86_64", 8, 32768, -1 };
            expect (evaluate (unknownDisk).level == Level::ok);   // 分からない項目は判定しない
        }

        beginTest ("gather returns sane values on this machine");
        {
            const auto i = gather (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vb-not-created/x"));
            expect (i.osName.isNotEmpty());
            expect (i.physicalCores >= 1);
            expect (i.memoryMB > 256);
            expect (i.freeDiskMB >= 0);   // 無い場所は親をたどって調べる
            logMessage ("  this machine: " + i.osName + " / " + i.cpuModel + " / " + i.architecture + " / "
                        + juce::String (i.physicalCores) + " cores / " + juce::String (i.memoryMB) + " MB / free "
                        + juce::String (i.freeDiskMB) + " MB");
        }
    }
};

static SystemCheckTests systemCheckTests;
} // namespace vb::system
