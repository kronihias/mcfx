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

#ifndef DETACHEDVIEWWINDOW_H_INCLUDED
#define DETACHEDVIEWWINDOW_H_INCLUDED

#include "JuceHeader.h"
#include "ViewStyle.h"

/** A meter view lifted out of the editor into its own window — for a second
    screen, a projector, or just more room than the host's plug-in window gives.

    The view is borrowed, not owned: the editor keeps owning it and takes it
    back when this closes. Uses the native title bar so fullscreen is the
    platform's own (its own Space on macOS, covering menu bar and dock), and
    so the window can also simply be left floating at any size. */
class DetachedViewWindow : public DocumentWindow
{
public:
    DetachedViewWindow (const String& title, Component& view, int minW, int minH)
        : DocumentWindow (title, Colour (0xff141414), DocumentWindow::allButtons, true)
    {
        // Not inherited from the editor out here.
        setLookAndFeel (&lnf_.get());
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (minW, minH, 16384, 16384);
        setContentNonOwned (&view, false);
        setWantsKeyboardFocus (true);
    }

    ~DetachedViewWindow() override
    {
        setLookAndFeel (nullptr);
    }

    /** Called when the window is closed by the user; the owner takes the view
        back and deletes this. Asynchronous, so the window is never deleted
        from inside its own callback. */
    std::function<void()> onClose;

    void showOn (const Displays::Display* display, bool fullscreen)
    {
        const auto area = display != nullptr ? display->userArea
                                             : Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;

        // A window starts out at a comfortable size on the chosen display, so
        // leaving fullscreen does not drop it back into the host's corner.
        setBounds (area.withSizeKeepingCentre (jmin (area.getWidth(), 1280),
                                               jmin (area.getHeight(), 800)));
        setVisible (true);
        toFront (true);

        if (fullscreen)
            setFullScreen (true);
    }

    void closeButtonPressed() override
    {
        requestClose();
    }

    bool keyPressed (const KeyPress& k) override
    {
        // Esc leaves fullscreen first, then closes — the usual expectation, and
        // the only way out when a projector is the only screen showing it.
        if (k == KeyPress::escapeKey)
        {
            if (isFullScreen()) setFullScreen (false);
            else                requestClose();
            return true;
        }
        if (k == KeyPress ('f', ModifierKeys::noModifiers, 0))
        {
            setFullScreen (! isFullScreen());
            return true;
        }
        return DocumentWindow::keyPressed (k);
    }

private:
    void requestClose()
    {
        Component::SafePointer<DetachedViewWindow> safe (this);
        MessageManager::callAsync ([safe]
        {
            if (safe != nullptr && safe->onClose)
                safe->onClose();
        });
    }

    SharedResourcePointer<ViewStyle::MeterLookAndFeel> lnf_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DetachedViewWindow)
};

#endif // DETACHEDVIEWWINDOW_H_INCLUDED
