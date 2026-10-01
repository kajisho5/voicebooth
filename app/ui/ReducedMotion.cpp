#include "Animator.h"

/*  OS の「動きを減らす」設定（DESIGN 4.10）
    Mac：ReducedMotion.mm がこのファイルを Objective-C++ として取り込む（NSWorkspace を使うため） */

#if JUCE_MAC
 #import <AppKit/AppKit.h>
#elif JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace vb::motion
{
bool systemPrefersReducedMotion()
{
   #if JUCE_MAC
    // システム設定 → アクセシビリティ → ディスプレイ →「視差効果を減らす」
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion] == YES;
   #elif JUCE_WINDOWS
    // 設定 → アクセシビリティ → 視覚効果 →「アニメーション効果」がオフ
    BOOL animations = TRUE;
    if (SystemParametersInfoW (SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
    return false;
   #else
    return false;
   #endif
}
} // namespace vb::motion
