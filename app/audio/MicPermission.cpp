#include "MicPermission.h"

#if JUCE_MAC
 #import <AVFoundation/AVFoundation.h>
#endif

namespace vb::audio
{
MicPermission checkMicPermission()
{
   #if JUCE_MAC
    switch ([AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeAudio])
    {
        case AVAuthorizationStatusAuthorized:    return MicPermission::granted;
        case AVAuthorizationStatusNotDetermined: return MicPermission::asking;
        case AVAuthorizationStatusDenied:
        case AVAuthorizationStatusRestricted:    return MicPermission::denied;
    }
    return MicPermission::granted;
   #else
    return MicPermission::notNeeded;
   #endif
}

void requestMicPermission (std::function<void (bool)> callback)
{
    auto cb = std::make_shared<std::function<void (bool)>> (std::move (callback));

   #if JUCE_MAC
    [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio
                             completionHandler: ^(BOOL granted)
                             {
                                 // 答えは任意のスレッドで来るので、メッセージスレッドへ渡す
                                 const bool ok = granted == YES;
                                 juce::MessageManager::callAsync ([cb, ok] { if (*cb) (*cb) (ok); });
                             }];
   #else
    juce::MessageManager::callAsync ([cb] { if (*cb) (*cb) (true); });
   #endif
}
} // namespace vb::audio
