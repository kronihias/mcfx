#pragma once

// __APPLE__ is a compiler-defined macro available without any include — using
// it here lets this header stay free of JuceHeader.h, which would otherwise
// pull `juce::Point` into the global namespace and conflict with the legacy
// `Point` type that MacTypes.h imports inside the Cocoa-using .mm file.
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
