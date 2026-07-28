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

#ifndef MCFX_FFTWPLANNER_H_INCLUDED
#define MCFX_FFTWPLANNER_H_INCLUDED

/** FFTW's planner is global mutable state and is **not** thread-safe: two
    threads creating plans at once corrupt it. Everything else in FFTW is fine
    concurrently — only planning needs the lock.

    That used to be handled by MtxConvMaster's constructor calling
    fftwf_make_planner_thread_safe(), which was enough while the convolution
    engine was the only thing in the suite that planned. It no longer is:
    juce::dsp::FFT plans too once it is built with its FFTW engine, and it does
    so from prepare()/prepareToPlay() on whichever thread got there first. A
    plugin whose analyser plans before any MtxConvMaster exists would have
    planned with no lock installed at all — which is the same shape as the
    startup crash the suite already hit once when another plug-in used FFTW.

    So the guard is installed here instead, from a static initialiser that runs
    at load time, before any plan can be created. Calling this function
    explicitly is belt-and-braces for the case where static initialisation order
    across translation units puts a planning call first; it is idempotent and
    costs an already-satisfied std::call_once after the first time. */
namespace mcfx
{
    void ensureFftwPlannerThreadSafe() noexcept;
}

#endif // MCFX_FFTWPLANNER_H_INCLUDED
