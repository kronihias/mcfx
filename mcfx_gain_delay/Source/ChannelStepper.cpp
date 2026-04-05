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

#include "ChannelStepper.h"

#define PARAMS_PER_CH 6

// Channels the host actually provided. In the per-channel VST2 build this
// always equals NUM_CHANNELS; in the single multichannel build it is the
// negotiated count. Queried dynamically so the stepper wraps at the right
// boundary instead of stepping through empty (inactive) parameter slots.
static inline int stepperNumCh(AudioProcessor* ap)
{
    return juce::jlimit (1, (int) NUM_CHANNELS, ap->getTotalNumInputChannels());
}

ChannelStepper::ChannelStepper(AudioProcessor* ownerFilter)
{
  channel_ = 0;
  
  ownerFilter_ = ownerFilter;
  
}

ChannelStepper::~ChannelStepper()
{
  stopTimer();
}

void ChannelStepper::start(int interval)
{
  const int numCh = stepperNumCh(ownerFilter_);
  // reset wrap index so we don't point past the active channel range
  if (channel_ >= numCh) channel_ = 0;
  // set all channels to signal generator off
  for (int i=0; i<numCh; i++) {
    ownerFilter_->setParameter(i*PARAMS_PER_CH+5, 0.f);
    // should i use +notifyhost??
  }
  
  stopTimer();
  timerCallback();
  startTimer(interval);
}

void ChannelStepper::stop()
{
  stopTimer();

  const int numCh = stepperNumCh(ownerFilter_);
  // set the actual channel to signal off
  if (channel_ == 0) {
    ownerFilter_->setParameter((numCh-1)*PARAMS_PER_CH+5, 0.f);
  } else {
    ownerFilter_->setParameter((channel_-1)*PARAMS_PER_CH+5, 0.f);
  }
}

void ChannelStepper::setInterval(int interval)
{
  stopTimer();
  startTimer(interval);
}

void ChannelStepper::timerCallback()
{
  const int numCh = stepperNumCh(ownerFilter_);

  // keep channel_ in range if the host re-negotiated layout
  if (channel_ >= numCh) channel_ = 0;

  // first set the previous channel to signal off
  if (channel_ == 0) {
    ownerFilter_->setParameter((numCh-1)*PARAMS_PER_CH+5, 0.f);
  } else {
    ownerFilter_->setParameter((channel_-1)*PARAMS_PER_CH+5, 0.f);
  }

  // then set the actual channel to signal on
  ownerFilter_->setParameter(channel_*PARAMS_PER_CH+5, 1.f);

  channel_ = (channel_+1) % numCh;
}