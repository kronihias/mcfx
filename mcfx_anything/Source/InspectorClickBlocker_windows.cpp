/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#ifdef _WIN32

// Stay free of JuceHeader.h here so we don't pull juce::Point into the
// global namespace alongside the Win32 POINT typedef.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "InspectorClickBlocker.h"

namespace mcfx
{
    void disableNativeChildHWNDs (void* peerHwndVoid)
    {
        HWND parent = (HWND) peerHwndVoid;

        if (parent == nullptr)
            return;

        // EnableWindow(FALSE) on a window stops it from receiving mouse
        // and keyboard input; per Win32 semantics, descendants of a
        // disabled window also stop receiving input, so a single call on
        // each direct child of the JUCE peer is enough.
        //
        // Iterate the top-level children only — GetWindow with GW_CHILD
        // returns the first child, GW_HWNDNEXT walks siblings. (We avoid
        // EnumChildWindows because it recurses into grandchildren, which
        // we'd then have to filter back to the top level.)
        HWND child = GetWindow (parent, GW_CHILD);

        while (child != nullptr)
        {
            EnableWindow (child, FALSE);
            child = GetWindow (child, GW_HWNDNEXT);
        }
    }
}

#endif // _WIN32
