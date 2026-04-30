/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

// IMPORTANT: include Cocoa BEFORE InspectorClickBlocker.h so the legacy
// `Point` typedef from MacTypes.h doesn't collide with anything else, and so
// the Cocoa headers don't import JUCE's `juce::Point` from the global
// namespace (we deliberately avoid pulling JuceHeader.h into this file).
#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "InspectorClickBlocker.h"

// A transparent NSView that swallows every mouse/keyboard event that would
// otherwise reach the AU/VST3 native UI underneath it. Drops events at the
// NSWindow hit-test level — JUCE's setEnabled / setInterceptsMouseClicks
// don't reach native plugin UIs because their events flow
// NSWindow → NSView hitTest: directly to the native view.
@interface MCFXClickBlockerView : NSView
@end

@implementation MCFXClickBlockerView

- (NSView*) hitTest: (NSPoint) point
{
    // Returning self for any point in our frame makes us the topmost
    // hit-test target inside our bounds. Returning nil for out-of-bounds
    // points lets sibling views (none in our case, but safe) receive
    // their own events.
    if (NSPointInRect (point, [self frame]))
        return self;
    return nil;
}

- (BOOL) acceptsFirstResponder    { return NO; }
- (BOOL) acceptsFirstMouse: (NSEvent*) e { (void) e; return YES; }

// Mouse events — intentionally empty so they're swallowed.
- (void) mouseDown:        (NSEvent*) e { (void) e; }
- (void) mouseDragged:     (NSEvent*) e { (void) e; }
- (void) mouseUp:          (NSEvent*) e { (void) e; }
- (void) mouseMoved:       (NSEvent*) e { (void) e; }
- (void) mouseEntered:     (NSEvent*) e { (void) e; }
- (void) mouseExited:      (NSEvent*) e { (void) e; }
- (void) rightMouseDown:   (NSEvent*) e { (void) e; }
- (void) rightMouseDragged:(NSEvent*) e { (void) e; }
- (void) rightMouseUp:     (NSEvent*) e { (void) e; }
- (void) otherMouseDown:   (NSEvent*) e { (void) e; }
- (void) otherMouseDragged:(NSEvent*) e { (void) e; }
- (void) otherMouseUp:     (NSEvent*) e { (void) e; }
- (void) scrollWheel:      (NSEvent*) e { (void) e; }
- (void) magnifyWithEvent: (NSEvent*) e { (void) e; }
- (void) rotateWithEvent:  (NSEvent*) e { (void) e; }
- (void) swipeWithEvent:   (NSEvent*) e { (void) e; }

// Keyboard events. The AU's NSView is still in the responder chain, so
// keystrokes can theoretically reach it via Tab navigation; eating them
// here doesn't fully prevent that, but it's the right thing to do for
// any event that does end up routed to us.
- (void) keyDown:          (NSEvent*) e { (void) e; }
- (void) keyUp:            (NSEvent*) e { (void) e; }
- (void) flagsChanged:     (NSEvent*) e { (void) e; }
- (BOOL) performKeyEquivalent: (NSEvent*) e { (void) e; return NO; }

@end

namespace mcfx
{
    void* createInspectorClickBlockerNSView()
    {
        // Autoreleased — JUCE's NSViewAttachment retains the view in setView,
        // so the local reference can drop with the surrounding pool.
        return [[[MCFXClickBlockerView alloc] init] autorelease];
    }
}

#endif // __APPLE__
