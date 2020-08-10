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

#ifndef __mcfx_filter__FilterInfo__
#define __mcfx_filter__FilterInfo__

struct freqResponse {
    float magnitude;
    int phase;
};


class FilterInfo {

public:
    
    // pure virtual function
    virtual freqResponse getResponse(double f) = 0;
    
    virtual float inMagnitude(double f) = 0;
    virtual float outMagnitude(double f) = 0;
};


#endif /* defined(__mcfx_filter__FilterInfo__) */
