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

#ifndef __mcfx_meter__MeterScale__
#define __mcfx_meter__MeterScale__

#include "JuceHeader.h"

class MeterScaleComponent  : public Component
{
public:
    MeterScaleComponent(int size, bool alignLeft);
    ~MeterScaleComponent();
    
    
    void paint (Graphics& g);
    void resized();
    void offset(int offset);
    
private:

    int _size;
    int _offset;
    OwnedArray<Label> _scale_labels;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterScaleComponent);
};

#endif /* defined(__mcfx_meter__MeterScale__) */
