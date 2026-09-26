/*
  ==============================================================================

   ConvolverSafeMode — whether MtxConvMaster should run in safe mode in the
   current host. Shared by mcfx_convolver and mcfx_mimoeq's long FIR bands so
   the two decide the same way.

   MtxConvMaster's minimum-latency mode reads a block's output in the same
   call that writes its input, which is only correct if every block is full
   (== the block size given to prepareToPlay). A shorter block reads output
   that hasn't been computed yet: gaps and misplaced audio. Safe mode reads one
   block later, which works with any block size and costs one block of
   latency (reported to the host).

   Hosts known to send irregular / incomplete blocks get safe mode. The
   MCFX_CONVOLVER_SAFEMODE environment variable overrides the detection
   ("1" = always safe, "0" = never): a workaround for an unlisted host, and
   how the tests exercise safe mode.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace mcfx
{
    inline bool convolverNeedsSafeMode()
    {
        const auto overrideValue = juce::SystemStats::getEnvironmentVariable ("MCFX_CONVOLVER_SAFEMODE", {});
        if (overrideValue == "1") return true;
        if (overrideValue == "0") return false;

        juce::PluginHostType host;
        return host.isAdobeAudition() || host.isPremiere() || host.isSteinberg();   // probably an incomplete list
    }
}
