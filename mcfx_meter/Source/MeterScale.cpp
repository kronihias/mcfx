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

#include "MeterScale.h"

extern float iec_scale(float dB);

MeterScaleComponent::MeterScaleComponent (int size, bool alignLeft): _offset(0)
{
    _size = size;
    
    // create scale
    for (int i=0; i<7; i++)
    {
        if (Label* const LABEL = new Label ("new label", String (-i*10)))
        {
            _scale_labels.add(LABEL);
            addChildComponent(_scale_labels.getUnchecked(i));
            if (i != 5)
                _scale_labels.getUnchecked(i)->setVisible(true); // don't show -50dB
            _scale_labels.getUnchecked(i)->setFont (Font (11.0000f, Font::plain));
            _scale_labels.getUnchecked(i)->setColour (Label::textColourId, Colours::white);
            if (alignLeft)
                _scale_labels.getUnchecked(i)->setJustificationType (Justification::topLeft);
            else
                _scale_labels.getUnchecked(i)->setJustificationType (Justification::topRight);
        }
    }
}

MeterScaleComponent::~MeterScaleComponent ()
{
    
}

void MeterScaleComponent::paint (Graphics& g)
{
    
}

void MeterScaleComponent::resized()
{
    for (int i=0; i<7; i++)
    {
        _scale_labels.getUnchecked(i)->setBounds(0, _size-iec_scale(-10*i)*_size, 30, getWidth());
    }
}

void MeterScaleComponent::offset(int offset)
{
    _offset = offset;
    
    for (int i=0; i<7; i++)
    {
        _scale_labels.getUnchecked(i)->setText(String (-i*10 + _offset), dontSendNotification);
    }
}

