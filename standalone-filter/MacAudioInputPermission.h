#pragma once

#include <functional>

namespace jsa
{
/** Makes sure the macOS audio-input (microphone) permission has been decided
    before the standalone opens any audio device, then calls onDecided on the
    message thread.

    JUCE has no RuntimePermissions support for this on macOS (isRequired() is
    always false), so the first device that opens with inputs triggers the
    system prompt. At startup the host opens the device several times: once in
    StandalonePluginHolder, again in reopenDeviceManagerForMaxChannels(), again
    for the symmetric-I/O setup. Each of those opens happens while the question
    is still undecided, so each one can put up its own prompt. Asking once, up
    front, and building the window only after the answer means there is exactly
    one prompt.

    If the status is already decided (granted or denied), or the OS is older
    than 10.14 (no input permission), onDecided runs straight away. */
void requestAudioInputPermissionThen (std::function<void()> onDecided);
} // namespace jsa
