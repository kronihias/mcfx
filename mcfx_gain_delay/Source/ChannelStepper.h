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

#ifndef __mcfx_gain_delay__ChannelStepper__
#define __mcfx_gain_delay__ChannelStepper__

#include "JuceHeader.h"

class ChannelStepper : public Timer
{
  
public:
  ChannelStepper(AudioProcessor* ownerFilter);
  ~ChannelStepper();
  
  void start(int interval);
  void stop();
  
  void setInterval(int interval);
  
  void timerCallback();
  
private:
  AudioProcessor* ownerFilter_;
  
  int channel_; // the currently active channel
  
};

#endif /* defined(__mcfx_gain_delay__ChannelStepper__) */
