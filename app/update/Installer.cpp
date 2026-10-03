#include "Installer.h"

#if JUCE_MAC
 #include <unistd.h>
#endif

namespace vb::update
{
juce::String windowsInstallerArguments()
{
    // 画面なし・確認の箱なし・再起動しない・動いている VoiceBooth を閉じる。/relaunch=1 で入れ終わったら起動し直す（VoiceBooth.iss）
    return "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /relaunch=1";
}

juce::String macSwapScript()
{
    // $1 = 待つ PID、$2 = DMG、$3 = 入れ替える .app。失敗したら元に戻して DMG を開く（手で入れられる）
    return R"SH(#!/bin/sh
pid="$1"; dmg="$2"; app="$3"
n=0
while kill -0 "$pid" 2>/dev/null; do
  sleep 0.5; n=$((n+1))
  [ "$n" -gt 120 ] && exit 1
done
fallback() { open "$dmg"; exit 1; }
mnt=$(mktemp -d "${TMPDIR:-/tmp}/vbupdate.XXXXXX") || fallback
hdiutil attach -nobrowse -readonly -noautoopen -mountpoint "$mnt" "$dmg" >/dev/null 2>&1 || fallback
new="$mnt/VoiceBooth.app"
if [ ! -d "$new" ]; then hdiutil detach "$mnt" -quiet; fallback; fi
staged="$app.updating"; old="$app.previous"
rm -rf "$staged" "$old"
if ! ditto "$new" "$staged"; then rm -rf "$staged"; hdiutil detach "$mnt" -quiet; fallback; fi
hdiutil detach "$mnt" -quiet
rmdir "$mnt" 2>/dev/null
if ! mv "$app" "$old"; then rm -rf "$staged"; fallback; fi
if ! mv "$staged" "$app"; then mv "$old" "$app"; rm -rf "$staged"; fallback; fi
rm -rf "$old"
xattr -dr com.apple.quarantine "$app" 2>/dev/null
rm -f "$dmg"
open "$app"
)SH";
}

bool macBundleReplaceable (const juce::File& bundle, juce::String* why)
{
    auto fail = [why] (const char* reason) { if (why != nullptr) *why = reason; return false; };
    const auto path = bundle.getFullPathName();
    if (! path.endsWith (".app") || ! bundle.isDirectory())
        return fail ("not running from an app bundle");
    if (path.startsWith ("/Volumes/"))
        return fail ("running from the disk image");
    if (path.contains ("/AppTranslocation/"))
        return fail ("macOS is running a translocated copy");
    if (! bundle.hasWriteAccess() || ! bundle.getParentDirectory().hasWriteAccess())
        return fail ("the app folder is not writable");
    return true;
}

bool canInstallInPlace (juce::String* why)
{
   #if JUCE_WINDOWS
    juce::ignoreUnused (why);
    return true;
   #elif JUCE_MAC
    return macBundleReplaceable (juce::File::getSpecialLocation (juce::File::currentApplicationFile), why);
   #else
    if (why != nullptr) *why = "no installer for this OS";
    return false;
   #endif
}

bool launchInstaller (const juce::File& installer, juce::String& error)
{
    if (! installer.existsAsFile())
    {
        error = "the installer is missing";
        return false;
    }
   #if JUCE_WINDOWS
    if (! installer.startAsProcess (windowsInstallerArguments()))
    {
        error = "couldn't start the installer";
        return false;
    }
    return true;
   #elif JUCE_MAC
    const auto bundle = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    if (! macBundleReplaceable (bundle, &error))
        return false;
    const auto script = installer.getSiblingFile ("swap.sh");
    if (! script.replaceWithText (macSwapScript(), false, false, "\n"))
    {
        error = "couldn't write the update script";
        return false;
    }
    // このアプリが終わっても動き続ける（親が消えても止まらない）。持っておくだけで、終わるのを待たない
    static std::unique_ptr<juce::ChildProcess> swapper;
    swapper = std::make_unique<juce::ChildProcess>();
    if (! swapper->start (juce::StringArray { "/bin/sh", script.getFullPathName(),
                                              juce::String (getpid()), installer.getFullPathName(), bundle.getFullPathName() }, 0))
    {
        error = "couldn't start the update script";
        return false;
    }
    return true;
   #else
    error = "no installer for this OS";
    return false;
   #endif
}
} // namespace vb::update
