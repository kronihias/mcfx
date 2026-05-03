#pragma once

// __APPLE__ / _WIN32 are compiler-defined macros available without any
// include — using them here lets this header stay free of JuceHeader.h,
// which would otherwise pull `juce::Point` into the global namespace and
// conflict with the legacy `Point` type that MacTypes.h imports inside the
// Cocoa-using .mm file. The Windows side avoids dragging <windows.h> into
// every translation unit that needs the blocker factory.
#ifdef __APPLE__

namespace mcfx
{
    // Returns an autoreleased NSView* (cast to void*) whose hitTest: returns
    // self for any point inside its bounds and whose mouse/keyboard handlers
    // are no-ops. Plug into a NSViewComponent and place it as a sibling
    // ABOVE the plugin editor's NSView to absorb every native event that
    // would otherwise bypass JUCE.
    void* createInspectorClickBlockerNSView();
}

#endif

#ifdef _WIN32

namespace mcfx
{
    // Disables Win32 input on every direct child HWND of `peerHwnd`,
    // recursing once is unnecessary because EnableWindow(FALSE) on a
    // parent HWND already drops mouse/keyboard messages for the entire
    // subtree. Painting continues unaffected, so the plugin's GUI stays
    // visible and animated. Call this after the plugin editor has been
    // added to a visible peer, so its native HWND is parented.
    //
    // We can't use a transparent overlay HWND like on macOS: child
    // layered windows on Windows do not reliably show lower-z-order
    // siblings through their alpha because the underlying HWND clips
    // sibling areas out of its drawing (WS_CLIPSIBLINGS is the default
    // for VST3 plugin HWNDs), so the overlay's region paints black.
    void disableNativeChildHWNDs (void* peerHwnd);
}

#endif
