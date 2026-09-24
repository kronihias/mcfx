#include "MacAudioInputPermission.h"

#import <AVFoundation/AVFoundation.h>
#include <dispatch/dispatch.h>

namespace jsa
{
void requestAudioInputPermissionThen (std::function<void()> onDecided)
{
    if (@available (macOS 10.14, *))
    {
        if ([AVCaptureDevice authorizationStatusForMediaType: AVMediaTypeAudio]
                == AVAuthorizationStatusNotDetermined)
        {
            // The completion handler runs on an arbitrary queue; hop back to the
            // main queue, which is JUCE's message thread on macOS.
            auto callback = std::move (onDecided);
            [AVCaptureDevice requestAccessForMediaType: AVMediaTypeAudio
                                     completionHandler: ^(BOOL) {
                dispatch_async (dispatch_get_main_queue(), ^{ callback(); });
            }];
            return;
        }
    }

    onDecided();
}
} // namespace jsa
