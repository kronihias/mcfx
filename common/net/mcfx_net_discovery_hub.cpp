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

#include "mcfx_net_discovery_hub.h"

namespace mcfx { namespace net {

DiscoveryHub& DiscoveryHub::instance()
{
    // Single, process-wide singleton. Defined here (not inline in the
    // header) so all translation units that include the header reach the
    // *same* storage even when JUCE compiles plugins with
    // -fvisibility=hidden — defining a function-local static in an inline
    // header function gives each TU its own copy.
    static DiscoveryHub h;
    return h;
}

}} // namespace mcfx::net
