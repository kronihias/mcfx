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

#ifndef EQPHASEGRAPH_H_INCLUDED
#define EQPHASEGRAPH_H_INCLUDED

#include "JuceHeader.h"
#include "EqChain.h"

/** Phase-response viewer — companion to EqGraph. Reads from the same EqChain
    and draws the wrapped phase of the combined response in degrees, on the
    same log-frequency grid as the magnitude graph. No band handles or
    interactive editing — purely a display widget. */
class EqPhaseGraph : public Component, public Timer
{
public:
    EqPhaseGraph();
    ~EqPhaseGraph() override;

    void setChain (EqChain* chain) { chain_ = chain; }

    void paint    (Graphics& g) override;
    void resized()              override;
    void timerCallback()        override;

    void mouseMove (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;

private:
    int   hzToXpos    (float hz)  const;
    int   degToYpos   (float deg) const;
    void  rebuildGrid();
    void  drawGrid    (Graphics& g);
    void  calcPath();

    EqChain* chain_ = nullptr;

    float minf_     =  20.f;
    float maxf_     = 20000.f;
    float minDeg_   = -180.f;
    float maxDeg_   =  180.f;
    float degStep_  =  90.f;
    float xmargin_  = 35.f;
    float ymargin_  = 12.f;

    Path pathPhase_;
    Path pathGrid_;
    Path pathGridW_;

    int hoverX_ = -1;
    float xposToHz (int xpos) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqPhaseGraph)
};

#endif // EQPHASEGRAPH_H_INCLUDED
