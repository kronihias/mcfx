/*
 ==============================================================================
 
 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com
 
 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)
 
 Details of these licenses can be found at: www.gnu.org/licenses
 
 ambix is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 
 ==============================================================================
 */

#ifndef __mcfx_filter__FilterGraph__
#define __mcfx_filter__FilterGraph__

#include "JuceHeader.h"
#include "FilterInfo.h"
#include "PluginProcessor.h"

class AnalyzerComponent : public Component
{
public:
    AnalyzerComponent()
    {
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
    };
    
    ~AnalyzerComponent(){};
    
    void paint (Graphics& g)
    {
        Colour traceColour = Colour (0xffffff7f);
        g.setColour (traceColour);
        g.strokePath (path_in_mag_, PathStrokeType (0.5f));
        // g.fillPath(path_in_mag_);
        traceColour = Colour (0xefff3c7f);
        g.setColour (traceColour);
        g.strokePath (path_out_mag_, PathStrokeType (1.0f));
        
    };
    
    void resized(){};
    
    void setPath(Path* in_path, Path* out_path)
    {
        path_in_mag_ = *in_path;
        path_out_mag_ = *out_path;
        repaint();
    };
    
private:
    Path path_in_mag_, path_out_mag_;
    
};

class GraphComponent : public Component
{
public:
    GraphComponent()
    {
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(false, false);
    };
    
    ~GraphComponent(){};
    
    void paint (Graphics& g)
    {
        Colour traceColour = Colour (0xaa00ff00);
        g.setColour (traceColour);
        g.strokePath (path_mag_, PathStrokeType (2.0f));
    };
    void resized(){};
    
    void setPath(Path* newpath)
    {
        path_mag_ = *newpath;
    };
    
private:
    Path path_mag_;
    
};

class FilterGraph    :  public Component,
                        public SettableTooltipClient,
                        public ButtonListener,
                        public Timer
{
    friend class GraphComponent;
    friend class AnalyzerComponent;
    
public:
    FilterGraph (FilterInfo* filterinfo, AudioProcessor* processor);
    ~FilterGraph();
    
    void paint (Graphics& g);
    void resized();
    
    void setDragButton(int idx, float f, float g); // set a drag button to freq and gain
    
    void buttonClicked (Button* buttonThatWasClicked);
    
    // void mouseDrag	(const MouseEvent &event);
    void mouseWheelMove (const MouseEvent &event, const MouseWheelDetails &wheel);
    
    void changed(bool changed);

    void timerCallback();
    
    bool analyzeron_; // analyzer is on/off
    
    // Binary resources:
    static const char* drag_off_png;
    static const int drag_off_pngSize;
    static const char* drag_on_png;
    static const int drag_on_pngSize;
    static const char* drag_over_png;
    static const int drag_over_pngSize;
    
private:
    
    void calcPathMagPhase();
    
    void calcPathAnalyzer();
    
    int dbtoypos (float db_val);
    
    float ypostodb (int ypos);
    
    int hztoxpos (float hz_val);
    
    float xpostohz (int xpos);
    
    FilterInfo* filterinfo_;
    
    ScopedPointer<GraphComponent> graph_;
    
    ScopedPointer<AnalyzerComponent> analyzer_;
    
    OwnedArray<ImageButton> btn_drag;
    
    // ScopedPointer<ImageButton> btn_drag_lc;
    
    
    float minf_;        // minimum displayed frequency
    float maxf_;        // maximum displayed frequency
    float mindb_;       // min displayed dB
    float maxdb_;       // max displayed dB
    float grid_div;     // how many dB per divisions (between grid lines)
    
    float xmargin = 35.f; // space for y labels
    float ymargin = 12.f;
    
    Path path_grid, path_w_grid;
    
    Path path_in_mag_, path_out_mag_;
    
    Path path_mag_;
    
    bool changed_; // if values have changed -> redraw...
    // void mouseMove (const MouseEvent &event);
    
    AudioProcessor* myprocessor_; // this is used for changing parameters
    
    TooltipWindow tooltipWindow;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterGraph)
};

#endif /* defined(__mcfx_filter__FilterGraph__) */
