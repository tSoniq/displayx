/** @file   DisplayXFBTiming.h
 *  @brief  Class used to provide timing services in the kernel.
 *
 *  Copyright (c) 2011 tSoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBTiming_H
#define COM_TSONIQ_DisplayXFBTiming_H   (1)

#include <stdint.h>
#include "DisplayXFBNames.h"


/** Class used to manage timing services.
 */
class DisplayXFBTiming
{
public:

    DisplayXFBTiming() : m_period(0), m_nextTick(0) { }

    void start(uint32_t period);
    void update(uint64_t& ticks, uint32_t& timeToNextTick);

private:

    uint64_t m_period;                      //! Tick period, in abstime units
    uint64_t m_nextTick;                    //! Time for next tick, in abstime units
};

#endif      // COM_TSONIQ_DisplayXFBTiming_H
