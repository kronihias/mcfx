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

#include "FftwPlanner.h"

#if MCFX_HAS_FFTW3
 #include <fftw3.h>
 #include <mutex>
#endif

namespace mcfx
{

void ensureFftwPlannerThreadSafe() noexcept
{
#if MCFX_HAS_FFTW3
    // The planner lock is global to libfftw3f, which is shared by every plug-in
    // in the host process — so this has to be installed once per process, not
    // once per instance, and before the first plan is made by anyone.
    static std::once_flag once;
    std::call_once (once, [] { fftwf_make_planner_thread_safe(); });
#endif
}

#if MCFX_HAS_FFTW3
namespace
{
    // Runs at load time, ahead of any prepareToPlay() or analyser prepare(),
    // so the very first plan in the process is already covered. The explicit
    // calls elsewhere then only matter if some static initialiser in another
    // translation unit plans before this one runs.
    const struct PlannerGuardInstaller
    {
        PlannerGuardInstaller() noexcept { ensureFftwPlannerThreadSafe(); }
    } plannerGuardInstaller;
}
#endif

} // namespace mcfx
